#include "ecg.h"

extern TIM_HandleTypeDef htim2;
extern ADC_HandleTypeDef hadc1;

uint16_t adcBuffer[ECG_BUFFER_SIZE * 2];

volatile ECG_DMAState_t dmaReady = ECG_NONE;

uint8_t ECG_LeadOffDetected(void)
{
    if(HAL_GPIO_ReadPin(LO_PLUS_GPIO_Port, LO_PLUS_Pin) ||
       HAL_GPIO_ReadPin(LO_MINUS_GPIO_Port, LO_MINUS_Pin))
    {
        return 1;
    }

    return 0;
}

void ECG_Init(void)
{
    HAL_TIM_Base_Start(&htim2);

    HAL_ADC_Start_DMA(&hadc1,
                      (uint32_t *)adcBuffer,
                      ECG_BUFFER_SIZE * 2);
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if(hadc->Instance == ADC1)
    {
        dmaReady = ECG_FIRST_HALF;

        vTaskNotifyGiveFromISR(ECGTaskHandle,
                               &xHigherPriorityTaskWoken);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if(hadc->Instance == ADC1)
    {
        dmaReady = ECG_SECOND_HALF;

        vTaskNotifyGiveFromISR(ECGTaskHandle,
                               &xHigherPriorityTaskWoken);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
