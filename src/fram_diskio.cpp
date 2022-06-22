// functions called from FRAMFS class
// (glue logic between FRAMFS, FatFs and low level driver)

#include "fram_diskio.h"
#include "esp_system.h"
extern "C" {
    #include "ff.h"
    #include "diskio.h"
#if ESP_IDF_VERSION_MAJOR > 3
    #include "diskio_impl.h"
#endif
    #include "esp_vfs_fat.h"
}

// FRAM OP-CODE
#define OP_WREN     0x06    // Set Write Enable Latch
#define OP_WRDI     0x04    // Reset Write Enable Latch
#define OP_RDSR     0x05    // Read Status Register
#define OP_WRSR     0x01    // Write Status Register
#define OP_READ     0x03    // Read Memory Code
#define OP_WRITE    0x02    // Write Memory Code
#define OP_RDID     0x9F    // Read Device ID
#define OP_FSTRD    0x0B    // Fast Read Memory Code
#define OP_SLEEP    0xB9    // Fast Read Memory Code

// Constants
#define MAX_DRIVES FF_VOLUMES  // Max number of FRAM devices 
#define SZ_BLOCK    1       // Block size (for GET_BLOCK_SIZE command, 1 for Non-Flash devices)
#define SS_RAMDISK  512     // sector size [byte]

// FRAM device infomation
typedef struct {
    uint8_t     ssPin;
    SPIClass    *spi;
    int         frequency;
    char        * base_path;
    
    DSTATUS     status;
    WORD        sz_sector;
    DWORD       n_sectors;
    DWORD       size;
} fram_info_t;

static fram_info_t* s_frams[MAX_DRIVES] = { NULL };

// Error messages
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_ERROR
const char * fferr2str[] = {
    "(0) Succeeded",
    "(1) A hard error occurred in the low level disk I/O layer",
    "(2) Assertion failed",
    "(3) The physical drive cannot work",
    "(4) Could not find the file",
    "(5) Could not find the path",
    "(6) The path name format is invalid",
    "(7) Access denied due to prohibited access or directory full",
    "(8) Access denied due to prohibited access",
    "(9) The file/directory object is invalid",
    "(10) The physical drive is write protected",
    "(11) The logical drive number is invalid",
    "(12) The volume has no work area",
    "(13) There is no valid FAT volume",
    "(14) The f_mkfs() aborted due to any problem",
    "(15) Could not get a grant to access the volume within defined period",
    "(16) The operation is rejected according to the file sharing policy",
    "(17) LFN working buffer could not be allocated",
    "(18) Number of open files > FF_FS_LOCK",
    "(19) Given parameter is invalid"
};
#endif

/*--------------------------------------------------------------------------
   private class for acquiring SPI bus lock
---------------------------------------------------------------------------*/
namespace
{
struct AcquireSPI
{
    fram_info_t *fram;
    AcquireSPI(fram_info_t* fram) : fram(fram)
    {
        fram->spi->beginTransaction(SPISettings(fram->frequency, MSBFIRST, SPI_MODE0));
    }

    ~AcquireSPI()
    {
        fram->spi->endTransaction();
    }
};
}

/*--------------------------------------------------------------------------
   FatFs low level driver functions (called fram FatFs)
---------------------------------------------------------------------------*/

// Initialize Disk Drive
// pdrv: drive number (0,1,...)
// return: status (STA_NOINIT | STA_NODISK | STA_PROTECT)
DSTATUS ff_fram_initialize(BYTE pdrv)
{
    DSTATUS sta;
    
    log_d("ff_fram_initialize %d\n", pdrv);

    if (pdrv >= MAX_DRIVES || s_frams[pdrv] == NULL){
        sta = STA_NOINIT;
    } else {
        fram_info_t *fram = s_frams[pdrv];
        fram->status    = 0;
        sta = fram->status;
    }

    return sta;
}

