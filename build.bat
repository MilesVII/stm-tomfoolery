@echo off
if not exist bin mkdir bin
arm-none-eabi-gcc ^
	--specs=nosys.specs -mcpu=cortex-m4 -mthumb -O2 ^
	-Wall ^
	-I stm/cmsis/include-core ^
	-I stm/cmsis/include-device ^
	-T stm/STM32F411CEUX_FLASH.ld ^
	stm/startup_stm32f411xe-gcc.s ^
	stm/system_stm32f4xx.c src/hal_at_home.c ^
	src/ili9341/display.c ^
	src/ov7670/camera.c ^
	src/picture.S ^
	src/main.c ^
	-o ./bin/firmware.elf
if errorlevel 1 exit /b 1
arm-none-eabi-objcopy -O binary ./bin/firmware.elf ./bin/firmware.bin
arm-none-eabi-size ./bin/firmware.elf
