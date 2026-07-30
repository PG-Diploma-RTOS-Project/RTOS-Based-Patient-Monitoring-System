/*
 * max30102.c
 *
 *  Created on: 19-Jun-2026
 *      Author: sunbeam
 */
#include "max30102.h"

int heartRate = 0;

uint32_t previousIR = 0;

uint32_t lastBeatTime = 0;

uint32_t currentBeatTime = 0;

extern I2C_HandleTypeDef hi2c1;

static HAL_StatusTypeDef MAX30102_WriteRegister(uint8_t reg, uint8_t data);
static HAL_StatusTypeDef MAX30102_ReadRegister(uint8_t reg, uint8_t *data);
static HAL_StatusTypeDef MAX30102_ReadBytes(uint8_t reg, uint8_t *buffer,uint8_t length);
static HAL_StatusTypeDef MAX30102_FIFO_Init(void);
static HAL_StatusTypeDef MAX30102_SPO2_Config(void);
static HAL_StatusTypeDef MAX30102_LED_Config(void);

static HAL_StatusTypeDef MAX30102_WriteRegister(uint8_t reg, uint8_t data)
{
    return HAL_I2C_Mem_Write(&hi2c1, MAX30102_I2C_ADDR,reg, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
}

static HAL_StatusTypeDef MAX30102_ReadRegister(uint8_t reg, uint8_t *data)
{
    return HAL_I2C_Mem_Read(&hi2c1,MAX30102_I2C_ADDR, reg,I2C_MEMADD_SIZE_8BIT,data,1, HAL_MAX_DELAY);
}

static HAL_StatusTypeDef MAX30102_ReadBytes(uint8_t reg, uint8_t *buffer,uint8_t length)
{
    return HAL_I2C_Mem_Read(&hi2c1, MAX30102_I2C_ADDR, reg,I2C_MEMADD_SIZE_8BIT,buffer,length,HAL_MAX_DELAY);
}

static HAL_StatusTypeDef MAX30102_Reset(void)
{
    if (MAX30102_WriteRegister(REG_MODE_CONFIG, MODE_RESET) != HAL_OK)
    {
        return HAL_ERROR;
    }

    HAL_Delay(20);

    return HAL_OK;
}

static HAL_StatusTypeDef MAX30102_CheckPartID(void)
{
    uint8_t partID;

    if (MAX30102_ReadRegister(REG_PART_ID, &partID) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (partID != MAX30102_PART_ID)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef MAX30102_FIFO_Init(void)
{
    if (MAX30102_WriteRegister(REG_FIFO_WR_PTR, 0x00) != HAL_OK)
        return HAL_ERROR;

    if (MAX30102_WriteRegister(REG_OVF_COUNTER, 0x00) != HAL_OK)
        return HAL_ERROR;

    if (MAX30102_WriteRegister(REG_FIFO_RD_PTR, 0x00) != HAL_OK)
        return HAL_ERROR;

    if (MAX30102_WriteRegister(REG_FIFO_CONFIG, FIFO_CONFIG_VALUE) != HAL_OK)
        return HAL_ERROR;

    return HAL_OK;
}

static HAL_StatusTypeDef MAX30102_SPO2_Config(void)
{
    if (MAX30102_WriteRegister(REG_SPO2_CONFIG, SPO2_CONFIG_VALUE) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (MAX30102_WriteRegister(REG_MODE_CONFIG, MODE_SPO2) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef MAX30102_LED_Config(void)
{
    if (MAX30102_WriteRegister(REG_LED1_PA, RED_LED_CURRENT) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (MAX30102_WriteRegister(REG_LED2_PA,IR_LED_CURRENT) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef MAX30102_Init(void)
{
    if (MAX30102_Reset() != HAL_OK)
        return HAL_ERROR;

    if (MAX30102_CheckPartID() != HAL_OK)
        return HAL_ERROR;

    if (MAX30102_FIFO_Init() != HAL_OK)
        return HAL_ERROR;

    if (MAX30102_SPO2_Config() != HAL_OK)
        return HAL_ERROR;

    if (MAX30102_LED_Config() != HAL_OK)
        return HAL_ERROR;

    return HAL_OK;
}

HAL_StatusTypeDef MAX30102_ReadSample(MAX30102_Data_t *data)
{
    uint8_t fifo[6];

    if (data == NULL)
        return HAL_ERROR;

    if (MAX30102_ReadBytes(REG_FIFO_DATA,fifo, 6) != HAL_OK)
    {
        return HAL_ERROR;
    }

    data->red = ((uint32_t)fifo[0] << 16) | ((uint32_t)fifo[1] << 8)  |(uint32_t)fifo[2];

    data->red &= 0x03FFFF;

    data->ir = ((uint32_t)fifo[3] << 16) | ((uint32_t)fifo[4] << 8)  | (uint32_t)fifo[5];

    data->ir &= 0x03FFFF;

    return HAL_OK;
}

float Calculate_SPO2(uint32_t *red, uint32_t *ir)
{
    uint32_t red_sum = 0, ir_sum = 0;
    uint32_t red_max = red[0], red_min = red[0];
    uint32_t ir_max  = ir[0],  ir_min  = ir[0];

    for(int i = 0; i < 100; i++)
    {
        red_sum += red[i];
        ir_sum  += ir[i];

        if(red[i] > red_max) red_max = red[i];
        if(red[i] < red_min) red_min = red[i];

        if(ir[i] > ir_max) ir_max = ir[i];
        if(ir[i] < ir_min) ir_min = ir[i];
    }

    float dc_red = red_sum / 100.0f;
    float dc_ir  = ir_sum  / 100.0f;

    float ac_red = red_max - red_min;
    float ac_ir  = ir_max - ir_min;

    if(ac_ir == 0 || dc_red == 0 || dc_ir == 0)
        return 0;

    float R = (ac_red / dc_red) / (ac_ir / dc_ir);

    //float spo2 = 110.0f - 25.0f * R;
    float spo2 = 104.0f - 17.0f * R;
    if(spo2 > 100) spo2 = 100;
    if(spo2 < 80)  spo2 = 80;

    return spo2;
}

void Calculate_HeartRate(uint32_t irValue)
{
    static uint32_t irBuffer[8] = {0};
    static uint8_t bufIndex = 0;

    static uint8_t rising = 0;

    static uint32_t peakValue = 0;

    static uint32_t beatDiff[5] = {0};
    static uint8_t diffIndex = 0;
    static uint8_t diffCount = 0;


    uint32_t filteredIR = 0;

    irBuffer[bufIndex] = irValue;

    bufIndex++;

    if(bufIndex >= 8)
        bufIndex = 0;


    for(int i = 0; i < 8; i++)
    {
        filteredIR += irBuffer[i];
    }

    filteredIR /= 8;

    if(filteredIR < 40000)
    {
        heartRate = 0;

        previousIR = filteredIR;

        rising = 0;

        return;
    }


    if(filteredIR > previousIR)
    {
        rising = 1;

        if(filteredIR > peakValue)
        {
            peakValue = filteredIR;
        }
    }

    if(rising && filteredIR < previousIR)
    {

//        if(peakValue - filteredIR > 300)
    	if((peakValue - filteredIR) > (peakValue * 0.005))
        {

            currentBeatTime = HAL_GetTick();

            if((currentBeatTime - lastBeatTime) > 600)

            {

                if(lastBeatTime != 0)
                {

                    uint32_t diff;

                    diff = currentBeatTime - lastBeatTime;

                    if(diff > 400 && diff < 1500)
                    {

                        beatDiff[diffIndex] = diff;

                        diffIndex++;

                        if(diffIndex >= 5)
                            diffIndex = 0;


                        if(diffCount < 5)
                            diffCount++;

                        uint32_t avgDiff = 0;

                        for(int i = 0; i < diffCount; i++)
                        {
                            avgDiff += beatDiff[i];
                        }


                        avgDiff /= diffCount;

                        uint16_t bpm;

                        bpm = 60000 / avgDiff;

                        //if(bpm >= 50 && bpm <= 110)
                        if(bpm >= 40 && bpm <= 150)
                        {
                            heartRate = bpm;
                        }
                    }
                }
                lastBeatTime = currentBeatTime;
            }

            rising = 0;
            peakValue = filteredIR;
        }
    }

    previousIR = filteredIR;
}