// Get Disk Status
// pdrv: drive number (0,1,...)
// return: status (STA_NOINIT | STA_NODISK | STA_PROTECT)
DSTATUS ff_fram_status(BYTE pdrv)
{
    DSTATUS sta;

    if (pdrv >= MAX_DRIVES || s_frams[pdrv] == NULL) {
        sta = STA_NOINIT;
    } else {
        sta = s_frams[pdrv]->status;
    }

    return sta;
}

// Read Sectors
// pdrv: drive number (0,1,...)
// buffer: data buffer to store read data
// sector: start sector number
// count: number of sectors to read
// return: result (RES_OK / RES_NOTRDY / RES_ERROR)
DRESULT ff_fram_read(BYTE pdrv, BYTE* buffer, DWORD sector, UINT count)
{
    if (pdrv >= MAX_DRIVES || s_frams[pdrv] == NULL){
        return RES_NOTRDY;
    }
    fram_info_t * fram = s_frams[pdrv];
    if (fram->status & STA_NOINIT) {
        return RES_NOTRDY;
    }
    
    uint32_t nc  = (uint32_t)count  * (uint32_t)fram->sz_sector;
    uint32_t ofs =           sector * (uint32_t)fram->sz_sector;
    if (ofs + nc >= fram->size * 1024) {
        return RES_ERROR;
    }
    
    AcquireSPI lock(fram);
    
    digitalWrite(fram->ssPin, LOW);
    uint8_t cmd_buff[4];
    cmd_buff[0] = OP_READ;
    cmd_buff[1] = (uint8_t)((ofs >> 16) & 0xFF);
    cmd_buff[2] = (uint8_t)((ofs >>  8) & 0xFF);
    cmd_buff[3] = (uint8_t)( ofs        & 0xFF);
    fram->spi->writeBytes(cmd_buff, 4);
    fram->spi->transferBytes(NULL, buffer, nc);
    digitalWrite(fram->ssPin, HIGH);
    
    return RES_OK;
}

// Write Sectors
// pdrv: drive number (0,1,...)
// buffer: data buffer to be written
// sector: start sector number
// count: number of sectors to write
// return: result (RES_OK / RES_NOTRDY / RES_WRPRT / RES_ERROR)
DRESULT ff_fram_write(BYTE pdrv, const BYTE* buffer, DWORD sector, UINT count)
{
    if (pdrv >= MAX_DRIVES || s_frams[pdrv] == NULL){
        return RES_NOTRDY;
    }
    fram_info_t * fram = s_frams[pdrv];
    if (fram->status & STA_NOINIT) {
        return RES_NOTRDY;
    }
    if (fram->status & STA_PROTECT) {
        return RES_WRPRT;
    }
    
    uint32_t nc  = (uint32_t)count  * (uint32_t)fram->sz_sector;
    uint32_t ofs =           sector * (uint32_t)fram->sz_sector;
    if (ofs + nc >= fram->size * 1024) {
        return RES_ERROR;
    }
    
    AcquireSPI lock(fram);
    
    digitalWrite(fram->ssPin, LOW);
    fram->spi->write(OP_WREN);
    digitalWrite(fram->ssPin, HIGH);
    
    digitalWrite(fram->ssPin, LOW);
    uint8_t cmd_buff[4];
    cmd_buff[0] = OP_WRITE;
    cmd_buff[1] = (uint8_t)((ofs >> 16) & 0xFF);
    cmd_buff[2] = (uint8_t)((ofs >>  8) & 0xFF);
    cmd_buff[3] = (uint8_t)( ofs        & 0xFF);
    fram->spi->writeBytes(cmd_buff, 4);
    fram->spi->writeBytes(buffer, nc);
    digitalWrite(fram->ssPin, HIGH);
    
    digitalWrite(fram->ssPin, LOW);
    fram->spi->write(OP_WRDI);
    digitalWrite(fram->ssPin, HIGH);
    
    return RES_OK;
}

