/*
 * uart_debug.c
 *
 *  Created on: 31-Jul-2026
 *      Author: sunbeam
 */


#include "uart_debug.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart2;

char txBuffer[20];

void UART_SendString(char *str)
{
    HAL_UART_Transmit(&huart2,
                      (uint8_t *)str,
                      strlen(str),
                      HAL_MAX_DELAY);
}

void UART_SendNumber(uint16_t value)
{
    int len = sprintf(txBuffer, "%u\r\n", value);

    HAL_UART_Transmit(&huart2,
                      (uint8_t *)txBuffer,
                      len,
                      HAL_MAX_DELAY);
}
