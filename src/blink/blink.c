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
#include <libopencm3/cm3/vector.h>
#include <bluepill.h>
#include <logger.h>
#include "target.h"

static void delay(int n) {
    for (int i = 0; i < n*1000000; ++i)
        asm("nop");
}

int main(void) {
    enable_debug();       //  Uncomment to allow display of debug messages in development devices. NOTE: This will hang if no debugger is attached.
    //  disable_debug();  //  Uncomment to disable display of debug messages.  For use in production devices.
    platform_setup();     //  STM32 platform setup.
    debug_println("----firmware");  debug_flush();

    //  Clock already setup in platform_setup()
    //  target_clock_setup();

    /* Initialize GPIO/LEDs if needed */
    target_gpio_setup();

        while (1) {
            debug_println("firmware loop");  debug_flush();
            target_set_led(1);
            delay(1);
            target_set_led(0);
            delay(1);
        }
    
    return 0;
}
