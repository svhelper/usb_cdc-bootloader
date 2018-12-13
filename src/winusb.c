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

#include <libopencm3/usb/usbd.h>
#include <logger.h>
#include "usb_conf.h"
#include "winusb.h"

#define CONTROL_CALLBACK_TYPE USB_REQ_TYPE_VENDOR
#define CONTROL_CALLBACK_MASK USB_REQ_TYPE_TYPE
#define DESCRIPTOR_CALLBACK_TYPE USB_REQ_TYPE_DEVICE
#define DESCRIPTOR_CALLBACK_MASK USB_REQ_TYPE_RECIPIENT

#define MIN(a, b) ({ typeof(a) _a = (a); typeof(b) _b = (b); _a < _b ? _a : _b; })

static int usb_descriptor_type(uint16_t wValue) {
	return wValue >> 8;
}

static int usb_descriptor_index(uint16_t wValue) {
	return wValue & 0xFF;
}

static struct winusb_compatible_id_descriptor winusb_wcid = {
	.header = {
		.dwLength = sizeof(struct winusb_compatible_id_descriptor_header) +
					1 * sizeof(struct winusb_compatible_id_function_section),
		.bcdVersion = WINUSB_BCD_VERSION,
		.wIndex = WINUSB_REQ_GET_COMPATIBLE_ID_FEATURE_DESCRIPTOR,
		.bNumSections = 1,
		.reserved = { 0, 0, 0, 0, 0, 0, 0 },
	},
	.functions = {
		{
			// note - bInterfaceNumber is rewritten in winusb_setup with the correct interface number
			.bInterfaceNumber = 0,
			.reserved0 = { 1 },
			.compatibleId = "WINUSB",
			.subCompatibleId = "",
			.reserved1 = { 0, 0, 0, 0, 0, 0}
		},
	}
};

static const struct usb_string_descriptor winusb_string_descriptor = {
	.bLength = 0x12,
	.bDescriptorType = USB_DT_STRING,
	.wData = WINUSB_EXTRA_STRING
};

static const struct winusb_extended_properties_descriptor guid = {
	.header = {
		.dwLength = sizeof(struct winusb_extended_properties_descriptor_header)
					+ 1 * sizeof (struct winusb_extended_properties_feature_descriptor),
		.bcdVersion = WINUSB_BCD_VERSION,
		.wIndex = WINUSB_REQ_GET_EXTENDED_PROPERTIES_OS_FEATURE_DESCRIPTOR,
		.wNumFeatures = 1,
	},
	.features = {
		{
			.dwLength = sizeof(struct winusb_extended_properties_feature_descriptor),
			.dwPropertyDataType = WINUSB_EXTENDED_PROPERTIES_MULTISZ_DATA_TYPE,
			.wNameLength = WINUSB_EXTENDED_PROPERTIES_GUID_NAME_SIZE_C,
			.name = WINUSB_EXTENDED_PROPERTIES_GUID_NAME,
			.dwPropertyDataLength = WINUSB_EXTENDED_PROPERTIES_GUID_DATA_SIZE_C,
			.propertyData = WINUSB_EXTENDED_PROPERTIES_GUID_DATA,
		},
	}
};

static int winusb_descriptor_request(usbd_device *usbd_dev,
					struct usb_setup_data *req,
					uint8_t **buf, uint16_t *len,
					usbd_control_complete_callback* complete) {
	(void)complete;
	(void)usbd_dev;
	dump_usb_request("winusb_descriptor", req); ////
	if ((req->bmRequestType & DESCRIPTOR_CALLBACK_MASK) != DESCRIPTOR_CALLBACK_TYPE) {
		////return USBD_REQ_NEXT_CALLBACK;  //  Not my callback type.  Hand off to next callback.
	}
	if ((req->bmRequestType & USB_REQ_TYPE_TYPE) != USB_REQ_TYPE_STANDARD) {
		return USBD_REQ_NEXT_CALLBACK;
	}
    debug_print("winusb_descriptor "); debug_print_unsigned(req->wIndex); debug_println(""); // debug_flush(); ////
	if (req->bRequest == USB_REQ_GET_DESCRIPTOR && usb_descriptor_type(req->wValue) == USB_DT_STRING) {
		if (usb_descriptor_index(req->wValue) == WINUSB_EXTRA_STRING_INDEX) {
			*buf = (uint8_t*)(&winusb_string_descriptor);
			*len = MIN(*len, winusb_string_descriptor.bLength);
			return USBD_REQ_HANDLED;
		}
	}
    debug_print("winusb_descriptor next "); debug_print_unsigned(req->bRequest); debug_println(""); debug_flush(); ////
	return USBD_REQ_NEXT_CALLBACK;
}

