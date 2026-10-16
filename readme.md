# STM32F411
Pure CMSIS drivers, no HAL

C13-14-15 | A0-A7 | B0-1-2 B10
B9-B3 | A15 A12-A8 | B15-B12

# pins
## unusable (blackpill board)
`A11`, `A12`, `B2`
avoid: `A9`, `A10`

## OV7670
```
RST  B2
PWDN B1
HS   A3
VS   A1

I2C1 AF4:
B6 SCL
B7 SDA

pixels:
D0 A9
D1 A10
D2 A11
D3 A12
D4 A15
D5 B3
D6 B4
D7 B5
(D0-D3, D4 and D5-D7 are each contiguous on their port, the
capture loop depends on that: see bus_byte in camera.c)

clocks:
PLK A2
XLK B8
```

## 25Q64JVSIQ
8MiB flash chip

```
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

## ILI9341/V
display

```
LED(ignored) -> 3V via 100ohm

SPI2 AF5:
(hanging) SDO/MISO
B13 SCK
B15 SDI/MOSI

GPIO:
B14 DC
B12 RESET
A8 CS (soft NSS)
```


## SH1106
display but cute

```
SP1 AF5:
A5 SCK
A7 MOSI

GPIO:
B0 D/C
B1 RESET
A6 CS
```

## FT6336G
capacitive touchscreen
```
I2C1 AF4:
B6 SCL
B7 SDA

GPIO:
__ INT (unused (fucking useless))
B5 RST
```

## XPT2046
resistive touchscreen
```
SPI3 AF6:
B3 SCK
B5 MOSI

GPIO:
A15 NSS (soft)
B6 IRQ
```

# camera (this branch)
OV7670 without FIFO, bit-banged. The MCU runs at 84 MHz from the PLL, feeds
the sensor 12 MHz on XCLK and reads QVGA RGB565 frames pixel by pixel, keeping
them as RGB332 (76800 bytes) since a full RGB565 frame does not fit in RAM.
Frames stream to the ILI9341 in landscape, about 2 per second.

- The sensor is configured with the register tables from the Linux ov7670
  driver (default set, RGB565 set, QVGA window), then DBLV = PLL bypass and
  CLKRC = `CAMERA_CLKRC` (camera.c). PCLK = 12 MHz / (CLKRC + 1): lower CLKRC
  for more fps, raise it if frames come out torn.
- KEY (PA0) toggles the sensor color bars. If the bars look right the data
  bus and capture loop are fine and any remaining problem is optics/exposure.
- The capture loop needs `-O2`; `-O0` is too slow to keep up with PCLK.

## LED (PC13) / screen
| what                          | meaning                                          |
| ----------------------------- | ------------------------------------------------ |
| short flash per frame         | capturing and displaying                         |
| 1 blink, pause                | nothing answers on SCCB (wiring, power, pull-ups)|
| 2 blinks, pause               | SCCB answers but PID is not 0x76                 |
| 3 blinks, pause               | a register write failed mid-setup                |
| solid on                      | clock init failed                                |

On a camera error the screen shows the error number and PID<<8|VER as a
decimal (30323 = 0x7673 = OV7670).
