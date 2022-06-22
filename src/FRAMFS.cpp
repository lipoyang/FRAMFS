// FRAMFS (FRAM File System)

#include "vfs_api.h"
#include "fram_diskio.h"
#include "ff.h"
#include "FRAMFS.h"

using namespace fs;

// constructor
// size: size of FRAM device [kB]
FRAMFS::FRAMFS(int size): FS(FSImplPtr(new VFSImpl())),
    _pdrv(0xFF), _sz_ramdisk(size), _unformatted(false)
{
    ;
}

// begin the FRAM drive
// ssPin: pin number of SS (Slave Select)
// spi: SPI bus
// frequency: SPI clock frequency [Hz]
// mountpoint: mount point path
// max_files: max file number
// format_if_empty: format if no file system?
// force_to_format: force to format?
// return: result
bool FRAMFS::begin(
        uint8_t ssPin, 
        SPIClass &spi, 
        uint32_t frequency, 
        const char * mountpoint, 
        uint8_t max_files,
        bool format_if_empty,
        bool force_to_format
    )
{
    _unformatted = false;
    
    if(_pdrv != 0xFF) {
        return true;
    }
    
    spi.begin();
    
    _pdrv = fram_init(ssPin, &spi, frequency, _sz_ramdisk);
    if(_pdrv == 0xFF) {
        log_e("ERROR: fram_init\n");
        return false;
    }
    
    uint8_t ret = fram_mount(_pdrv, mountpoint, max_files, format_if_empty, force_to_format);
    if(ret != ESP_OK){
        fram_unmount(_pdrv);
        fram_uninit(_pdrv);
        _pdrv = 0xFF;
        if(ret == FR_NO_FILESYSTEM){
            _unformatted = true;
        }
        log_e("ERROR: fram_mount\n");
        return false;
    }
    
    _impl->mountpoint(mountpoint);
    return true;
}

// format the FRAM drive
// ssPin: pin number of SS (Slave Select)
// spi: SPI bus
// frequency: SPI clock frequency [Hz]
// mountpoint: mount point path
// max_files: max file number
// return: result
bool FRAMFS::format(
        uint8_t ssPin,
        SPIClass &spi, 
        uint32_t frequency,
        const char * mountpoint,
        uint8_t max_files
    )
{
    return this->begin(ssPin, spi, frequency, mountpoint, max_files, false, true);
}

// end the FRAM drive
void FRAMFS::end()
{
    if(_pdrv != 0xFF) {
        _impl->mountpoint(NULL);
        fram_unmount(_pdrv);
        fram_uninit(_pdrv);
        _pdrv = 0xFF;
    }
}

// is unformatted?
// return: has no file system?
bool FRAMFS::isUnformatted()
{
    return _unformatted;
}

// get total bytes of the FRAM drive
// return: total bytes
uint32_t FRAMFS::totalBytes()
{
    char path[3] = "0:";
    path[0] = '0' + _pdrv;
    
    FATFS* fsinfo;
    DWORD fre_clust;
    if(f_getfree(path, &fre_clust, &fsinfo)!= 0) return 0;
    
    uint32_t size = ((uint32_t)(fsinfo->csize))*(fsinfo->n_fatent - 2)
// TODO
#if _MAX_SS != 512
        *((uint32_t)(fsinfo->ssize));
#else
        *512;
#endif
    return size;
}

// get used bytes of the FRAM drive
// return: used bytes
uint32_t FRAMFS::usedBytes()
{
    char path[3] = "0:";
    path[0] = '0' + _pdrv;
    
    FATFS* fsinfo;
    DWORD fre_clust;
    if(f_getfree(path, &fre_clust, &fsinfo)!= 0) return 0;
    
    uint32_t size = ((uint32_t)(fsinfo->csize))*((fsinfo->n_fatent - 2) - (fsinfo->free_clst))
// TODO
#if _MAX_SS != 512
        *((uint32_t)(fsinfo->ssize));
#else
        *512;
#endif
    return size;
}

// read a RAW sector
// buffer: buffer to read data
// sector: sector number
bool FRAMFS::readRAW(uint8_t* buffer, uint32_t sector)
{
    return fram_read_raw(_pdrv, buffer, sector);
}

// write a RAW sector
// buffer: data to be written
// sector: sector number
bool FRAMFS::writeRAW(uint8_t* buffer, uint32_t sector)
{
    return fram_write_raw(_pdrv, buffer, sector);
}

//********** just for debug **********

// unformat the FRAM drive
// ssPin: pin number of SS (Slave Select)
// spi: SPI bus
// return: result
bool FRAMFS::unformat(
        uint8_t ssPin,
        SPIClass &spi,
        uint32_t frequency)
{
    return fram_unformat(ssPin, &spi, frequency);
}
