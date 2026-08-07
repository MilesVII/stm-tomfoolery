#include "tusb.h"
#include "flash25q64.h"

#if CFG_TUD_MSC

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTOTYPES
//--------------------------------------------------------------------+

// Disk geometry: 8 MiB flash, 512-byte sectors
#define DISK_BLOCK_NUM    (8 * 1024 * 1024 / 512)  // 16384 sectors
#define DISK_BLOCK_SIZE   512

// Sector buffer for read-modify-erase-write (4 KiB flash sector = 8 x 512B USB sectors)
static uint8_t sector_buf[DISK_BLOCK_SIZE];

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
    (void) load_eject;
    return true;
}

// Callback invoked when received READ10 command
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    (void) lun;

    if (lba >= DISK_BLOCK_NUM) {
        return -1;
    }

    // Read directly from flash
    flash25q64_read(lba * DISK_BLOCK_SIZE + offset, (uint8_t *)buffer, bufsize);

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

    // Flash write requires sector erase (4 KiB) before programming
    // We need to read-modify-erase-write the containing 4 KiB flash sector
    uint32_t flash_addr = lba * DISK_BLOCK_SIZE + offset;
    uint32_t sector_base = flash_addr & ~(FLASH25Q64_SECTOR_SIZE - 1);

    // Read the entire 4 KiB flash sector
    flash25q64_read(sector_base, sector_buf, FLASH25Q64_SECTOR_SIZE);

    // Modify the portion being written
    memcpy(sector_buf + (flash_addr - sector_base), buffer, bufsize);

    // Erase and write back
    flash25q64_erase_and_write(sector_base, sector_buf, FLASH25Q64_SECTOR_SIZE);

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
