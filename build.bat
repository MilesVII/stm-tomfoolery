@echo off
if not exist bin mkdir bin
arm-none-eabi-gcc ^
	--specs=nosys.specs -mcpu=cortex-m4 -mthumb -O0 -g3 ^
	-Wall -Wextra -Wno-unused-parameter ^
	-I src ^
	-I src/usb ^
	-I stm/cmsis/include-core ^
	-I stm/cmsis/include-device ^
	-I tinyusb/src ^
	-T stm/STM32F411CEUX_FLASH.ld ^
	stm/startup_stm32f411xe-gcc.s ^
	stm/system_stm32f4xx.c ^
	src/hal_at_home.c ^
	src/usb/board.c ^
	src/usb/usb_descriptors.c ^
	src/flash/flash25q64.c ^
	src/msc_disk.c ^
	src/main.c ^
	tinyusb/src/tusb.c ^
	tinyusb/src/common/tusb_fifo.c ^
	tinyusb/src/device/usbd.c ^
	tinyusb/src/portable/synopsys/dwc2/dcd_dwc2.c ^
	tinyusb/src/portable/synopsys/dwc2/dwc2_common.c ^
	tinyusb/src/class/msc/msc_device.c ^
	-o ./bin/firmware.elf
if errorlevel 1 exit /b 1
arm-none-eabi-objcopy -O binary ./bin/firmware.elf ./bin/firmware.bin
arm-none-eabi-size ./bin/firmware.elf
