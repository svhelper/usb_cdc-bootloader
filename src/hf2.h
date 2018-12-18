#ifndef HF2_H_INCLUDED
#define HF2_H_INCLUDED

#include <libopencm3/usb/usbd.h>

extern void hf2_setup(usbd_device *usbd_dev, const uint8_t *hid_report_descriptor0, uint16_t hid_report_descriptor_size0);

#endif  //  HF2_H_INCLUDED
