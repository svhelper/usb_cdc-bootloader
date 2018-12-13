//  CDC code from https://github.com/Apress/Beg-STM32-Devel-FreeRTOS-libopencm3-GCC/blob/master/rtos/usbcdcdemo/usbcdc.c
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>
#include <logger.h>
#include "usb_conf.h"
#include "cdc.h"

#define CONTROL_CALLBACK_TYPE (USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE)
#define CONTROL_CALLBACK_MASK (USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT)

/*
 * USB Control Requests:
 */
static enum usbd_request_return_codes
cdcacm_control_request(
  usbd_device *usbd_dev __attribute__((unused)),
  struct usb_setup_data *req,
  uint8_t **buf __attribute__((unused)),
  uint16_t *len,
  void (**complete)(
    usbd_device *usbd_dev,
    struct usb_setup_data *req
  ) __attribute__((unused))
) {
	dump_usb_request("*** cdcacm_control", req); ////
	if ((req->bmRequestType & CONTROL_CALLBACK_MASK) != CONTROL_CALLBACK_TYPE) {
		return USBD_REQ_NEXT_CALLBACK;  //  Not my callback type.  Hand off to next callback.
	}
    if (req->wIndex != INTF_COMM && req->wIndex != INTF_DATA) {
		//  Not for my interface.  Hand off to next interface.
        return USBD_REQ_NEXT_CALLBACK;
    }
    debug_print("*** cdcacm_control "); debug_print_unsigned(req->bRequest); debug_println(""); // debug_flush(); ////
	switch (req->bRequest) {
	case USB_CDC_REQ_SET_CONTROL_LINE_STATE:
		/*
		 * The Linux cdc_acm driver requires this to be implemented
		 * even though it's optional in the CDC spec, and we don't
		 * advertise it in the ACM functional descriptor.
		 */
		return USBD_REQ_HANDLED;
	case USB_CDC_REQ_SET_LINE_CODING:
		if ( *len < sizeof(struct usb_cdc_line_coding) ) {
			return USBD_REQ_NOTSUPP;
		}
		return USBD_REQ_HANDLED;
	}
    debug_print("*** cdcacm_control notsupp "); debug_print_unsigned(req->bRequest); debug_println(""); debug_flush(); ////
	return USBD_REQ_NOTSUPP;
}

//  TODO: TX Up to MAX_USB_PACKET_SIZE
//  usbd_ep_write_packet(usbd_dev, DATA_IN, txbuf, txlen)

static char cdcbuf[MAX_USB_PACKET_SIZE + 1];   // rx buffer

/*
 * USB Receive Callback:
 */
static void
cdcacm_data_rx_cb(
  usbd_device *usbd_dev,
  uint8_t ep __attribute__((unused))
) {
	uint16_t len = usbd_ep_read_packet(usbd_dev, DATA_OUT, cdcbuf, MAX_USB_PACKET_SIZE);
    if (len == 0) { return; }
    uint16_t pos = (len < MAX_USB_PACKET_SIZE) ? len : MAX_USB_PACKET_SIZE;
    cdcbuf[pos] = 0;
    debug_print("<< "); debug_println(cdcbuf); ////
/*
	// How much queue capacity left?
	unsigned rx_avail = uxQueueSpacesAvailable(usb_rxq);
	char buf[64];	// rx buffer
	int len, x;

	if ( rx_avail <= 0 )
		return;	// No space to rx

	// Bytes to read
	len = sizeof buf < rx_avail ? sizeof buf : rx_avail;

	// Read what we can, leave the rest:
	len = usbd_ep_read_packet(usbd_dev, 0x01, buf, len);

	for ( x=0; x<len; ++x ) {
		// Send data to the rx queue
		xQueueSend(usb_rxq,&buf[x],0);
	}
*/
}

/*
 * USB Configuration:
 */
static void
cdcacm_set_config(
  usbd_device *usbd_dev,
  uint16_t wValue __attribute__((unused))
) {
    debug_println("*** cdcacm_set_config"); ////
	usbd_ep_setup(usbd_dev,
		DATA_OUT,
		USB_ENDPOINT_ATTR_BULK,
		MAX_USB_PACKET_SIZE,
		cdcacm_data_rx_cb);
	usbd_ep_setup(usbd_dev,
		DATA_IN,
		USB_ENDPOINT_ATTR_BULK,
		MAX_USB_PACKET_SIZE,
		NULL);
	//  TODO: Only 4 callbacks allowed. Aggregate them.
	int status = usbd_register_control_callback(
		usbd_dev,
		CONTROL_CALLBACK_TYPE,
		CONTROL_CALLBACK_MASK,
		cdcacm_control_request);
	if (status < 0) {
    	debug_println("*** cdcacm_set_config failed"); debug_flush(); ////
	}
}

void cdc_setup(usbd_device* usbd_dev) {
    debug_println("*** cdc_setup"); ////
	usbd_register_set_config_callback(usbd_dev, cdcacm_set_config);
}
