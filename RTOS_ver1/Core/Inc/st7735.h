/*
 * st7735.h
 *
 *  Created on: Jul 26, 2026
 *      Author: sainath
 */

#ifndef INC_ST7735_H_
#define INC_ST7735_H_


#ifndef ST7735_H
#define ST7735_H

#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "font.h"

// Display Dimensions
#define ST7735_WIDTH   128
#define ST7735_HEIGHT  160

// Basic Color Definitions (16-bit RGB565)
#define ST7735_BLACK   0x0000
#define ST7735_RED    0x001F
#define ST7735_BLUE     0xF800
#define ST7735_GREEN   0x07E0
#define ST7735_CYAN    0x07FF
#define ST7735_MAGENTA 0xF81F
#define ST7735_YELLOW  0xFFE0
#define ST7735_WHITE   0xFFFF


// Display Offset Fix for KMR-1.8 SPI Display Module
#define ST7735_XSTART  0
#define ST7735_YSTART  0

// Pin Definitions - Change these to match your board configuration
#define ST7735_CS_PORT    GPIOB
#define ST7735_CS_PIN     GPIO_PIN_0

#define ST7735_DC_PORT    GPIOB
#define ST7735_DC_PIN     GPIO_PIN_1

#define ST7735_RES_PORT   GPIOB
#define ST7735_RES_PIN    GPIO_PIN_2

// ST7735 Commands
#define ST7735_NOP        0x00
#define ST7735_SWRESET    0x01
#define ST7735_SLPOUT     0x11
#define ST7735_DISPON     0x29
#define ST7735_CASET      0x2A
#define ST7735_RASET      0x2B
#define ST7735_RAMWR      0x2C
#define ST7735_MADCTL     0x36
#define ST7735_COLMOD     0x3A

// Driver Functions
void ST7735_Init(SPI_HandleTypeDef *hspi);
void ST7735_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ST7735_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7735_FillScreen(uint16_t color);
void ST7735_WriteChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor);
void ST7735_WriteString(uint16_t x, uint16_t y, const char *str, FontDef font, uint16_t color, uint16_t bgcolor);

#endif // ST7735_H
#endif /* INC_ST7735_H_ */
