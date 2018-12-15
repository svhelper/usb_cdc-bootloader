/*
 * Copyright (c) 2016, Devan Lai
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <string.h>
#include <logger.h>
#include "usb_conf.h"
#include "webusb.h"
#include "usb21_standard.h"

#define CONTROL_CALLBACK_TYPE (USB_REQ_TYPE_VENDOR | USB_REQ_TYPE_DEVICE)
#define CONTROL_CALLBACK_MASK (USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT)

#define MIN(a, b) ({ typeof(a) _a = (a); typeof(b) _b = (b); _a < _b ? _a : _b; })

/*
Test in Chrome console:
navigator.usb.getDevices().then(console.log)

Captured BOS from microbit:

Binary Object Store descriptor
05:0f:39:00:02:

WebUSB Platform Capability Descriptor:
https://wicg.github.io/webusb/#webusb-platform-capability-descriptor
bLength: 18:
bDescriptorType: 10:
bDevCapabilityType: 05:
bReserved: 00:
PlatformCapability UUID:
platformCapabilityUUID: 38:b6:08:34:a9:09:a0:47:8b:fd:a0:76:88:15:b6:65:
bcdVersion: 00:01:
bVendorCode: 21:
iLandingPage: 00:

Microsoft OS 2.0 Platform Capability Descriptor:
bLength: 1c:
bDescriptorType: 10:
bDevCapabilityType: 05:
bReserved: 00:
MS OS 2.0 Platform Capability ID:
platformCapabilityUUID: df:60:dd:d8:89:45:c7:4c:9c:d2:65:9d:9e:64:8a:9f:
Windows version:
bcdVersion: 00:00:03:06:
Descriptor set length, Vendor code, Alternate enumeration code:
aa:00:20:00

0000   05 0f 39 00 02 18 10 05 00 38 b6 08 34 a9 09 a0   ..9......8¶.4©. 
0010   47 8b fd a0 76 88 15 b6 65 00 01 21 00 1c 10 05   G.ý v..¶e..!....
0020   00 df 60 dd d8 89 45 c7 4c 9c d2 65 9d 9e 64 8a   .ß`ÝØ.EÇL.Òe..d.
0030   9f 00 00 03 06 aa 00 20 00                        .....ª. .
*/

#ifdef NOTUSED
//  From https://github.com/intel/zephyr.js/blob/master/src/zjs_webusb.c
/* Microsoft OS 2.0 Descriptor Set */
static const uint8_t ms_os_20_descriptor_set[] = {
    0x0A, 0x00,              // wLength
    0x00, 0x00,              // MS OS 2.0 descriptor set header
    0x00, 0x00, 0x03, 0x06,  // Windows 8.1
    0xB2, 0x00,              // Size, MS OS 2.0 descriptor set

    // Configuration subset header
    0x08, 0x00,  // wLength
    0x01, 0x00,  // wDescriptorType
    0x00,        // bConfigurationValue
    0x00,        // bReserved
    0xA8, 0x00,  // wTotalLength of this subset header

    // Function subset header
    0x08, 0x00,  // wLength
    0x02, 0x00,  // wDescriptorType
    0x02,        // bFirstInterface
    0x00,        // bReserved
    0xA0, 0x00,  // wTotalLength of this subset header

    // Compatible ID descriptor
    0x14, 0x00,                                      // wLength
    0x03, 0x00,                                      // wDescriptorType
    'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,        // compatible ID
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // subCompatibleID

    // Extended properties descriptor with interface GUID
    0x84, 0x00,  // wLength
    0x04, 0x00,  // wDescriptorType
    0x07, 0x00,  // wPropertyDataType
    0x2A, 0x00,  // wPropertyNameLength
    // Property name : DeviceInterfaceGUIDs
    'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00,
    'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00, 'r', 0x00, 'f', 0x00,
    'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00,
    'D', 0x00, 's', 0x00, 0x00, 0x00,
    0x50, 0x00,  // wPropertyDataLength
    // Property data: {9D32F82C-1FB2-4486-8501-B6145B5BA336}
    '{', 0x00, '9', 0x00, 'D', 0x00, '3', 0x00, '2', 0x00, 'F', 0x00,
    '8', 0x00, '2', 0x00, 'C', 0x00, '-', 0x00, '1', 0x00, 'F', 0x00,
    'B', 0x00, '2', 0x00, '-', 0x00, '4', 0x00, '4', 0x00, '8', 0x00,
    '6', 0x00, '-', 0x00, '8', 0x00, '5', 0x00, '0', 0x00, '1', 0x00,
    '-', 0x00, 'B', 0x00, '6', 0x00, '1', 0x00, '4', 0x00, '5', 0x00,
    'B', 0x00, '5', 0x00, 'B', 0x00, 'A', 0x00, '3', 0x00, '3', 0x00,
    '6', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00
};

