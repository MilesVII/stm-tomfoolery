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

## ILI9341
LED(ignored) -> 3V via 100ohm
```
SPI2 AF5:
(hanging) SDO/MISO
B10 SCK
B15 SDI/MOSI

GPIO:
B8 DC
B7 RESET
B9 CS (manual)
```

## XPT2046
```
SPI3 AF6:
B4 DO
B5 DIN
B3 CLK

GPIO:
B6 IRQ
A15 CS (manual)
```