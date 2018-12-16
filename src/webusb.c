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
#include "winusb.h"  //  For WINUSB_MS_VENDOR_CODE
#include "usb21_standard.h"

#define CONTROL_CALLBACK_TYPE (USB_REQ_TYPE_VENDOR | USB_REQ_TYPE_DEVICE)
#define CONTROL_CALLBACK_MASK (USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT)

#define MIN(a, b) ({ typeof(a) _a = (a); typeof(b) _b = (b); _a < _b ? _a : _b; })

/*
Test WebUSB in Chrome console:
navigator.usb.getDevices().then(console.log)

Captured BOS from Blue Pill:

Binary Object Store descriptor
05:0f:39:00:02:

WebUSB Platform Capability Descriptor: https://wicg.github.io/webusb/#webusb-platform-capability-descriptor
bLength: 18:
bDescriptorType: 10:
bDevCapabilityType: 05:
bReserved: 00:
PlatformCapability UUID:
platformCapabilityUUID: 38:b6:08:34:a9:09:a0:47:8b:fd:a0:76:88:15:b6:65:
bcdVersion: 00:01:
bVendorCode: 22:
iLandingPage: 01:

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

Captured BOS from microbit:

Binary Object Store descriptor
05:0f:39:00:02:

WebUSB Platform Capability Descriptor: https://wicg.github.io/webusb/#webusb-platform-capability-descriptor
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
aa:00:20:00 */

//  WebUSB Descriptor with landing page.
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

//  WebUSB Descriptor without landing page.
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

//  Microsoft Platform Descriptor.  From http://download.microsoft.com/download/3/5/6/3563ED4A-F318-4B66-A181-AB1D8F6FD42D/MS_OS_2_0_desc.docx
const struct microsoft_platform_descriptor microsoft_platform_capability_descriptor = {
	.bLength = MICROSOFT_PLATFORM_DESCRIPTOR_SIZE,
	.bDescriptorType = USB_DT_DEVICE_CAPABILITY,
	.bDevCapabilityType = USB_DC_PLATFORM,
	.bReserved = 0,
	.platformCapabilityUUID = MSOS20_PLATFORM_UUID,
	.dwWindowsVersion = MSOS20_WINDOWS_VERSION,  //  Windows version e.g. 0x00, 0x00, 0x03, 0x06
	.wMSOSDescriptorSetTotalLength = MSOS20_DESCRIPTOR_SET_SIZE, //  Descriptor set length e.g. 0xb2
	.bMS_VendorCode = WINUSB_MS_VENDOR_CODE,     //  Vendor code e.g. 0x21.  Host will call WinUSB to fetch descriptor.
	.bAltEnumCode = 0  //  Alternate enumeration code e.g. 0x00
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
				dump_usb_request("weburl", req); debug_flush(); ////
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
    //  debug_println("webusb_set_config"); // debug_flush(); ////
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
