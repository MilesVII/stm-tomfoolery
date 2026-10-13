# STM32F411
Pure CMSIS drivers, no HAL

C13-14-15 | A0-A7 | B0-1-2 B10
B9-B3 | A15 A12-A8 | B15-B12

# pins
## unusable (blackpill board)
`A11`, `A12`, `B2`
avoid: `A9`, `A10`

## 25Q64JVSIQ
8MiB flash chip

```
soldered pins connections
3V 3V A5 A7
A4 A6 3V GND
```

```
SPI1 AF5:
A4 NSS
A5 SCK
A6 MISO
A7 MOSI
```

# flash drive (this branch)
The board enumerates as a USB mass storage device whose blocks map 1:1 onto
the flash chip: LBA `n` is flash bytes `n*512 .. n*512+511`, 16384 blocks in
total. There is no filesystem layer; whatever the host writes lands in the
chip verbatim. A blank or non-FAT chip therefore shows up as "needs
formatting"; formatting it writes a FAT image into the chip, raw reads with
e.g. `dd` work regardless.

## build
```
git submodule update --init
build.bat
flash.bat
```
TinyUSB is a submodule, pinned by the repo. `build.bat` needs `arm-none-eabi-gcc` on
the path and writes `bin/firmware.bin`; `flash.bat` pushes it over DFU.

## behaviour
- Writes are gathered per 4 KiB flash sector in RAM and committed when the host
  finishes a WRITE command, syncs, ejects, or after 250 ms of silence. A sector
  is erased only when some bit has to go back to 1; unchanged pages are skipped
  and every committed sector is read back and verified.
- Hold KEY (PA0) for 2 s to erase the whole chip. The drive reports "no medium"
  while the erase runs (20 s or so) and comes back by itself.
- SPI runs at 10.5 MHz (`FLASH25Q64_SPI_BR` in `src/flash/flash25q64.c`).

## LED (PC13)
| pattern                     | meaning                              |
| --------------------------- | ------------------------------------ |
| short blip every second     | mounted by a host                    |
| 50% blink, 2 Hz             | plugged in, not mounted              |
| fast flicker               | chip erase in progress               |
| frantic flicker             | a flash write failed verification    |
| two blinks, pause           | flash chip not found (bad JEDEC ID)  |
| solid on                    | hard fault or clock failure          |