static int winusb_control_vendor_request(usbd_device *usbd_dev,
					struct usb_setup_data *req,
					uint8_t **buf, uint16_t *len,
					usbd_control_complete_callback* complete) {
	(void)complete;
	(void)usbd_dev;
	dump_usb_request("winusb_control", req); ////
	if ((req->bmRequestType & CONTROL_CALLBACK_MASK) != CONTROL_CALLBACK_TYPE) {
		////return USBD_REQ_NEXT_CALLBACK;  //  Not my callback type.  Hand off to next callback.
	}
	if (req->bRequest != WINUSB_MS_VENDOR_CODE) {
		return USBD_REQ_NEXT_CALLBACK;
	}
    if (req->wIndex != INTF_DFU) {
		//  Not for my interface.  Hand off to next interface.
        return USBD_REQ_NEXT_CALLBACK;
    }
    debug_print("winusb_control "); debug_print_unsigned(req->wIndex); debug_println(""); // debug_flush(); ////
	int status = USBD_REQ_NOTSUPP;
	if (((req->bmRequestType & USB_REQ_TYPE_RECIPIENT) == USB_REQ_TYPE_DEVICE) &&
		(req->wIndex == WINUSB_REQ_GET_COMPATIBLE_ID_FEATURE_DESCRIPTOR)) {
		*buf = (uint8_t*)(&winusb_wcid);
		*len = MIN(*len, winusb_wcid.header.dwLength);
		status = USBD_REQ_HANDLED;

	} else if (((req->bmRequestType & USB_REQ_TYPE_RECIPIENT) == USB_REQ_TYPE_INTERFACE) &&
		(req->wIndex == WINUSB_REQ_GET_EXTENDED_PROPERTIES_OS_FEATURE_DESCRIPTOR) &&
		(usb_descriptor_index(req->wValue) == winusb_wcid.functions[0].bInterfaceNumber)) {

		*buf = (uint8_t*)(&guid);
		*len = MIN(*len, guid.header.dwLength);
		status = USBD_REQ_HANDLED;

	} else {
		status = USBD_REQ_NOTSUPP;
		if ((req->bmRequestType & USB_REQ_TYPE_RECIPIENT) == USB_REQ_TYPE_DEVICE) {
    		debug_print("winusb NOTSUPP device "); debug_print_unsigned(req->wIndex); debug_println(""); debug_flush(); ////
		} else {
    		debug_print("winusb NOTSUPP iface "); debug_print_unsigned(usb_descriptor_index(req->wValue)); debug_println(""); debug_flush(); ////
		}
	}

	return status;
}

static void winusb_set_config(usbd_device* usbd_dev, uint16_t wValue) {
	debug_println("winusb_set_config"); // debug_flush(); ////
	(void)wValue;
	int status = usbd_register_control_callback(
		usbd_dev,
		CONTROL_CALLBACK_TYPE,
		CONTROL_CALLBACK_MASK,
		winusb_control_vendor_request);
	if (status < 0) {
    	debug_println("*** winusb_set_config failed"); debug_flush(); ////
	}
}

void winusb_setup(usbd_device* usbd_dev, uint8_t interface) {
	// debug_println("winusb_setup"); // debug_flush(); ////
	winusb_wcid.functions[0].bInterfaceNumber = interface;

	usbd_register_set_config_callback(usbd_dev, winusb_set_config);

	/* Windows probes the compatible ID before setting the configuration,
	   so also register the callback now */

	int status1 = usbd_register_control_callback(
		usbd_dev,
		CONTROL_CALLBACK_TYPE,
		CONTROL_CALLBACK_MASK,
		winusb_control_vendor_request);

	int status2 = usbd_register_control_callback(
		usbd_dev,
		DESCRIPTOR_CALLBACK_TYPE,
		DESCRIPTOR_CALLBACK_MASK,
		winusb_descriptor_request);

	if (status1 < 0 || status2 < 0) {
    	debug_println("*** winusb_setup failed"); debug_flush(); ////
	}
}

