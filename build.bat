arm-none-eabi-gcc ^
 --specs=nosys.specs -mcpu=cortex-m4 -mthumb -O2 ^
 -I stm/cmsis/include-core ^
 -I stm/cmsis/include-device ^
 -T stm/STM32F411CEUX_FLASH.ld ^
 stm/startup_stm32f411xe-gcc.s ^
 stm/system_stm32f4xx.c src/hal_at_home.c src/display.c src/thermal.c src/main.c ^
 -o ./bin/firmware.elf
arm-none-eabi-objcopy -O binary ./bin/firmware.elf ./bin/firmware.bin