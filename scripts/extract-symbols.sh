#!/usr/bin/env bash
# Export symbols from the bootloader ELF into a symbol ELF file.

# file=.pioenvs/bluepill_f103c8_bootloader/firmware.elf
# file=/mnt/c/stm32bluepill-makecode/pxt-maker/projects/blink/built/dockercodal/build/STM32_BLUE_PILL
file=/mnt/c/stm32bluepill-makecode/pxt-maker/projects/blink/built/dockercodal/build/liblibopencm3.a

arm-none-eabi-nm \
    --print-file-name \
    $file

exit

#Output:
#/mnt/c/stm32bluepill-makecode/pxt-maker/projects/blink/built/dockercodal/build/liblibopencm3.a:rtc.c.o:00000000 T rtc_set_prescale_val
#/mnt/c/stm32bluepill-makecode/pxt-maker/projects/blink/built/dockercodal/build/liblibopencm3.a:timer.c.o:00000000 T timer_ic_set_polarity

#    --debug-syms \

arm-none-eabi-objcopy \
    --strip-all \
    @scripts/extract-symbols.options \
    .pioenvs/bluepill_f103c8_bootloader/firmware.elf \
    bootloader-symbols.elf

arm-none-eabi-nm bootloader-symbols.elf

# arm-none-eabi-objcopy \
#     --strip-all \
#     --keep-symbol=led_on \
#     --keep-symbol=led_off \
#     .pioenvs/bluepill_f103c8_bootloader/firmware.elf \
#     bootloader-symbols.elf
