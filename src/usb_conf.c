//  CDC code from https://github.com/Apress/Beg-STM32-Devel-FreeRTOS-libopencm3-GCC/blob/master/rtos/usbcdcdemo/usbcdc.c
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
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/dfu.h>
#include <libopencm3/usb/msc.h>
#include <libopencm3/usb/cdc.h>
#include <logger.h>
#include "target.h"
#include "dfu.h"
#include "cdc.h"
#include "webusb.h"
#include "winusb.h"
#include "usb21_standard.h"
#include "usb_conf.h"
#include "uf2.h"

static void set_aggregate_callback(
  usbd_device *usbd_dev,
  uint16_t wValue
);

static const char* origin_url = "visualbluepill.github.io";

static char serial_number[USB_SERIAL_NUM_LENGTH+1];

static const char *usb_strings[] = {
    "Devanarchy",              //  USB Manufacturer
    "DAPBoot DFU Bootloader",  //  USB Product
    serial_number,             //  Serial number
    //"Blue Pill DFU"            //  DFU
    "DAPBoot DFU"
};

#define MSC_VENDOR_ID "BluePill"
#define MSC_PRODUCT_ID "UF2 Bootloader"
#define MSC_PRODUCT_REVISION_LEVEL "42.00"

enum usb_strings_index {  //  Index of USB strings.  Must sync with above, starts from 1.
    USB_STRINGS_MANUFACTURER = 1,
    USB_STRINGS_PRODUCT,
    USB_STRINGS_SERIAL_NUMBER,
    USB_STRINGS_DFU,
};

//  USB Device
static const struct usb_device_descriptor dev = {
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0210,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = MAX_USB_PACKET_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0210,  //  Release number 2.1
    .iManufacturer = USB_STRINGS_MANUFACTURER,
    .iProduct = USB_STRINGS_PRODUCT,
    .iSerialNumber = USB_STRINGS_SERIAL_NUMBER,
    .bNumConfigurations = 1,
};

//  MSC Endpoints
static const struct usb_endpoint_descriptor msc_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = MSC_OUT,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = MAX_USB_PACKET_SIZE,
	.bInterval = 0,
}, {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = MSC_IN,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = MAX_USB_PACKET_SIZE,
	.bInterval = 0,
}};

//  CDC Endpoints

/*
 * This notification endpoint isn't implemented. According to CDC spec it's
 * optional, but its absence causes a NULL pointer dereference in the
 * Linux cdc_acm driver. (Gareth McMullin <gareth@blacksphere.co.nz>)
 */
static const struct usb_endpoint_descriptor comm_endp[] = {
	{
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bEndpointAddress = COMM_IN,
		.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
		.wMaxPacketSize = COMM_PACKET_SIZE,  //  Smaller than others
		.bInterval = 255,
	}
};

static const struct usb_endpoint_descriptor data_endp[] = {
	{
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bEndpointAddress = DATA_OUT,
		.bmAttributes = USB_ENDPOINT_ATTR_BULK,
		.wMaxPacketSize = MAX_USB_PACKET_SIZE,
		.bInterval = 1,
	}, {
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bEndpointAddress = DATA_IN,
		.bmAttributes = USB_ENDPOINT_ATTR_BULK,
		.wMaxPacketSize = MAX_USB_PACKET_SIZE,
		.bInterval = 1,
	}
};

