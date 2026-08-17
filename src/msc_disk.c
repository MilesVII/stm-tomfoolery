#include "tusb.h"
#include "flash/flash25q64.h"

#if CFG_TUD_MSC

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTOTYPES
//--------------------------------------------------------------------+

// Disk geometry: 8 MiB flash, 512-byte sectors
#define DISK_BLOCK_NUM    (8 * 1024 * 1024 / 512)  // 16384 sectors
#define DISK_BLOCK_SIZE   512

//--------------------------------------------------------------------+
// Write-back cache
//--------------------------------------------------------------------+
// Flash-backed MSC requires 4 KiB sector erase before programming.
// To avoid read-modify-erase-write on every 512-byte USB write, we
// cache multiple 4 KiB flash sectors in RAM and flush only when the
// cache is full or on safe-removal (START STOP UNIT with LOAD_EJECT).

#define CACHE_NUM_SLOTS  4  // 4 x 4 KiB = 16 KiB RAM

typedef struct {
	uint32_t sector_base;   // 0xFFFFFFFF = invalid/empty
	uint8_t  data[FLASH25Q64_SECTOR_SIZE];
	bool     dirty;
	bool     valid;
} cache_slot_t;

static cache_slot_t cache[CACHE_NUM_SLOTS];
static bool flush_requested;

static void cache_flush_slot(cache_slot_t *slot) {
	if (slot->dirty && slot->valid) {
		flash25q64_erase_and_write(slot->sector_base, slot->data, FLASH25Q64_SECTOR_SIZE);
		slot->dirty = false;
	}
}

static void cache_flush_all(void) {
	for (int i = 0; i < CACHE_NUM_SLOTS; i++) {
		cache_flush_slot(&cache[i]);
	}
}

static cache_slot_t *cache_find_or_alloc(uint32_t sector_base) {
	// Check if sector is already cached
	for (int i = 0; i < CACHE_NUM_SLOTS; i++) {
		if (cache[i].valid && cache[i].sector_base == sector_base) {
			return &cache[i];
		}
	}

	// Find an empty slot
	for (int i = 0; i < CACHE_NUM_SLOTS; i++) {
		if (!cache[i].valid) {
			cache[i].sector_base = sector_base;
			cache[i].valid = true;
			cache[i].dirty = false;
			flash25q64_read(sector_base, cache[i].data, FLASH25Q64_SECTOR_SIZE);
			return &cache[i];
		}
	}

	// All slots full: evict slot 0 (simple policy; could be LRU)
	cache_flush_slot(&cache[0]);
	cache[0].sector_base = sector_base;
	cache[0].dirty = false;
	flash25q64_read(sector_base, cache[0].data, FLASH25Q64_SECTOR_SIZE);
	return &cache[0];
}

//--------------------------------------------------------------------+
// Public API
//--------------------------------------------------------------------+

// Initialize the write-back cache state.
// Must be called before any MSC callbacks are invoked.
void msc_disk_init(void) {
	for (int i = 0; i < CACHE_NUM_SLOTS; i++) {
		cache[i].sector_base = 0xFFFFFFFF;
		cache[i].dirty = false;
		cache[i].valid = false;
	}
	flush_requested = false;
}

// Flush all dirty cached sectors to flash.
// Call this from the main loop when safe to block for ~50 ms per dirty sector.
void msc_disk_flush(void) {
	cache_flush_all();
}

// Check if a flush was requested (e.g. by safe-removal) and flush if so.
bool msc_disk_pending_flush(void) {
	if (flush_requested) {
		flush_requested = false;
		msc_disk_flush();
		return true;
	}
	return false;
}

//--------------------------------------------------------------------+
// Flash-backed MSC callbacks
//--------------------------------------------------------------------+

// Invoked when received SCSI_CMD_INQUIRY
uint32_t tud_msc_inquiry2_cb(uint8_t lun, scsi_inquiry_resp_t *inquiry_resp, uint32_t bufsize) {
	(void) lun;
	(void) bufsize;

	const char vid[] = "Custom";
	const char pid[] = "STM32F411 Flash";
	const char rev[] = "1.0";

	(void) strncpy((char*) inquiry_resp->vendor_id, vid, 8);
	(void) strncpy((char*) inquiry_resp->product_id, pid, 16);
	(void) strncpy((char*) inquiry_resp->product_rev, rev, 4);

	return sizeof(scsi_inquiry_resp_t);
}

// Invoked when received Test Unit Ready command
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
	(void) lun;
	return true;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
	(void) lun;
	*block_count = DISK_BLOCK_NUM;
	*block_size = DISK_BLOCK_SIZE;
}

// Invoked when received Start Stop Unit command
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
	(void) lun;
	(void) power_condition;
	(void) start;
	if (load_eject) {
		// Defer the slow flash flush to the main loop so we don't block
		// the USB stack inside this callback.
		flush_requested = true;
	}
	return true;
}

// Callback invoked when received READ10 command
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
	(void) lun;

	if (lba >= DISK_BLOCK_NUM) {
		return -1;
	}

	uint32_t flash_addr = lba * DISK_BLOCK_SIZE + offset;
	uint32_t sector_base = flash_addr & ~(FLASH25Q64_SECTOR_SIZE - 1);

	// Find or load the sector into cache, then serve from there
	cache_slot_t *slot = cache_find_or_alloc(sector_base);
	memcpy(buffer, slot->data + (flash_addr - sector_base), bufsize);

	return (int32_t) bufsize;
}

bool tud_msc_is_writable_cb(uint8_t lun) {
	(void) lun;
	return true;
}

// Callback invoked when received WRITE10 command
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
	(void) lun;

	if (lba >= DISK_BLOCK_NUM) {
		return -1;
	}

	uint32_t flash_addr = lba * DISK_BLOCK_SIZE + offset;
	uint32_t sector_base = flash_addr & ~(FLASH25Q64_SECTOR_SIZE - 1);

	// Find or load the sector into cache, then apply the write
	cache_slot_t *slot = cache_find_or_alloc(sector_base);
	memcpy(slot->data + (flash_addr - sector_base), buffer, bufsize);
	slot->dirty = true;

	return (int32_t) bufsize;
}

// Callback invoked when received an SCSI command not in built-in list
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize) {
	(void) lun;
	(void) scsi_cmd;
	(void) buffer;
	(void) bufsize;

	// Set Sense = Invalid Command Operation
	tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

	return -1;
}

#endif