// I/O Control
// pdrv: drive number (0,1,...)
// cmd: control code
// buff: buffer to send/receive data
// return: result (RES_OK / RES_NOTRDY / RES_PARERR)
DRESULT ff_fram_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
    if (pdrv >= MAX_DRIVES || s_frams[pdrv] == NULL){
        return RES_NOTRDY;
    }
    fram_info_t * fram = s_frams[pdrv];
    if (fram->status & STA_NOINIT) {
        return RES_NOTRDY;
    }
    
    switch(cmd) {
    // Synchronize
    case CTRL_SYNC:
        ; /* Nothing to do */
        return RES_OK;
    // Get number of sectors on the drive
    case GET_SECTOR_COUNT:
        *((DWORD*) buff) = fram->n_sectors;
        return RES_OK;
    // Get size of sector for generic read/write
    case GET_SECTOR_SIZE:
        *((WORD*) buff) = fram->sz_sector;
        log_d("GET_SECTOR_SIZE %d", *((WORD*) buff));
        return RES_OK;
    // Get internal block size in unit of sector
    case GET_BLOCK_SIZE:
        *((uint32_t*)buff) = SZ_BLOCK;
        return RES_OK;
    }
    return RES_PARERR;
}

/*--------------------------------------------------------------------------
   functions called fram FRAMFS class
---------------------------------------------------------------------------*/

// read one sector
// pdrv: drive number (0,1,...)
// buffer: data buffer to store read data
// sector: start sector number
// return: reuslt
bool fram_read_raw(uint8_t pdrv, uint8_t* buffer, uint32_t sector)
{
    return ff_fram_read(pdrv, buffer, sector, 1) == RES_OK;
}

// write one sector
// pdrv: drive number (0,1,...)
// buffer: data buffer to be written
// sector: start sector number
// return: reuslt
bool fram_write_raw(uint8_t pdrv, uint8_t* buffer, uint32_t sector)
{
    return ff_fram_write(pdrv, buffer, sector, 1) == RES_OK;
}

// uninitialize
// pdrv: drive number (0,1,...)
// return: result
uint8_t fram_uninit(uint8_t pdrv)
{
    if (pdrv >= MAX_DRIVES || s_frams[pdrv] == NULL){
        return ESP_FAIL;
    }
    fram_info_t * fram = s_frams[pdrv];
    
    ff_diskio_register(pdrv, NULL);
    
    s_frams[pdrv] = NULL;
    esp_err_t err = ESP_OK;
    if (fram->base_path) {
        err = esp_vfs_fat_unregister_path(fram->base_path);
        free(fram->base_path);
    }
    free(fram);
    return err;
}

// initialize
// ssPin: pin number of SS (Slave Select)
// spi: SPI bus
// hz: SPI clock frequency [Hz]
// size: size of FRAM device [kB]
// return: drive number
uint8_t fram_init(uint8_t ssPin, SPIClass * spi, int hz, int size)
{
    uint8_t pdrv = 0xFF;
    if (ff_diskio_get_drive(&pdrv) != ESP_OK || pdrv == 0xFF) {
        return 0xFF;
    }

    fram_info_t * fram = (fram_info_t *)malloc(sizeof(fram_info_t));
    if (!fram) {
        return 0xFF;
    }

    fram->base_path = NULL;
    fram->frequency = hz;
    fram->spi = spi;
    fram->ssPin = ssPin;
    fram->status = STA_NOINIT;
    fram->sz_sector = SS_RAMDISK;
    fram->n_sectors = size * 1024 / SS_RAMDISK;
    fram->size = size;

    pinMode(fram->ssPin, OUTPUT);
    digitalWrite(fram->ssPin, HIGH);

    s_frams[pdrv] = fram;
    
    // register low level drivers for FatFs
    static const ff_diskio_impl_t sd_impl = {
        .init   = &ff_fram_initialize,
        .status = &ff_fram_status,
        .read   = &ff_fram_read,
        .write  = &ff_fram_write,
        .ioctl  = &ff_fram_ioctl
    };
    ff_diskio_register(pdrv, &sd_impl);

    return pdrv;
}

