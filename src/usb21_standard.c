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

#include <stdint.h>
#include <string.h>
#include <logger.h>
#include "usb_conf.h"
#include "usb21_standard.h"

#define DESCRIPTOR_CALLBACK_TYPE (USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE)
#define DESCRIPTOR_CALLBACK_MASK (USB_REQ_TYPE_DIRECTION | USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT)

#define MIN(a, b) ({ typeof(a) _a = (a); typeof(b) _b = (b); _a < _b ? _a : _b; })

static uint16_t build_bos_descriptor(const struct usb_bos_descriptor *bos,
									 uint8_t *buf, uint16_t len)
{
	uint8_t *tmpbuf = buf;
	uint16_t count, total = 0, totallen = 0;
	uint16_t i;

	memcpy(buf, bos, count = MIN(len, bos->bLength));
	buf += count;
	len -= count;
	total += count;
	totallen += bos->bLength;

	/* For each device capability */
	for (i = 0; i < bos->bNumDeviceCaps; i++) {
		/* Copy device capability descriptor. */
		const struct usb_device_capability_descriptor *cap =
			bos->capabilities[i];

		memcpy(buf, cap, count = MIN(len, cap->bLength));
		buf += count;
		len -= count;
		total += count;
		totallen += cap->bLength;
	}

	/* Fill in wTotalLength. */
	*(uint16_t *)(tmpbuf + 2) = totallen;

	return total;
}

static int usb_descriptor_type(uint16_t wValue) {
	return wValue >> 8;
}

static int usb_descriptor_index(uint16_t wValue) {
	return wValue & 0xFF;
}

static const struct usb_bos_descriptor* usb21_bos;

static int usb21_standard_get_descriptor(usbd_device* usbd_dev,
											struct usb_setup_data *req,
											uint8_t **buf, uint16_t *len,
											usbd_control_complete_callback* complete) {
	(void)complete;
	(void)usbd_dev;
	dump_usb_request("usb21_descriptor", req); ////
	if ((req->bmRequestType & DESCRIPTOR_CALLBACK_MASK) != DESCRIPTOR_CALLBACK_TYPE) {
		////return USBD_REQ_NEXT_CALLBACK;  //  Not my callback type.  Hand off to next callback.
	}
	int descr_type = req->wValue >> 8;
    if (descr_type != USB_DT_BOS) {
		//  Not BOS request.  Hand off to next interface.
        return USBD_REQ_NEXT_CALLBACK;
    }
	if (!usb21_bos) {
		debug_println("*** usb21_descriptor no bos "); debug_flush(); ////
		return USBD_REQ_NOTSUPP;
	}
	debug_print("usb21_descriptor "); debug_print_unsigned(req->bRequest); 
    debug_print(", type "); debug_print_unsigned(usb_descriptor_type(req->wValue)); 	
    debug_print(", index "); debug_print_unsigned(usb_descriptor_index(req->wValue)); 	
	debug_println(""); // debug_flush(); ////
	if (req->bRequest == USB_REQ_GET_DESCRIPTOR) {
		*len = MIN(*len, build_bos_descriptor(usb21_bos, *buf, *len));
		return USBD_REQ_HANDLED;
	}
	debug_print("usb21_descriptor next "); debug_print_unsigned(req->bRequest); 
    debug_print(", type "); debug_print_unsigned(usb_descriptor_type(req->wValue)); 	
    debug_print(", index "); debug_print_unsigned(usb_descriptor_index(req->wValue)); 	
	debug_println(""); // debug_flush(); ////
	return USBD_REQ_NEXT_CALLBACK;
}

static void usb21_set_config(usbd_device* usbd_dev, uint16_t wValue) {
    debug_println("usb21_set_config"); // debug_flush(); ////
	(void)wValue;
	int status = aggregate_register_callback(
		usbd_dev,
		DESCRIPTOR_CALLBACK_TYPE,
		DESCRIPTOR_CALLBACK_MASK,
		&usb21_standard_get_descriptor);
	if (status < 0) {
    	debug_println("*** usb21_set_config failed"); debug_flush(); ////
	}
}

void usb21_setup(usbd_device* usbd_dev, const struct usb_bos_descriptor* binary_object_store) {
    // debug_println("usb21_setup"); // debug_flush(); ////
	usb21_bos = binary_object_store;

	/* Register the control request handler _before_ the config is set */
	usb21_set_config(usbd_dev, 0x0000);
	usbd_register_set_config_callback(usbd_dev, usb21_set_config);
}
