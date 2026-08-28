// Custom TinyUSB host class driver for Xbox GIP gamepads
// (vendor-specific interface: class 0xFF, subclass 0x47, protocol 0xD0,
// e.g. VID_20D6&PID_200D "Xbox One S" style pads).
//
// Protocol essentials:
//  - After enumeration, send power/announce packet on the OUT interrupt EP:
//      05 20 00 01 00
//  - Periodically send keep-alive:
//      05 11 00 00 00 00 00 00 00 00 00 00
//  - Pad then streams input reports on the IN interrupt EP starting with 0x20:
//      [0]=0x20 [1]=counter [2]=unused [3]=unused
//      [4..5] buttons (big endian bitmask)
//      [6..7] LT/RT triggers, [8..15] LX LY RX RY int16 LE

#include "tusb_option.h"

#if CFG_TUH_ENABLED && CFG_TUH_XGIP

#include "host/usbh.h"
#include "host/usbh_pvt.h"
#include "xgip_host.h"

//--------------------------------------------------------------------+
// Configuration / state
//--------------------------------------------------------------------+

#define XGIP_POWER_ON_INTERVAL_MS   100   // retry power-on if no reports arrive
#define XGIP_KEEPALIVE_INTERVAL_MS  1000

typedef struct {
	uint8_t daddr;
	uint8_t itf_num;
	uint8_t ep_in;
	uint8_t ep_out;

	bool mounted;          // set_config() completed
	bool powered_on;       // power-on packet sent successfully

	uint16_t buttons;      // latest button bitmask
	uint32_t last_power_ms;
	uint32_t last_keepalive_ms;

	CFG_TUH_MEM_ALIGN uint8_t epin_buf[CFG_TUH_XGIP_EP_BUFSIZE];
	CFG_TUH_MEM_ALIGN uint8_t epout_buf[12];
} xgip_interface_t;

static xgip_interface_t _xgip_itf;

bool xgip_mounted(void) {
	return _xgip_itf.mounted && _xgip_itf.powered_on;
}

uint16_t xgip_buttons(void) {
	return _xgip_itf.buttons;
}

bool xgip_any_button_pressed(void) {
	return _xgip_itf.buttons != 0;
}

//--------------------------------------------------------------------+
// Transfers
//--------------------------------------------------------------------+

static void xgip_queue_in(xgip_interface_t *p_xgip) {
	// Completion is routed back to xgip_xfer_cb() by usbh (class driver)
	bool r0 = usbh_edpt_xfer(p_xgip->daddr, p_xgip->ep_in,
			p_xgip->epin_buf, sizeof(p_xgip->epin_buf));
	(void) r0;
}

static void xgip_send_out(xgip_interface_t *p_xgip, const uint8_t *pkt, uint16_t len) {
	memcpy(p_xgip->epout_buf, pkt, len);
	bool r0 = usbh_edpt_xfer(p_xgip->daddr, p_xgip->ep_out, p_xgip->epout_buf, len);
	(void) r0;
}

static void xgip_send_power_on(xgip_interface_t *p_xgip) {
	static const uint8_t power_on[] = { 0x05, 0x20, 0x00, 0x01, 0x00 };
	xgip_send_out(p_xgip, power_on, sizeof(power_on));
	p_xgip->last_power_ms = tusb_time_millis_api();
}

