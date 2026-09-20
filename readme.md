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
A8 CS (soft NSS)
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

