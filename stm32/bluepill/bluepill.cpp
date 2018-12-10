//  Defines functions specific to the STM32 Blue Pill platform.
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/spi.h>    //  For SPI port definitions e.g. SPI1
#include <libopencm3/stm32/i2c.h>    //  For I2C port definitions e.g. I2C1
#include <libopencm3/stm32/usart.h>  //  For USART port definitions e.g. USART1
#include <logger.h>
#include "bluepill.h"

//  Debugging is off by default.  Developer must switch it on with enable_debug().
static bool debugEnabled = false;

void platform_setup(void) {
    //  Initialise the STM32 platform. At startup, the onboard LED will blink on-off-on-off-on and stays on.
	//  If LED blinks on-off-on-off and stays off, then debug mode is enabled and no debugger is connected.
	rcc_clock_setup_in_hse_8mhz_out_72mhz();
	led_setup();
	if (debugEnabled) {
		led_on(); led_wait();
		led_off(); led_wait();
		led_on(); led_wait();
		led_off(); led_wait();
		//  This line will call ARM Semihosting and may hang until debugger is connected.
  		debug_println("----platform_setup");
		led_on();
	}
}

void enable_debug(void) {
	//  Enable ARM Semihosting for displaying debug messages.
	debugEnabled = true;
	enable_log();
}

void disable_debug(void) {
	//  Disable ARM Semihosting for displaying debug messages.
	debugEnabled = false;
	disable_log();
}

//  For Legacy Arduino Support only...

#define ALL_PINS_SIZE (sizeof(allPins) / sizeof(uint32_t))
static const uint32_t allPins[] = {  //  Map Arduino pin number to STM32 port ID.
	0,  //  Unknown pin.
	SPI1,
	SPI2,
	I2C1,
	I2C2,
	USART1,
	USART2,
};  //  TODO: Support STM32 alternate port mapping.

uint32_t convert_pin_to_port(uint8_t pin) {
	//  Map Arduino Pin to STM32 Port, e.g. 1 becomes SPI1
	if (pin < 1 || pin >= ALL_PINS_SIZE) { return 0; }  //  Invalid pin.
	return allPins[pin];
}

uint8_t convert_port_to_pin(uint32_t port_id) {
	//  Map STM32 port to Arduino Pin, e.g. SPI1 becomes 1
	for (uint8_t pin = 1; pin < ALL_PINS_SIZE; pin++) {
		if (port_id == allPins[pin]) { return pin; }
	}
	return 0;  //  Invalid port.
}

//////////////////////////////////////////////////////////////////////////
//  From codal-libopencm3/stm32/cm/codal_target_hal.cpp

void target_enable_irq() {
  	//  debug_println("----target_enable_irq"); debug_flush();
	cm_enable_interrupts();
}

void target_disable_irq() {
  	//  debug_println("----target_disable_irq"); debug_flush();
	cm_disable_interrupts();
}

__attribute__((weak))
void target_panic(int statusCode) {
	//  TODO
    target_disable_irq();
	debug_print("*****target_panic ");
	debug_println((int) statusCode);
	debug_flush();
    ////DMESG("*** CODAL PANIC : [%d]", statusCode);
	for (;;) {
        __asm("wfe");  //  Allow CPU to go to sleep.
    }  //  Loop forever.
}

//  These functions must be located here or the compiler will pull in the standard C library versions.
//  From https://github.com/lancaster-university/codal-arduino-uno/blob/master/source/codal_target_hal.cpp

extern "C" void __cxa_pure_virtual() {
	//  Disable exceptions for abstract classes. See https://arobenko.gitbooks.io/bare_metal_cpp/content/compiler_output/abstract_classes.html
    target_panic(1000);
}

// define new and delete.
extern "C" void *operator new(size_t objsize) {
    return malloc(objsize);
}

extern "C" void operator delete(void* obj) {
    free(obj);
}
