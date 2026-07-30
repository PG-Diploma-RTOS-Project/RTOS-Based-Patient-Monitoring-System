/*
 * max30102.h
 *
 *  Created on: 19-Jun-2026
 *      Author: sunbeam
 */

#ifndef INC_MAX30102_H_
#define INC_MAX30102_H_


#include "main.h"

#define MAX30102_I2C_ADDR      (0x57 << 1)

#define REG_FIFO_WR_PTR        0x04
#define REG_OVF_COUNTER        0x05
#define REG_FIFO_RD_PTR        0x06
#define REG_FIFO_DATA          0x07

#define REG_FIFO_CONFIG        0x08
#define REG_MODE_CONFIG        0x09
#define REG_SPO2_CONFIG        0x0A

#define REG_LED1_PA            0x0C
#define REG_LED2_PA            0x0D

#define REG_PART_ID            0xFF

#define MAX30102_PART_ID       0x15

#define MODE_RESET             0x40
#define MODE_SPO2              0x03

#define FIFO_CONFIG_VALUE      0x4F

#define SPO2_CONFIG_VALUE      0x27

#define RED_LED_CURRENT        0x24
#define IR_LED_CURRENT         0x24

typedef struct
{
    uint32_t red;
    uint32_t ir;

} MAX30102_Data_t;

typedef MAX30102_Data_t SensorData_t;

typedef struct
{
    int heartRate;
    float spo2;

} DisplayData_t;

/* Public Functions */
HAL_StatusTypeDef MAX30102_Init(void);

HAL_StatusTypeDef MAX30102_ReadSample(MAX30102_Data_t *data);

float Calculate_SPO2(uint32_t *red, uint32_t *ir);

void Calculate_HeartRate(uint32_t irValue);

extern int heartRate;

extern uint32_t previousIR;

extern uint32_t lastBeatTime;

extern uint32_t currentBeatTime;



#endif /* INC_MAX30102_H_ */