// unmount
// pdrv: drive number (0,1,...)
// return: reuslt
uint8_t fram_unmount(uint8_t pdrv)
{
    if (pdrv >= MAX_DRIVES || s_frams[pdrv] == NULL){
        return ESP_FAIL;
    }
    fram_info_t * fram = s_frams[pdrv];
    
    fram->status |= STA_NOINIT;
    
    char drv[3] = {(char)('0' + pdrv), ':', 0};
    f_mount(NULL, drv, 0);
    return ESP_OK;
}

// mount
// pdrv: drive number (0,1,...)
// path: base path of the FRAM drive
// max_files: max file number
// format_if_empty: format if no file system?
// force_to_format: force to format?
// return: reuslt
uint8_t fram_mount(uint8_t pdrv, const char* path, uint8_t max_files, bool format_if_empty, bool force_to_format)
{
    if (pdrv >= MAX_DRIVES || s_frams[pdrv] == NULL){
        return ESP_FAIL;
    }
    fram_info_t * fram = s_frams[pdrv];
    
    if(fram->base_path){
        free(fram->base_path);
    }
    fram->base_path = strdup(path);

    FATFS* fs;
    char drv[3] = {(char)('0' + pdrv), ':', 0};
    esp_err_t err = esp_vfs_fat_register(path, drv, max_files, &fs);
    if (err != ESP_OK) {
        log_e("esp_vfs_fat_register failed 0x(%x)", err);
        return ESP_FAIL;
    }
    
    bool need_to_format = false;
    if(force_to_format){
        need_to_format = true;
    }else{
        FRESULT res = f_mount(fs, drv, 1);
        if (res != FR_OK) {
            log_e("f_mount failed: %s", fferr2str[res]);
            if(res == FR_NO_FILESYSTEM){
                if(format_if_empty){
                    need_to_format = true;
                }else{
                    return FR_NO_FILESYSTEM;
                }
            } else {
                esp_vfs_fat_unregister_path(path);
                return ESP_FAIL;
            }
        }
    }
    if(need_to_format){
        int len = sizeof(BYTE) * FF_MAX_SS;
        BYTE* work = (BYTE*) malloc(len);
        if (!work) {
          log_e("alloc for f_mkfs failed");
          return ESP_FAIL;
        }
        FRESULT res = f_mkfs(drv, FM_ANY, 0, work, len);
        free(work);
        if (res != FR_OK) {
            log_e("f_mkfs failed: %s", fferr2str[res]);
            esp_vfs_fat_unregister_path(path);
            return ESP_FAIL;
        }
        res = f_mount(fs, drv, 1);
        if (res != FR_OK) {
            log_e("f_mount failed: %s", fferr2str[res]);
            esp_vfs_fat_unregister_path(path);
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

//********** just for debug **********

// unformat the FRAM drive
// ssPin: pin number of SS (Slave Select)
// spi: SPI bus
// return: result
bool fram_unformat(uint8_t ssPin, SPIClass * spi, int hz)
{
    spi->beginTransaction(SPISettings(hz, MSBFIRST, SPI_MODE0));
    
    digitalWrite(ssPin, LOW);
    spi->write(OP_WREN);
    digitalWrite(ssPin, HIGH);
    
    // erase the first 512 bytes
    digitalWrite(ssPin, LOW);
    uint8_t cmd_buff[4];
    cmd_buff[0] = OP_WRITE;
    cmd_buff[1] = 0;
    cmd_buff[2] = 0;
    cmd_buff[3] = 0;
    spi->writeBytes(cmd_buff, 4);
    for(int i=0;i<512;i++){
        spi->write(0xFF);
    }
    digitalWrite(ssPin, HIGH);
    
    digitalWrite(ssPin, LOW);
    spi->write(OP_WRDI);
    digitalWrite(ssPin, HIGH);
    
    spi->endTransaction();
}
