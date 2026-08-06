#include "ecg.h"

extern TIM_HandleTypeDef htim2;
extern ADC_HandleTypeDef hadc1;

uint16_t adcBuffer[ECG_BUFFER_SIZE * 2];

volatile ECG_DMAState_t dmaReady = ECG_NONE;

uint8_t ECG_LeadOffDetected(void)
   {
       // Active-Low configuration: returns true ONLY when pins go HIGH
       return (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET ||
               HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_SET);
   }

void ECG_Init(void)
{
	HAL_TIM_Base_Start(&htim2);

	if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adcBuffer, ECG_BUFFER_SIZE * 2) != HAL_OK)
	    {
		Error_Handler();
	    }
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

        if(ECGTaskHandle != NULL)
                {
                    vTaskNotifyGiveFromISR(ECGTaskHandle, &xHigherPriorityTaskWoken);
                }

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