static const struct {
	struct usb_cdc_header_descriptor header;
	struct usb_cdc_call_management_descriptor call_mgmt;
	struct usb_cdc_acm_descriptor acm;
	struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm_functional_descriptors = {
	.header = {
		.bFunctionLength = sizeof(struct usb_cdc_header_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_HEADER,
		.bcdCDC = 0x0110,
	},
	.call_mgmt = {
		.bFunctionLength =
			sizeof(struct usb_cdc_call_management_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
		.bmCapabilities = 0,
		.bDataInterface = INTF_DATA,  //  Was 1
	},
	.acm = {
		.bFunctionLength = sizeof(struct usb_cdc_acm_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_ACM,
		.bmCapabilities = 0,
	},
	.cdc_union = {
		.bFunctionLength = sizeof(struct usb_cdc_union_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_UNION,
		.bControlInterface = INTF_COMM,  //  Was 0
		.bSubordinateInterface0 = INTF_DATA,  //  Was 1
	 }
};

//  DFU Interface
static const struct usb_interface_descriptor dfu_iface = {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = INTF_DFU,
    .bAlternateSetting = 0,
    .bNumEndpoints = 0,
    .bInterfaceClass = 0xFE,
    .bInterfaceSubClass = 1,
    .bInterfaceProtocol = 2,
    .iInterface = USB_STRINGS_DFU,  //  Name of DFU
    .endpoint = NULL,
    .extra = &dfu_function,
    .extralen = sizeof(dfu_function),
};

//  MSC Interface
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
	.endpoint = msc_endp,  //  MSC Endpoints
	.extra = NULL,
	.extralen = 0
};

//  CDC Interfaces
static const struct usb_interface_descriptor comm_iface = {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = INTF_COMM,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,
    .bInterfaceClass = USB_CLASS_CDC,
    .bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
    .bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
    .iInterface = 0,
    .endpoint = comm_endp,  //  COMM Endpoint
    .extra = &cdcacm_functional_descriptors,
    .extralen = sizeof(cdcacm_functional_descriptors)
};

static const struct usb_interface_descriptor data_iface = {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = INTF_DATA,
    .bAlternateSetting = 0,
    .bNumEndpoints = 2,
    .bInterfaceClass = USB_CLASS_DATA,
    .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 0,
    .iInterface = 0,
    .endpoint = data_endp,  //  DATA Endpoints
};

//  All USB Interfaces
static const struct usb_interface interfaces[] = {{
    .num_altsetting = 1,
    .altsetting = &dfu_iface,  //  Index must sync with INTF_DFU.
}, {
    .num_altsetting = 1,
    .altsetting = &msc_iface,  //  Index must sync with INTF_MSC.
}, 	{
    .num_altsetting = 1,
    .altsetting = &comm_iface,  //  Index must sync with INTF_COMM.
}, {
    .num_altsetting = 1,
    .altsetting = &data_iface,  //  Index must sync with INTF_DATA.
}};

//  USB Config
static const struct usb_config_descriptor config = {
    .bLength = USB_DT_CONFIGURATION_SIZE,
    .bDescriptorType = USB_DT_CONFIGURATION,
    .wTotalLength = 0,
    .bNumInterfaces = sizeof(interfaces) / sizeof(struct usb_interface),
    .bConfigurationValue = 1,
    .iConfiguration = 0,
    .bmAttributes = 0x80,  //  Bus-powered, i.e. it draws power from USB bus.
    .bMaxPower = 0x32,
    .interface = interfaces,
};

//  BOS Capabilities for WebUSB
static const struct usb_device_capability_descriptor* capabilities[] = {
	(const struct usb_device_capability_descriptor*) 
        &webusb_platform_capability_descriptor,
};

//  BOS Descriptor for WebUSB
static const struct usb_bos_descriptor bos_descriptor = {
	.bLength = USB_DT_BOS_SIZE,
	.bDescriptorType = USB_DT_BOS,
	.bNumDeviceCaps = sizeof(capabilities) / sizeof(capabilities[0]),
	.capabilities = capabilities
};

/* Buffer to be used for control requests. */
static uint8_t usbd_control_buffer[USB_CONTROL_BUF_SIZE] __attribute__ ((aligned (2)));
usbd_device* usbd_dev = NULL;

usbd_device* usb_setup(void) {
    int num_strings = sizeof(usb_strings)/sizeof(const char*);
    // debug_print("usb_setup num_strings "); debug_print_int(num_strings); debug_println(""); // debug_flush(); ////
    const usbd_driver* driver = target_usb_init();
    usbd_dev = usbd_init(driver, &dev, &config, 
        usb_strings, num_strings,
        usbd_control_buffer, sizeof(usbd_control_buffer));

    //  Set the aggregate callback.    
	usbd_register_set_config_callback(usbd_dev, set_aggregate_callback);
    //  For WinUSB: Windows probes the compatible ID before setting the configuration, so also register the callback now.
    set_aggregate_callback(usbd_dev, 0);

    //  The following USB setup functions will call aggregate_register_callback() to register callbacks.
    dfu_setup(usbd_dev, &target_manifest_app, NULL, NULL);
    msc_setup(usbd_dev);
    cdc_setup(usbd_dev);
	usb21_setup(usbd_dev, &bos_descriptor);
	webusb_setup(usbd_dev, origin_url);
	winusb_setup(usbd_dev, INTF_DFU);
    return usbd_dev;
}

extern usbd_mass_storage *custom_usb_msc_init(usbd_device *usbd_dev,
				 uint8_t ep_in, uint8_t ep_in_size,
				 uint8_t ep_out, uint8_t ep_out_size,
				 const char *vendor_id,
				 const char *product_id,
				 const char *product_revision_level,
				 const uint32_t block_count,
				 int (*read_block)(uint32_t lba, uint8_t *copy_to),
				 int (*write_block)(uint32_t lba, const uint8_t *copy_from),
				 uint8_t msc_interface_index0);

void msc_setup(usbd_device* usbd_dev0) {
    //  debug_println("msc_setup"); ////
#ifdef RAM_DISK
    ramdisk_init();
#endif  //  RAM_DISK
    
    custom_usb_msc_init(usbd_dev0, MSC_IN, MAX_USB_PACKET_SIZE, MSC_OUT, MAX_USB_PACKET_SIZE, 
        MSC_VENDOR_ID, MSC_PRODUCT_ID, MSC_PRODUCT_REVISION_LEVEL, 
#ifdef RAM_DISK    
        ramdisk_blocks(), ramdisk_read, ramdisk_write,
#else
        UF2_NUM_BLOCKS, read_block, write_block,
#endif  //  RAM_DISK        
        INTF_MSC
    );
}

struct control_callback_struct {
    uint8_t type;
    uint8_t type_mask;
    usbd_control_callback cb;
};

#define MAX_CONTROL_CALLBACK 10
static struct control_callback_struct control_callback[MAX_CONTROL_CALLBACK];

/* Register application callback function for handling USB control requests.  We aggregate here so we can handle more than 4 callbacks.  */
int aggregate_register_callback(
    usbd_device *usbd_dev, 
    uint8_t type,
    uint8_t type_mask,
    usbd_control_callback callback)
{
    // debug_println("aggregate_register_callback"); ////
	int i;
	for (i = 0; i < MAX_CONTROL_CALLBACK; i++) {
		if (control_callback[i].cb) { 
            //  If already exists, skip.
            if (control_callback[i].type == type &&
                control_callback[i].type_mask == type_mask &&
                control_callback[i].cb == callback) { 
                    //  debug_println("callback exists"); ////
                    return 0;
                }
            continue;  //  Continue checking.
        }
		control_callback[i].type = type;
		control_callback[i].type_mask = type_mask;
		control_callback[i].cb = callback;
		return 0;
	}
    debug_println("*** ERROR: Too many control callbacks"); debug_flush();
	return -1;
}

static int aggregate_callback(
    usbd_device *usbd_dev,
	struct usb_setup_data *req, 
    uint8_t **buf, 
    uint16_t *len,
	usbd_control_complete_callback *complete) {
	int i, result = 0;
	//  dump_usb_request(">>", req); ////
	/* Call user command hook function. */
	for (i = 0; i < MAX_CONTROL_CALLBACK; i++) {
		if (control_callback[i].cb == NULL) { break; }
		if ((req->bmRequestType & control_callback[i].type_mask) == control_callback[i].type) {
			result = control_callback[i].cb(
                usbd_dev, 
                req,
                buf,
                len,
                complete);
			if (result == USBD_REQ_HANDLED ||
			    result == USBD_REQ_NOTSUPP) {
				return result;
			}
		}
	}
	dump_usb_request(">> ***", req); debug_flush(); ////
	return USBD_REQ_NEXT_CALLBACK;
}

static void set_aggregate_callback(
  usbd_device *usbd_dev,
  uint16_t wValue
) {
    // debug_println("set_aggregate_callback"); ////
#ifdef NOTUSED
    int status = usbd_register_control_callback(
		usbd_dev,
        0,  //  All types
        0,  //  All masks
		aggregate_callback);
	if (status < 0) {
    	debug_println("*** ERROR: set_aggregate_callback failed"); debug_flush();
	}    
#else
	int status = usbd_register_control_callback(
		usbd_dev,
        USB_REQ_TYPE_VENDOR | USB_REQ_TYPE_DEVICE,
        USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
		aggregate_callback);
	if (status < 0) {
    	debug_println("*** ERROR: set_aggregate_callback failed"); debug_flush();
	}    
    status = usbd_register_control_callback(
        usbd_dev,
        USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE,
        USB_REQ_TYPE_DIRECTION | USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
        aggregate_callback);        
	if (status < 0) {
    	debug_println("*** ERROR: set_aggregate_callback failed"); debug_flush();
	}
    status = usbd_register_control_callback(
        usbd_dev,
        USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
        USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
        aggregate_callback);        
	if (status < 0) {
    	debug_println("*** ERROR: set_aggregate_callback failed"); debug_flush();
	}
#endif  //  NOTUSED    
}

void usb_set_serial_number(const char* serial) {
    serial_number[0] = '\0';
    if (serial) {
        strncpy(serial_number, serial, USB_SERIAL_NUM_LENGTH);
        serial_number[USB_SERIAL_NUM_LENGTH] = '\0';
    }
}

static int usb_descriptor_type(uint16_t wValue) {
	return wValue >> 8;
}

static int usb_descriptor_index(uint16_t wValue) {
	return wValue & 0xFF;
}

void dump_usb_request(const char *msg, struct usb_setup_data *req) {
    debug_print(msg);
    debug_print(" type 0x"); debug_printhex(req->bmRequestType);
    debug_print(", req 0x"); debug_printhex(req->bRequest);
    debug_print(", val "); debug_print_unsigned(req->wValue);
    debug_print(", idx "); debug_print_unsigned(req->wIndex);
    debug_print(", type 0x"); debug_printhex(usb_descriptor_type(req->wValue)); 	
	debug_print(", index 0x"); debug_printhex(usb_descriptor_index(req->wValue)); 	
    debug_println("");
}

/*
> Executing task in folder bluepill-bootloader: openocd -f interface/stlink-v2.cfg -f target/stm32f1x.cfg -f scripts/connect.ocd <

Open On-Chip Debugger 0.10.0
Licensed under GNU GPL v2
For bug reports, read
        http://openocd.org/doc/doxygen/bugs.html
Info : auto-selecting first available session transport "hla_swd". To override use 'transport select <transport>'.
Info : The selected transport took over low-level target control. The results might differ compared to plain JTAG/SWD
adapter speed: 1000 kHz
adapter_nsrst_delay: 100
none separate
debug_level: 0
semihosting is enabled
NOTE: Trash this window before uploading a program to the Blue Pill
Restarting the Blue Pill...
----platform_setup
----bootloader
bootloader target_get_force_bootloader
bootloader target_get_serial_number
bootloader usb_set_serial_number
bootloader usb_setup
usb_setup num_strings 4
set_aggregate_callback
msc_setup
custom_usb_msc_init
*** cdc_setup
usb21_set_config
aggregate_register_callback
aggregate_register_callback
aggregate_register_callback
bootloader loop
>> type 0x80, req 0x06, val 256, idx 0
usb21_descriptor type 0x80, req 0x06, val 256, idx 0
winusb_descriptor type 0x80, req 0x06, val 256, idx 0
winusb_descriptor 0
winusb_descriptor next 6, type 1, index 0
>> *** type 0x80, req 0x06, val 256, idx 0
>> type 0x80, req 0x06, val 256, idx 0
usb21_descriptor type 0x80, req 0x06, val 256, idx 0
winusb_descriptor type 0x80, req 0x06, val 256, idx 0
winusb_descriptor 0
winusb_descriptor next 6, type 1, index 0
>> *** type 0x80, req 0x06, val 256, idx 0
>> type 0x80, req 0x06, val 770, idx 1033
usb21_descriptor type 0x80, req 0x06, val 770, idx 1033
winusb_descriptor type 0x80, req 0x06, val 770, idx 1033
winusb_descriptor 1033
winusb_descriptor next 6, type 3, index 2
>> *** type 0x80, req 0x06, val 770, idx 1033
>> type 0x80, req 0x06, val 770, idx 1033
usb21_descriptor type 0x80, req 0x06, val 770, idx 1033
winusb_descriptor type 0x80, req 0x06, val 770, idx 1033
winusb_descriptor 1033
winusb_descriptor next 6, type 3, index 2
>> *** type 0x80, req 0x06, val 770, idx 1033
>> type 0x80, req 0x06, val 769, idx 1033
usb21_descriptor type 0x80, req 0x06, val 769, idx 1033
winusb_descriptor type 0x80, req 0x06, val 769, idx 1033
winusb_descriptor 1033
winusb_descriptor next 6, type 3, index 1
>> *** type 0x80, req 0x06, val 769, idx 1033
>> type 0x80, req 0x06, val 769, idx 1033
usb21_descriptor type 0x80, req 0x06, val 769, idx 1033
winusb_descriptor type 0x80, req 0x06, val 769, idx 1033
winusb_descriptor 1033
winusb_descriptor next 6, type 3, index 1
>> *** type 0x80, req 0x06, val 769, idx 1033
>> type 0x80, req 0x06, val 771, idx 1033
usb21_descriptor type 0x80, req 0x06, val 771, idx 1033
winusb_descriptor type 0x80, req 0x06, val 771, idx 1033
winusb_descriptor 1033
winusb_descriptor next 6, type 3, index 3
>> *** type 0x80, req 0x06, val 771, idx 1033
>> type 0x80, req 0x06, val 771, idx 1033
usb21_descriptor type 0x80, req 0x06, val 771, idx 1033
winusb_descriptor type 0x80, req 0x06, val 771, idx 1033
winusb_descriptor 1033
winusb_descriptor next 6, type 3, index 3
>> *** type 0x80, req 0x06, val 771, idx 1033
>> type 0x80, req 0x06, val 3840, idx 0
usb21_descriptor type 0x80, req 0x06, val 3840, idx 0
usb21_descriptor 6, type 15, index 0
>> type 0x80, req 0x06, val 3840, idx 0
usb21_descriptor type 0x80, req 0x06, val 3840, idx 0
usb21_descriptor 6, type 15, index 0
>> type 0x80, req 0x06, val 512, idx 0
usb21_descriptor type 0x80, req 0x06, val 512, idx 0
winusb_descriptor type 0x80, req 0x06, val 512, idx 0
winusb_descriptor 0
winusb_descriptor next 6, type 2, index 0
>> *** type 0x80, req 0x06, val 512, idx 0
>> type 0x80, req 0x06, val 512, idx 0
usb21_descriptor type 0x80, req 0x06, val 512, idx 0
winusb_descriptor type 0x80, req 0x06, val 512, idx 0
winusb_descriptor 0
winusb_descriptor next 6, type 2, index 0
>> *** type 0x80, req 0x06, val 512, idx 0
>> type 0x80, req 0x06, val 768, idx 0
usb21_descriptor type 0x80, req 0x06, val 768, idx 0
winusb_descriptor type 0x80, req 0x06, val 768, idx 0
winusb_descriptor 0
winusb_descriptor next 6, type 3, index 0
>> *** type 0x80, req 0x06, val 768, idx 0
>> type 0xc0, req 0x01, val 1, idx 2
winusb_control type 0xc0, req 0x01, val 1, idx 2
winusb_descriptor type 0xc0, req 0x01, val 1, idx 2
>> *** type 0xc0, req 0x01, val 1, idx 2
set_aggregate_callback
dfu_set_config
aggregate_register_callback
msc_set_config
aggregate_register_callback
*** cdcacm_set_config
>> type 0x80, req 0x06, val 772, idx 1033
usb21_descriptor type 0x80, req 0x06, val 772, idx 1033
winusb_descriptor type 0x80, req 0x06, val 772, idx 1033
winusb_descriptor 1033
winusb_descriptor next 6, type 3, index 4
>> *** type 0x80, req 0x06, val 772, idx 1033
>> type 0x80, req 0x06, val 772, idx 1033
usb21_descriptor type 0x80, req 0x06, val 772, idx 1033
winusb_descriptor type 0x80, req 0x06, val 772, idx 1033
winusb_descriptor 1033
winusb_descriptor next 6, type 3, index 4
>> *** type 0x80, req 0x06, val 772, idx 1033
>> type 0xa1, req 0xfe, val 0, idx 1
dfu_control type 0xa1, req 0xfe, val 0, idx 1
msc_control type 0xa1, req 0xfe, val 0, idx 1
SBC_SENSE_KEY_ILLEGAL_REQUEST 1e
SBC_SENSE_KEY_ILLEGAL_REQUEST 1e
>> type 0x21, req 0x22, val 3, idx 2
dfu_control type 0x21, req 0x22, val 3, idx 2
msc_control type 0x21, req 0x22, val 3, idx 2
*** msc_control next 34
>> *** type 0x21, req 0x22, val 3, idx 2
*** cdcacm_control type 0x21, req 0x22, val 3, idx 2
*** cdcacm_control 34
>> type 0x21, req 0x20, val 0, idx 2
dfu_control type 0x21, req 0x20, val 0, idx 2
msc_control type 0x21, req 0x20, val 0, idx 2
*** msc_control next 32
>> *** type 0x21, req 0x20, val 0, idx 2
*** cdcacm_control type 0x21, req 0x20, val 0, idx 2
*** cdcacm_control 32
>> type 0x21, req 0x20, val 0, idx 2
dfu_control type 0x21, req 0x20, val 0, idx 2
msc_control type 0x21, req 0x20, val 0, idx 2
*** msc_control next 32
>> *** type 0x21, req 0x20, val 0, idx 2
*** cdcacm_control type 0x21, req 0x20, val 0, idx 2
*** cdcacm_control 32
>> type 0x21, req 0x20, val 0, idx 2
dfu_control type 0x21, req 0x20, val 0, idx 2
msc_control type 0x21, req 0x20, val 0, idx 2
*** msc_control next 32
>> *** type 0x21, req 0x20, val 0, idx 2
*** cdcacm_control type 0x21, req 0x20, val 0, idx 2
*** cdcacm_control 32
>> type 0x21, req 0x20, val 0, idx 2
dfu_control type 0x21, req 0x20, val 0, idx 2
msc_control type 0x21, req 0x20, val 0, idx 2
*** msc_control next 32
>> *** type 0x21, req 0x20, val 0, idx 2
*** cdcacm_control type 0x21, req 0x20, val 0, idx 2
*** cdcacm_control 32
>> type 0x21, req 0x20, val 0, idx 2
dfu_control type 0x21, req 0x20, val 0, idx 2
msc_control type 0x21, req 0x20, val 0, idx 2
*** msc_control next 32
>> *** type 0x21, req 0x20, val 0, idx 2
*** cdcacm_control type 0x21, req 0x20, val 0, idx 2
*** cdcacm_control 32
>> type 0x21, req 0x20, val 0, idx 2
dfu_control type 0x21, req 0x20, val 0, idx 2
msc_control type 0x21, req 0x20, val 0, idx 2
*** msc_control next 32
>> *** type 0x21, req 0x20, val 0, idx 2
*** cdcacm_control type 0x21, req 0x20, val 0, idx 2
*** cdcacm_control 32
>> type 0x21, req 0x20, val 0, idx 2
dfu_control type 0x21, req 0x20, val 0, idx 2
msc_control type 0x21, req 0x20, val 0, idx 2
*** msc_control next 32
>> *** type 0x21, req 0x20, val 0, idx 2
*** cdcacm_control type 0x21, req 0x20, val 0, idx 2
*** cdcacm_control 32
>> type 0x21, req 0x20, val 0, idx 2
dfu_control type 0x21, req 0x20, val 0, idx 2
msc_control type 0x21, req 0x20, val 0, idx 2
*** msc_control next 32
>> *** type 0x21, req 0x20, val 0, idx 2
*** cdcacm_control type 0x21, req 0x20, val 0, idx 2
*** cdcacm_control 32
>> type 0x21, req 0x20, val 0, idx 2
dfu_control type 0x21, req 0x20, val 0, idx 2
msc_control type 0x21, req 0x20, val 0, idx 2
*** msc_control next 32
>> *** type 0x21, req 0x20, val 0, idx 2
*** cdcacm_control type 0x21, req 0x20, val 0, idx 2
*** cdcacm_control 32
[t
][e
][s
][t
][i
][n
][g
][
][o
][k
][
][1
][2
][3
][
][g
][s
][h
][s
][h
][s
][
][s
][u
][c
][c
][e
][s
][s
][!
][
][h
][e
][l
][l
][o
][!
][
][y
][e
][s
][
*/
