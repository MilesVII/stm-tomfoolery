@echo off
set VIDPID=0483:df11

echo Waiting for STM32 DFU device...

:wait_loop
dfu-util -l | find "%VIDPID%" >nul
if errorlevel 1 (
	timeout /t 1 >nul
	goto wait_loop
)

echo Device detected. Flashing...
dfu-util -a 0 -d ,%VIDPID% -s 0x08000000:leave -D bin/firmware.bin

echo Done.