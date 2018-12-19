# bluepill-bootloader
bluepill-bootloader is an open-source MakeCode UF2 USB bootloader for STM32 Blue Pill devices.

_This is the HF2 branch that supports HF2 flashing over WebUSB by visualbluepill.github.io. USB Serial is supported but not USB Storage and USB DFU._ 

See https://medium.com/@ly.lee/stm32-blue-pill-usb-bootloader-how-i-fixed-the-usb-storage-serial-dfu-and-webusb-interfaces-36d7fe245b5c

https://medium.com/@ly.lee/work-in-progress-stm32-blue-pill-visual-programming-with-makecode-codal-and-libopencm3-422d308f252e

Original bootloader implementation from https://github.com/mmoskal/uf2-stm32f103

WebUSB and WinUSB implementation from https://github.com/trezor/trezor-mcu

USB CDC implementation from https://github.com/Apress/Beg-STM32-Devel-FreeRTOS-libopencm3-GCC/blob/master/rtos/usbcdcdemo/usbcdc.c

USB MSC patch from https://habr.com/company/thirdpin/blog/304924/

HF2 implementation from https://github.com/mmoskal/uf2-stm32f

## Build instructions

1. Clone this branch with git

1. Launch Visual Studio Code

1. Install the PlatformIO extension for Visual Studio Code

1. Open the workspace file in the above folder

1. Build the bootloader

1. Flash the bootloader to Blue Pill via ST-Link v2

1. Browse to visualbluepill.github.io

1. Create a new program

1. At top right, click `Pair Device`. Select `DAPBoot DFU Bootloader`

1. Click `Download` to flash

## Licensing
All contents of the project are licensed under terms that are compatible with the terms of the GNU Lesser General Public License version 3.

Non-libopencm3 related portions of the dapboot project are licensed under the less restrictive ISC license, except where otherwise specified in the headers of specific files.

See the LICENSE file for full details.
