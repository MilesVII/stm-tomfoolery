# STM32F411
Pure CMSIS drivers, no HAL

# pins
## unusable (blackpill board)
`A11`, `A12`, `B2`
avoid: `A9`, `A10`

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
B9 CS (soft NSS)
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
