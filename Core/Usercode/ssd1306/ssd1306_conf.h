/**
 * Private configuration file for the SSD1306 library.
 * Adapted for PWM_Tool (STM32F103C8T6, SPI2).
 */

#ifndef __SSD1306_CONF_H__
#define __SSD1306_CONF_H__

#include "main.h"

// Choose a microcontroller family
#define STM32F1

// Choose a bus
#define SSD1306_USE_SPI

// SPI Configuration (SPI2: PB13=SCK, PB15=MOSI)
#define SSD1306_SPI_PORT        hspi2

#define SSD1306_CS_Port         OLED_CS_GPIO_Port
#define SSD1306_CS_Pin          OLED_CS_Pin
#define SSD1306_DC_Port         OLED_DC_GPIO_Port
#define SSD1306_DC_Pin          OLED_DC_Pin
#define SSD1306_Reset_Port      OLED_RST_GPIO_Port
#define SSD1306_Reset_Pin       OLED_RST_Pin

// Mirror the screen if needed
// #define SSD1306_MIRROR_VERT
// #define SSD1306_MIRROR_HORIZ

// Set inverse color if needed
// # define SSD1306_INVERSE_COLOR

// Include only needed fonts
#define SSD1306_INCLUDE_FONT_6x8
#define SSD1306_INCLUDE_FONT_7x10
#define SSD1306_INCLUDE_FONT_11x18
//#define SSD1306_INCLUDE_FONT_16x26
//#define SSD1306_INCLUDE_FONT_16x24
//#define SSD1306_INCLUDE_FONT_16x15

// The width of the screen can be set using this
// define. The default value is 128.
#define SSD1306_WIDTH           128

// The height can be changed as well if necessary.
// It can be 32, 64 or 128. The default value is 64.
#define SSD1306_HEIGHT          32

// If your screen horizontal axis does not start
// in column 0 you can use this define to
// adjust the horizontal offset
// #define SSD1306_X_OFFSET

#endif /* __SSD1306_CONF_H__ */
