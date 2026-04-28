STM32F411

Pure CMSIS display driver, no HAL

# pins
## 25Q64JVSIQ 8MiB flash chip:
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

## ILI9341V
LED(ignored) -> 3V via 100ohm
```
SPI2 AF5:
(hanging) SDO/MISO
B10 SCK
B15 SDI/MOSI

GPIO:
B8 DC
B2 RESET
B9 CS (manual)
```

## FT6336G
```
I2C1 AF4:
B6 SCL
B7 SDA

INT (unused (fucking useless))
B5 RST (manual)
```