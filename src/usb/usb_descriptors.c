#include "tusb.h"
#include "board.h"

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+

#define USB_VID   0x1209  // PID: VID pair for open hardware
#define USB_PID   0x0001  // Custom PID
#define USB_BCD   0x0200  // USB 2.0

static tusb_desc_device_t const desc_device = {
	.bLength            = sizeof(tusb_desc_device_t),
	.bDescriptorType    = TUSB_DESC_DEVICE,
	.bcdUSB             = USB_BCD,

	// Use Interface Association Descriptor (IAD) for MSC
	.bDeviceClass       = TUSB_CLASS_MISC,
	.bDeviceSubClass    = MISC_SUBCLASS_COMMON,
	.bDeviceProtocol    = MISC_PROTOCOL_IAD,
	.bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

	.idVendor           = USB_VID,
	.idProduct          = USB_PID,
	.bcdDevice          = 0x0100,

	.iManufacturer      = 0x01,
	.iProduct           = 0x02,
	.iSerialNumber      = 0x03,

	.bNumConfigurations = 0x01
};

uint8_t const *tud_descriptor_device_cb(void) {
	return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

enum {
	ITF_NUM_MSC = 0,
	ITF_NUM_TOTAL
};

#define EPNUM_MSC_OUT     0x01
#define EPNUM_MSC_IN      0x81

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

static uint8_t const desc_fs_configuration[] = {
	// Config number, interface count, string index, total length, attribute, power in mA
	TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

	// Interface number, string index, EP Out & EP In address, EP size
	TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 4, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
	(void) index;
	return desc_fs_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

enum {
	STRID_LANGID = 0,
	STRID_MANUFACTURER,
	STRID_PRODUCT,
	STRID_SERIAL,
	STRID_MSC,
};

static char const *string_desc_arr[] = {
	(const char[]) { 0x09, 0x04 }, // 0: English (0x0409)
	"Custom",                       // 1: Manufacturer
	"STM32F411 Flash Drive",        // 2: Product
	NULL,                           // 3: Serial (from unique ID)
	"Mass Storage",                 // 4: MSC Interface
};

static uint16_t _desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
	(void) langid;
	size_t chr_count;

	switch (index) {
		case STRID_LANGID:
			memcpy(&_desc_str[1], string_desc_arr[0], 2);
			chr_count = 1;
			break;

		case STRID_SERIAL: {
			uint8_t uid[12];
			size_t uid_len = board_get_unique_id(uid, sizeof(uid));
			if (uid_len > 16) uid_len = 16;

			for (size_t i = 0; i < uid_len; i++) {
				const unsigned char nibble_to_hex[16] = {
					'0', '1', '2', '3', '4', '5', '6', '7',
					'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
				};
				uint8_t nibble_hi = (uid[i] >> 4) & 0x0F;
				uint8_t nibble_lo = uid[i] & 0x0F;
				_desc_str[1 + i * 2] = (uint16_t)((nibble_to_hex[nibble_hi] << 8) | nibble_to_hex[nibble_lo]);
			}
			chr_count = uid_len * 2;
			break;
		}

		default:
			if ( !(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) ) { return NULL; }

			const char *str = string_desc_arr[index];
			chr_count = strlen(str);
			size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;
			if (chr_count > max_count) chr_count = max_count;

			for (size_t i = 0; i < chr_count; i++) {
				_desc_str[1 + i] = (uint16_t)str[i];
			}
			break;
	}

	_desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
	return _desc_str;
}
