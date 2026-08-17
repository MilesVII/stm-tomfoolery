arm-none-eabi-gcc ^
	--specs=nosys.specs -mcpu=cortex-m4 -mthumb -O2 ^
	-DCFG_TUSB_MCU=304 ^
	-DBOARD_TUD_RHPORT=0 ^
	-DBOARD_TUD_MAX_SPEED=512 ^
	-DBOARD_TUH_RHPORT=0 ^
	-DBOARD_TUH_MAX_SPEED=512 ^
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
	src/usb/board.c ^
	src/flash/flash25q64.c ^
	src/usb/usb_descriptors.c ^
	src/msc_disk.c ^
	tinyusb/src/tusb.c ^
	tinyusb/src/common/tusb_fifo.c ^
	tinyusb/src/device/usbd.c ^
	tinyusb/src/portable/synopsys/dwc2/dcd_dwc2.c ^
	tinyusb/src/portable/synopsys/dwc2/dwc2_common.c ^
	tinyusb/src/class/msc/msc_device.c ^
	src/main.c ^
	-o ./bin/firmware.elf
arm-none-eabi-objcopy -O binary ./bin/firmware.elf ./bin/firmware.bin
