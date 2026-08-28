arm-none-eabi-gcc ^
	--specs=nosys.specs -mcpu=cortex-m4 -mthumb -O0 -g3 ^
	-DCFG_TUSB_MCU=304 ^
	-DBOARD_TUH_RHPORT=0 ^
	-DBOARD_TUH_MAX_SPEED=OPT_MODE_FULL_SPEED ^
	-I src ^
	-I src/usb ^
	-I stm/cmsis/include-core ^
	-I stm/cmsis/include-device ^
	-I tinyusb/src ^
	-I tinyusb/hw/bsp/stm32f4 ^
	-T stm/STM32F411CEUX_FLASH.ld ^
	stm/startup_stm32f411xe-gcc.s ^
	stm/system_stm32f4xx.c ^
	src/hal_at_home.c ^
	src/sh1106/display.c ^
	src/usb/board.c ^
	src/usb/xgip_host.c ^
	tinyusb/src/tusb.c ^
	tinyusb/src/common/tusb_fifo.c ^
	tinyusb/src/host/usbh.c ^
	tinyusb/src/host/hub.c ^
	tinyusb/src/portable/synopsys/dwc2/hcd_dwc2.c ^
	tinyusb/src/portable/synopsys/dwc2/dwc2_common.c ^
	src/main.c ^
	src/tetris.c ^
	-o ./bin/firmware.elf
arm-none-eabi-objcopy -O binary ./bin/firmware.elf ./bin/firmware.bin
