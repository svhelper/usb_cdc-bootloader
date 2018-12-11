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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/dfu.h>
#include <libopencm3/usb/msc.h>
#include <logger.h>
#include "target.h"
#include "dfu.h"
#include "webusb.h"
#include "winusb.h"
#include "usb21_standard.h"
#include "usb_conf.h"
#include "uf2.h"

static const char* origin_url = "lupyuen.github.io/pxt-maker";
// static const char* origin_url = "trezor.io/start";

static const struct usb_device_descriptor dev = {
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0210,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = 64,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0110,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

static const struct usb_interface_descriptor dfu_iface = {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = INTF_DFU,
    .bAlternateSetting = 0,
    .bNumEndpoints = 0,
    .bInterfaceClass = 0xFE,
    .bInterfaceSubClass = 1,
    .bInterfaceProtocol = 2,
    .iInterface = 4,

    .endpoint = NULL,

    .extra = &dfu_function,
    .extralen = sizeof(dfu_function),
};

static const struct usb_endpoint_descriptor msc_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x01,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = 64,
	.bInterval = 0,
}, {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x82,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = 64,
	.bInterval = 0,
}};

static const struct usb_interface_descriptor msc_iface = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = INTF_MSC,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_MSC,
	.bInterfaceSubClass = USB_MSC_SUBCLASS_SCSI,
	.bInterfaceProtocol = USB_MSC_PROTOCOL_BBB,
	.iInterface = 0,
	.endpoint = msc_endp,
	.extra = NULL,
	.extralen = 0
};

static const struct usb_interface interfaces[] = {
    /* DFU interface */
    {
        .num_altsetting = 1,
        .altsetting = &dfu_iface,
    },
    {
        .num_altsetting = 1,
        .altsetting = &msc_iface,
    },    
};

static const struct usb_config_descriptor config = {
    .bLength = USB_DT_CONFIGURATION_SIZE,
    .bDescriptorType = USB_DT_CONFIGURATION,
    .wTotalLength = 0,
    .bNumInterfaces = sizeof(interfaces)/sizeof(struct usb_interface),
    .bConfigurationValue = 1,
    .iConfiguration = 0,
    .bmAttributes = 0xC0,
    .bMaxPower = 0x32,

    .interface = interfaces,
};

static char serial_number[USB_SERIAL_NUM_LENGTH+1];

static const char *usb_strings[] = {
    "Devanarchy",
    "DAPBoot DFU Bootloader",
    serial_number,
    "DAPBoot DFU"
};

/* Buffer to be used for control requests. */
static uint8_t usbd_control_buffer[USB_CONTROL_BUF_SIZE] __attribute__ ((aligned (2)));
//  TODO: static uint8_t usbd_control_buffer[256] __attribute__ ((aligned (2)));

void usb_set_serial_number(const char* serial) {
    serial_number[0] = '\0';
    if (serial) {
        strncpy(serial_number, serial, USB_SERIAL_NUM_LENGTH);
        serial_number[USB_SERIAL_NUM_LENGTH] = '\0';
    }
}

static const struct usb_device_capability_descriptor* capabilities[] = {
	(const struct usb_device_capability_descriptor*) &webusb_platform_capability_descriptor,
};

static const struct usb_bos_descriptor bos_descriptor = {
	.bLength = USB_DT_BOS_SIZE,
	.bDescriptorType = USB_DT_BOS,
	.bNumDeviceCaps = sizeof(capabilities) / sizeof(capabilities[0]),
	.capabilities = capabilities
};

usbd_device* usb_setup(void) {
    debug_println("usb_setup"); debug_flush(); ////
    int num_strings = sizeof(usb_strings)/sizeof(const char*);
    const usbd_driver* driver = target_usb_init();
    usbd_device* usbd_dev = usbd_init(driver, &dev, &config, 
        usb_strings, num_strings,
        usbd_control_buffer, sizeof(usbd_control_buffer));
    
    debug_println("bootloader usb_msc_init");  debug_flush();
    usb_msc_init(usbd_dev, 0x82, 64, 0x01, 64, "Example Ltd", "UF2 Bootloader",
	    "42.00", UF2_NUM_BLOCKS, read_block, write_block);
    debug_println("bootloader usb_msc_init done");  debug_flush();        

    ////debug_println("bootloader dfu_setup");  debug_flush();
    ////dfu_setup(usbd_dev, &target_manifest_app, NULL, NULL);

	////usb21_setup(usbd_dev, &bos_descriptor);
	////webusb_setup(usbd_dev, origin_url);
	////winusb_setup(usbd_dev, 0);
    return usbd_dev;
}

/* Previously:                                      
usbd_register_set_config_callback(usbd_dev, set_config);

static const struct usb_device_capability_descriptor* capabilities[] = {
    (const struct usb_device_capability_descriptor*)&webusb_platform,
};

static const struct usb_bos_descriptor bos = {
    .bLength = USB_DT_BOS_SIZE,
    .bDescriptorType = USB_DT_BOS,
    .wTotalLength = USB_DT_BOS_SIZE + sizeof(webusb_platform),
    .bNumDeviceCaps = sizeof(capabilities)/sizeof(capabilities[0]),
    .capabilities = capabilities
};

static void set_config(usbd_device *dev, uint16_t wValue) {
	(void)wValue;
	usbd_ep_setup(dev, ENDPOINT_ADDRESS_IN,  USB_ENDPOINT_ATTR_INTERRUPT, 64, 0);
	usbd_ep_setup(dev, ENDPOINT_ADDRESS_OUT, USB_ENDPOINT_ATTR_INTERRUPT, 64, rx_callback);
}

usbd_dev = usbd_init(&otgfs_usb_driver, &dev_descr, &config, 
    usb_strings, sizeof(usb_strings)/sizeof(const char *), 
    usbd_control_buffer, sizeof(usbd_control_buffer));
usbd_device* usbd_dev = usbd_init(driver, &dev, &config, &bos,
    usb_strings, num_strings,
    usbd_control_buffer, sizeof(usbd_control_buffer)); */
