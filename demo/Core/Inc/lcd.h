/*
 * lcd.h
 *
 *  Created on: 13-Jun-2026
 *      Author: sunbeam
 */

#ifndef INC_LCD_H_
#define INC_LCD_H_
#include "main.h"


#ifndef BV
	#define BV(n) (1 << (n))
#endif
#define LCD_SLAVE_ADDR (0x27 << 1)

#define LCD_DB4_Pos     4
#define LCD_DB5_Pos     5
#define LCD_DB6_Pos     6
#define LCD_DB7_Pos     7

#define LCD_RS_Pos 0
#define LCD_RW_Pos 1
#define LCD_EN_Pos 2
#define LCD_BL_Pos 3

#define LCD_CLEAR 0x01
#define ENTRY_MODE_SET 0x06
#define DISPLAY_ON_OFF 0x0C
#define FUNCTION_SET 0x28
#define DISPLAY_SHIFT_LEFT 0x18


#define LCD_LINE_1 0x80
#define LCD_LINE_2 0xC0

#define LCD_CMD 0
#define LCD_DATA 1

void lcd_init(void);
void lcd_write_nibble(uint8_t rs,uint8_t val);
void lcd_busy_wait(void);
void lcd_write_byte(uint8_t rs, uint8_t val);
void lcd_puts(uint8_t line, char str[]);
void display_lcd(void);


#endif /* INC_LCD_H_ */
