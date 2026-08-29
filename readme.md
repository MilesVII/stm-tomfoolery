# STM32F411
Running tetris on SH1106 display with Xbox One-compatible USB gamepad.

USB is on pins A11 for D- and A12 for D+, onboard type-C port doesn't work, requires direct connection. assumed gamepad hardware id is `USB\VID_20D6&PID_200D`. tinyUSB integration and gamepad driver are v*becoded. 

C13-14-15 | A0-A7 | B0-1-2 B10
B9-B3 | A15 A12-A8 | B15-B12

# pins
## unusable (blackpill board)
`A11`, `A12`, `B2`
avoid: `A9`, `A10`

refer to device drivers to check the pin definitons
