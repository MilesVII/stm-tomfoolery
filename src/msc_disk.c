#include <string.h>
#include "tusb.h"
#include "hal_at_home.h"
#include "msc_disk.h"
#include "flash/flash25q64.h"

#define BLOCK_SIZE   512u
#define BLOCK_COUNT  (FLASH25Q64_SIZE / BLOCK_SIZE)
#define SECTOR_SIZE  FLASH25Q64_SECTOR_SIZE

// Not in the TinyUSB SCSI enum; hosts send these on sync / unmount.
#define SCSI_CMD_SYNCHRONIZE_CACHE_10  0x35
#define SCSI_CMD_SYNCHRONIZE_CACHE_16  0x91

// Flush on our own if the host goes quiet mid-command for this long.
#define IDLE_FLUSH_MS   250u
#define ERASE_POLL_MS   50u

//--------------------------------------------------------------------+
// Single-sector write-back buffer
//--------------------------------------------------------------------+

static uint8_t  wb_data[SECTOR_SIZE];
static uint32_t wb_addr = UINT32_MAX; // sector base address, UINT32_MAX = empty
static bool     wb_dirty;
static uint32_t wb_last_write_ms;

static bool media_present = true;     // false while the chip is being erased
static bool io_error;

static bool wb_flush(void) {
	if (!wb_dirty) return true;
	wb_dirty = false;
	if (flash25q64_write_sector(wb_addr, wb_data)) return true;
	io_error = true;
	wb_addr = UINT32_MAX; // flash contents are unknown now, stop serving reads from it
	return false;
}

// Point the buffer at `sector`, loading it from flash unless the caller is
// about to overwrite all of it anyway.
static bool wb_select(uint32_t sector, bool whole_sector) {
	if (wb_addr == sector) return true;
	if (!wb_flush()) return false;
	wb_addr = sector;
	if (!whole_sector) flash25q64_read(sector, wb_data, SECTOR_SIZE);
	return true;
}

static bool disk_write(uint32_t addr, const uint8_t* src, uint32_t len) {
	while (len) {
		uint32_t sector = addr & ~(SECTOR_SIZE - 1);
		uint32_t off = addr - sector;
		uint32_t n = SECTOR_SIZE - off;
		if (n > len) n = len;

		if (!wb_select(sector, off == 0 && n == SECTOR_SIZE)) return false;
		memcpy(wb_data + off, src, n);
		wb_dirty = true;

		addr += n;
		src += n;
		len -= n;
	}
	wb_last_write_ms = ticks;
	return true;
}

static void disk_read(uint32_t addr, uint8_t* dst, uint32_t len) {
	while (len) {
		uint32_t sector = addr & ~(SECTOR_SIZE - 1);
		uint32_t off = addr - sector;
		uint32_t n = SECTOR_SIZE - off;
		if (n > len) n = len;

		if (sector == wb_addr) {
			memcpy(dst, wb_data + off, n);
		} else {
			flash25q64_read(addr, dst, n);
		}

		addr += n;
		dst += n;
		len -= n;
	}
}

//--------------------------------------------------------------------+
// Public API
//--------------------------------------------------------------------+

void msc_disk_init(void) {
	wb_addr = UINT32_MAX;
	wb_dirty = false;
	media_present = true;
	io_error = false;
}

bool msc_disk_flush(void) {
	return wb_flush();
}

void msc_disk_task(void) {
	static uint32_t last_poll;

	if (!media_present) {
		if (ticks - last_poll >= ERASE_POLL_MS) {
			last_poll = ticks;
			if (!flash25q64_busy()) media_present = true;
		}
		return;
	}
	if (wb_dirty && ticks - wb_last_write_ms >= IDLE_FLUSH_MS) {
		wb_flush();
	}
}

bool msc_disk_chip_erase_start(void) {
	if (!media_present) return false;
	// Everything is about to become 0xFF; whatever was buffered is moot.
	wb_dirty = false;
	wb_addr = UINT32_MAX;
	if (!flash25q64_chip_erase_start()) {
		io_error = true;
		return false;
	}
	media_present = false;
	return true;
}

bool msc_disk_erasing(void) {
	return !media_present;
}

bool msc_disk_error(void) {
	return io_error;
}

//--------------------------------------------------------------------+
// TinyUSB MSC callbacks
//--------------------------------------------------------------------+

// SCSI strings are space padded, not NUL terminated.
static void pad_copy(uint8_t* dst, uint32_t n, const char* s) {
	uint32_t i = 0;
	for (; i < n && s[i]; i++) dst[i] = (uint8_t)s[i];
	for (; i < n; i++) dst[i] = 0x20;
}

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
	(void) lun;
	pad_copy(vendor_id, 8, "STM32");
	pad_copy(product_id, 16, "W25Q64 flash");
	pad_copy(product_rev, 4, "1.0");
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
	(void) lun;
	// TinyUSB reports NOT READY / MEDIUM NOT PRESENT for us when this is false.
	return media_present;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
	(void) lun;
	*block_count = BLOCK_COUNT;
	*block_size = BLOCK_SIZE;
}

bool tud_msc_is_writable_cb(uint8_t lun) {
	(void) lun;
	return true;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
	(void) lun;
	(void) power_condition;
	// "Safely remove" / eject
	if (load_eject && !start) wb_flush();
	return true;
}

bool tud_msc_prevent_allow_medium_removal_cb(uint8_t lun, uint8_t prohibit_removal, uint8_t control) {
	(void) lun;
	(void) control;
	if (!prohibit_removal) wb_flush();
	return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
	(void) lun;
	if (!media_present || lba >= BLOCK_COUNT) return TUD_MSC_RET_ERROR;

	uint32_t addr = lba * BLOCK_SIZE + offset;
	if (addr >= FLASH25Q64_SIZE) return TUD_MSC_RET_ERROR;
	if (bufsize > FLASH25Q64_SIZE - addr) bufsize = FLASH25Q64_SIZE - addr;

	disk_read(addr, buffer, bufsize);
	return (int32_t) bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
	(void) lun;
	if (!media_present || lba >= BLOCK_COUNT) return TUD_MSC_RET_ERROR;

	uint32_t addr = lba * BLOCK_SIZE + offset;
	if (addr >= FLASH25Q64_SIZE) return TUD_MSC_RET_ERROR;
	if (bufsize > FLASH25Q64_SIZE - addr) bufsize = FLASH25Q64_SIZE - addr;

	if (!disk_write(addr, buffer, bufsize)) return TUD_MSC_RET_ERROR;
	return (int32_t) bufsize;
}

// The host considers the data written once the WRITE command completes, and
// the Windows default "quick removal" policy never asks for a sync afterwards.
void tud_msc_write10_complete_cb(uint8_t lun) {
	(void) lun;
	wb_flush();
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
	(void) buffer;
	(void) bufsize;

	switch (scsi_cmd[0]) {
		case SCSI_CMD_SYNCHRONIZE_CACHE_10:
		case SCSI_CMD_SYNCHRONIZE_CACHE_16:
			if (!wb_flush()) {
				tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x0C, 0x00); // WRITE ERROR
				return TUD_MSC_RET_ERROR;
			}
			return 0;

		default:
			tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00); // INVALID COMMAND OPERATION CODE
			return TUD_MSC_RET_ERROR;
	}
}

//--------------------------------------------------------------------+
// Device callbacks: last chance to commit before the link goes away
//--------------------------------------------------------------------+

void tud_umount_cb(void) {
	wb_flush();
}

void tud_suspend_cb(bool remote_wakeup_en) {
	(void) remote_wakeup_en;
	wb_flush();
}
