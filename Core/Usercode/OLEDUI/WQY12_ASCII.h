#ifndef __WQY12_ASCII_H
#define __WQY12_ASCII_H

#include <stdint.h>

/* WenQuanYi Bitmap Song 9pt, printable ASCII 0x20..0x7E in a 7x12 cell.
 * Byte layout is identical to OLED_F7x12[][14]:
 * 14 bytes = 2 pages x 7 columns, bit0 is the top row of the page. */
#define WQY12_ASCII_FIRST_CHAR      (0x20)
#define WQY12_ASCII_COUNT           (95)
#define WQY12_ASCII_WIDTH           (7)
#define WQY12_ASCII_HEIGHT          (12)

extern const uint8_t WQY12_ASCII[WQY12_ASCII_COUNT][14];

#endif
