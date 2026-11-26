#!/usr/bin/env bash
VIDPID=0483:df11

echo "Waiting for STM32 DFU device..."

until dfu-util -l 2>/dev/null | grep -q "$VIDPID"; do
	sleep 1
done

echo "Device detected. Flashing..."
dfu-util -a 0 -d ",$VIDPID" -s 0x08000000:leave -D bin/firmware.bin

echo "Done."
