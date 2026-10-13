#pragma once

#include <stdbool.h>

// USB mass storage backed 1:1 by the SPI flash chip: LBA n maps to flash
// bytes [n*512, n*512+512). Writes are collected per 4 KiB flash sector in a
// single write-back buffer that is committed when the host finishes a WRITE
// command, asks for a cache sync, ejects the drive, or when the sector changes.

void msc_disk_init(void);
// Call from the main loop: finishes a pending chip erase, flushes stale data.
void msc_disk_task(void);
// Commit the write buffer to flash now. Returns false on a flash error.
bool msc_disk_flush(void);

// Start erasing the whole chip; the drive reports "no medium" until it ends.
bool msc_disk_chip_erase_start(void);
bool msc_disk_erasing(void);
// Sticky: some flash write failed verification at some point.
bool msc_disk_error(void);
