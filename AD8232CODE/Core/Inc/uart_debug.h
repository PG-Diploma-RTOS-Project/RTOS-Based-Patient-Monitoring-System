/*
 * uart_debug.h
 *
 *  Created on: 29-Jul-2026
 *      Author: sunbeam
 */

#ifndef INC_UART_DEBUG_H_
#define INC_UART_DEBUG_H_

#include "main.h"
#include <stdint.h>

/* Function Prototypes */
void UART_SendString(char *str);
void UART_SendNumber(uint16_t value);

#endif /* INC_UART_DEBUG_H_ */
