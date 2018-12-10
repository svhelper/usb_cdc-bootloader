//  Functions missing from libopencm3
#include <stdlib.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/lib/usb/usb_private.h>

//  From bluepill-bootloader/libopencm3/lib/usb/usb.c
void usbd_register_extra_string(usbd_device *usbd_dev, int index, const char* string)
{
	if (string != NULL && index > 0)
	{
		usbd_dev->extra_string_idx = index;
		usbd_dev->extra_string = string;
	}
	else
	{
		usbd_dev->extra_string_idx = -1;
	}
}
