# FRAM File System for ESP32 Arduino

* File systems for FRAM devices with single SPI I/O.
* Just for ESP32 Arduino.
* FRAMFS class has similar methods to SD class and SPIFFS class.

## Common Methods
see [the reference of SD library](https://www.arduino.cc/reference/en/libraries/sd/).

## Special Methods

### FRAMFS(size)
constructor.
* size: size of FRAM device [kB]

### begin(ssPin, spi, frequency, mountpoint, max_files, format_if_empty, force_to_format)
begin the FRAM drive.
* ssPin: pin number of SS (Slave Select), default = SS (4)
* spi: SPI bus, default = SPI
* frequency: SPI clock frequency [Hz], default = 4000000
* mountpoint: mount point path, default = "/fram"
* max_files: max file number, default = 10
* format_if_empty: format if no file system? defaut = false
* force_to_format: force to format? default = false
* return: result (true/false)

### isUnformatted()
Is the FRAM drive unformatted? (check after begin() returned false)
* return: has no file system? (true/false)

### format(ssPin, spi, frequency, mountpoint, max_files)
format the FRAM drive.

parameters and return are the same as begin()'s.

it's equivalent to begin(ssPin, spi, frequency, mountpoint, max_files, false, true).

### beginOrFormat(ssPin, spi, frequency, mountpoint, max_files)
begin the FRAM drive, or format it.

parameters and return are the same as begin()'s.

it's equivalent to begin(ssPin, spi, frequency, mountpoint, max_files, true, false).

### end()
end the FRAM drive.

### totalBytes()
get total bytes of the FRAM drive
* return: total bytes

### usedBytes()
get used bytes of the FRAM drive
* return: used bytes

### readRAW(buffer, sector)
read a RAW sector
* buffer: buffer to read data
* sector: sector number

### writeRAW(buffer, sector)
write a RAW sector
* buffer: data to be written
* sector: sector number
