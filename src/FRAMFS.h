#pragma once
#include "FS.h"
#include "SPI.h"

namespace fs
{

// FRAMFS (FRAM File System)
class FRAMFS : public FS
{
public:
    FRAMFS(int size);
    bool begin(
            uint8_t ssPin=SS,
            SPIClass &spi=SPI, 
            uint32_t frequency=4000000,
            const char * mountpoint="/fram",
            uint8_t max_files=10,
            bool format_if_empty=false,
            bool force_to_format=false);
    bool format(
            uint8_t ssPin=SS,
            SPIClass &spi=SPI, 
            uint32_t frequency=4000000,
            const char * mountpoint="/fram",
            uint8_t max_files=10);
    bool beginOrFormat(
            uint8_t ssPin=SS,
            SPIClass &spi=SPI, 
            uint32_t frequency=4000000,
            const char * mountpoint="/fram",
            uint8_t max_files=10);
    void end();
    
    bool isUnformatted();
    
    uint32_t totalBytes();
    uint32_t usedBytes();
    
    bool readRAW (uint8_t* buffer, uint32_t sector);
    bool writeRAW(uint8_t* buffer, uint32_t sector);
    
    // just for debug
    bool unformat(
            uint8_t ssPin=SS,
            SPIClass &spi=SPI, 
            uint32_t frequency=4000000);
    
private:
    const int _sz_ramdisk;  // size of FRAM device [kB]
    uint8_t _pdrv;          // drive number (0,1,...)
    bool _unformatted;      // has no file system
};

}

using namespace fs;
