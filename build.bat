arm-none-eabi-gcc ^
 --specs=nosys.specs -mcpu=cortex-m4 -mthumb -O2 ^
 -I src\cmsis\include-core ^
 -I src\cmsis\include-device ^
 -I src\cmsis ^
 -T STM32F411CEUX_FLASH.ld ^
 src/cmsis/startup_stm32f411xe-gcc.s ^
 src/system_stm32f4xx.c src/hal_at_home.c src/display.c src/thermal.c src/main.c ^
 -o ./bin/firmware.elf
arm-none-eabi-objcopy -O binary ./bin/firmware.elf ./bin/firmware.bin