/* Microsoft OS 2.0 descriptor request */
#define MS_OS_20_REQUEST_DESCRIPTOR 0x07
#endif  //  NOTUSED

const struct webusb_platform_descriptor webusb_platform_capability_descriptor = {
	.bLength = WEBUSB_PLATFORM_DESCRIPTOR_SIZE,
	.bDescriptorType = USB_DT_DEVICE_CAPABILITY,
	.bDevCapabilityType = USB_DC_PLATFORM,
	.bReserved = 0,
	.platformCapabilityUUID = WEBUSB_UUID,
	.bcdVersion = 0x0100,
	.bVendorCode = WEBUSB_VENDOR_CODE,
	.iLandingPage = 1
};

const struct webusb_platform_descriptor webusb_platform_capability_descriptor_no_landing_page = {
	.bLength = WEBUSB_PLATFORM_DESCRIPTOR_SIZE,
	.bDescriptorType = USB_DT_DEVICE_CAPABILITY,
	.bDevCapabilityType = USB_DC_PLATFORM,
	.bReserved = 0,
	.platformCapabilityUUID = WEBUSB_UUID,
	.bcdVersion = 0x0100,
	.bVendorCode = WEBUSB_VENDOR_CODE,
	.iLandingPage = 0
};

static const char* webusb_https_url;

static int webusb_control_vendor_request(usbd_device *usbd_dev,
									 struct usb_setup_data *req,
									 uint8_t **buf, uint16_t *len,
									 usbd_control_complete_callback* complete) {
	//  Handle >>  type 0xc0, req WEBUSB_VENDOR_CODE, val 1, idx 2, type 0x00, index 0x01
	(void)complete;
	(void)usbd_dev;
	//  For WebUSB, only request types C0 and C1 are allowed.
	if (req->bmRequestType != 0xc0 && req->bmRequestType != 0xc1) { return USBD_REQ_NEXT_CALLBACK; }
	if (req->bRequest != WEBUSB_VENDOR_CODE) { return USBD_REQ_NEXT_CALLBACK; }
	dump_usb_request("web", req); ////
	int status = USBD_REQ_NOTSUPP;
	switch (req->wIndex) {
		case WEBUSB_REQ_GET_URL: {
			struct webusb_url_descriptor* url = (struct webusb_url_descriptor*)(*buf);
			uint16_t index = req->wValue;
			if (index == 0) {
    			debug_print("*** webusb notsupp index "); debug_print_unsigned(index); debug_println(""); debug_flush(); ////
				return USBD_REQ_NOTSUPP;
			}

			if (index == 1) {
				size_t url_len = strlen(webusb_https_url);
				url->bLength = WEBUSB_DT_URL_DESCRIPTOR_SIZE + url_len;
				url->bDescriptorType = WEBUSB_DT_URL;
				url->bScheme = WEBUSB_URL_SCHEME_HTTPS;
				memcpy(&url->URL, webusb_https_url, url_len);
				*len = MIN(*len, url->bLength);
				status = USBD_REQ_HANDLED;
			} else {
				// TODO: stall instead?
    			debug_print("*** webusb notsupp index "); debug_print_unsigned(index); debug_println(""); debug_flush(); ////
				status = USBD_REQ_NOTSUPP;
			}
			break;
		}
		default: {
    		debug_print("*** webusb notsupp wIndex "); debug_print_unsigned(req->wIndex); debug_println(""); debug_flush(); ////
			status = USBD_REQ_NOTSUPP;
			break;
		}
	}

	return status;
}

static void webusb_set_config(usbd_device* usbd_dev, uint16_t wValue) {
    debug_println("webusb_set_config"); // debug_flush(); ////
	(void)wValue;
	int status = aggregate_register_callback(
		usbd_dev,
		CONTROL_CALLBACK_TYPE,
		CONTROL_CALLBACK_MASK,
		webusb_control_vendor_request);
	if (status < 0) { debug_println("*** webusb_set_config failed"); debug_flush(); }
}

void webusb_setup(usbd_device* usbd_dev, const char* https_url) {
    // debug_println("webusb_setup"); // debug_flush(); ////
	webusb_https_url = https_url;

	//  Register the callback now because WebUSB requests come earlier.
	int status = aggregate_register_callback(
		usbd_dev,
		CONTROL_CALLBACK_TYPE,
		CONTROL_CALLBACK_MASK,
		webusb_control_vendor_request);
	if (status < 0) { debug_println("*** webusb_setup failed"); debug_flush(); }

    //  Re-register the callback in case the USB restarts.
	status = aggregate_register_config_callback(usbd_dev, webusb_set_config);
	if (status < 0) { debug_println("*** webusb_setup failed"); debug_flush(); }
}
