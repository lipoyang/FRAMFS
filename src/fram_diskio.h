#pragma once

// functions called from FRAMFS class
// (glue logic between FRAMFS, FatFs and low level driver)

#include "Arduino.h"
#include "SPI.h"

uint8_t fram_init(uint8_t ssPin, SPIClass * spi, int hz, int size);
uint8_t fram_uninit(uint8_t pdrv);

uint8_t fram_mount  (uint8_t pdrv, const char* path, uint8_t max_files, bool format_if_empty, bool force_to_format);
uint8_t fram_unmount(uint8_t pdrv);

bool fram_read_raw (uint8_t pdrv, uint8_t* buffer, uint32_t sector);
bool fram_write_raw(uint8_t pdrv, uint8_t* buffer, uint32_t sector);

// just for debug
bool fram_unformat(uint8_t ssPin, SPIClass * spi, int hz);