static void xgip_send_keepalive(xgip_interface_t *p_xgip) {
	static const uint8_t keepalive[] = {
		0x05, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	xgip_send_out(p_xgip, keepalive, sizeof(keepalive));
	p_xgip->last_keepalive_ms = tusb_time_millis_api();
}

// Called from main loop: periodic keep-alive + power-on retry
void xgip_task(void) {
	xgip_interface_t *p_xgip = &_xgip_itf;
	if (!p_xgip->mounted || !p_xgip->daddr) return;

	uint32_t now = tusb_time_millis_api();

	if (!p_xgip->powered_on) {
		if (now - p_xgip->last_power_ms >= XGIP_POWER_ON_INTERVAL_MS) {
			xgip_send_power_on(p_xgip);
		}
	} else if (now - p_xgip->last_keepalive_ms >= XGIP_KEEPALIVE_INTERVAL_MS) {
		xgip_send_keepalive(p_xgip);
	}
}

//--------------------------------------------------------------------+
// Class driver implementation
//--------------------------------------------------------------------+

static bool xgip_init(void) {
	memset(&_xgip_itf, 0, sizeof(_xgip_itf));
	return true;
}

static bool xgip_deinit(void) {
	memset(&_xgip_itf, 0, sizeof(_xgip_itf));
	return true;
}

// Check if this interface matches the GIP gamepad; if so claim it and
// return the number of bytes consumed from the interface descriptor.
static uint16_t xgip_open(uint8_t rhport, uint8_t dev_addr,
		const tusb_desc_interface_t *itf_desc, uint16_t max_len) {
	(void) rhport;

	// GIP interface: vendor-specific class, subclass 0x47, protocol 0xD0
	TU_VERIFY(itf_desc->bInterfaceClass == 0xFF &&
			itf_desc->bInterfaceSubClass == 0x47 &&
			itf_desc->bInterfaceProtocol == 0xD0, 0);

	TU_VERIFY(itf_desc->bNumEndpoints >= 1, 0);
	TU_VERIFY(max_len >= sizeof(tusb_desc_interface_t) +
			itf_desc->bNumEndpoints * sizeof(tusb_desc_endpoint_t), 0);

	xgip_interface_t *p_xgip = &_xgip_itf;
	TU_ASSERT(p_xgip->daddr == 0, 0); // only one pad supported

	uint8_t const *p_desc = (uint8_t const *) itf_desc;
	uint8_t const *desc_end = p_desc + max_len;

	p_desc += sizeof(tusb_desc_interface_t);

	// Parse endpoint descriptors
	for (uint8_t i = 0; i < itf_desc->bNumEndpoints; i++) {
		tusb_desc_endpoint_t const *desc_ep = (tusb_desc_endpoint_t const *) p_desc;
		TU_VERIFY(TUSB_DESC_ENDPOINT == desc_ep->bDescriptorType, 0);

		bool r0 = tuh_edpt_open(dev_addr, desc_ep);

		if (desc_ep->bEndpointAddress & TUSB_DIR_IN_MASK) {
			p_xgip->ep_in = desc_ep->bEndpointAddress;
		} else {
			p_xgip->ep_out = desc_ep->bEndpointAddress;
		}

		p_desc += sizeof(tusb_desc_endpoint_t);
	}

	TU_ASSERT(p_xgip->ep_in != 0, 0);

	p_xgip->daddr = dev_addr;
	p_xgip->itf_num = itf_desc->bInterfaceNumber;
	p_xgip->mounted = false;
	p_xgip->powered_on = false;
	p_xgip->buttons = 0;

	return (uint16_t) ((uint8_t const *) p_desc - (uint8_t const *) itf_desc);
}

// Called by usbh after SET_CONFIGURATION completed
static bool xgip_set_config(uint8_t dev_addr, uint8_t itf_num) {
	xgip_interface_t *p_xgip = &_xgip_itf;
	TU_ASSERT(p_xgip->daddr == dev_addr && p_xgip->itf_num == itf_num);

	p_xgip->mounted = true;
	p_xgip->last_power_ms = tusb_time_millis_api() - XGIP_POWER_ON_INTERVAL_MS;
	p_xgip->last_keepalive_ms = tusb_time_millis_api();

	// Start listening for input reports
	xgip_queue_in(p_xgip);

	// Power-on will be sent by xgip_task() on next main-loop iteration
	usbh_driver_set_config_complete(dev_addr, itf_num);
	return true;
}

static bool xgip_xfer_cb(uint8_t dev_addr, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
	(void) dev_addr;
	(void) result;

	xgip_interface_t *p_xgip = &_xgip_itf;

	if (ep_addr == p_xgip->ep_in) {
		if (result == XFER_RESULT_SUCCESS && xferred_bytes >= 6 &&
				p_xgip->epin_buf[0] == 0x20) {
			// Input report: buttons in bytes 4-5 (big endian)
			p_xgip->buttons =
					(uint16_t) ((p_xgip->epin_buf[4] << 8) | p_xgip->epin_buf[5]);
			p_xgip->powered_on = true;
		}
		// Re-queue for the next report
		xgip_queue_in(p_xgip);
	}
	// OUT completions (power-on / keep-alive) need no handling

	return true;
}

static void xgip_close(uint8_t dev_addr) {
	xgip_interface_t *p_xgip = &_xgip_itf;
	if (p_xgip->daddr != dev_addr) return;

	memset(p_xgip, 0, sizeof(*p_xgip));
}

//--------------------------------------------------------------------+
// Driver registration (consumed by usbh via usbh_app_driver_get_cb)
//--------------------------------------------------------------------+

static usbh_class_driver_t const xgip_driver = {
	.name = "XGIP",
	.init = xgip_init,
	.deinit = xgip_deinit,
	.open = xgip_open,
	.set_config = xgip_set_config,
	.xfer_cb = xgip_xfer_cb,
	.close = xgip_close,
};

usbh_class_driver_t const *usbh_app_driver_get_cb(uint8_t *driver_count) {
	*driver_count = 1;
	return &xgip_driver;
}

#endif // CFG_TUH_ENABLED && CFG_TUH_XGIP
