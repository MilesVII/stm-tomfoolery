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
