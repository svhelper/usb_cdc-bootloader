#ifndef GHOSTFAT_H_INCLUDED
#define GHOSTFAT_H_INCLUDED

#include <stdint.h>

extern void flash_write(uint32_t dst, const uint8_t *src, int len);

#endif  //  GHOSTFAT_H_INCLUDED
