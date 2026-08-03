#ifndef INC_ECG_H_
#define INC_ECG_H_

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

#ifndef LO_PLUS_Pin
#define LO_PLUS_Pin          GPIO_PIN_1
#define LO_PLUS_GPIO_Port    GPIOA
#endif

#ifndef LO_MINUS_Pin
#define LO_MINUS_Pin         GPIO_PIN_4
#define LO_MINUS_GPIO_Port   GPIOA
#endif

#define ECG_BUFFER_SIZE 200

extern uint16_t adcBuffer[ECG_BUFFER_SIZE * 2];

extern TaskHandle_t ECGTaskHandle;

typedef enum
{
    ECG_NONE = 0,
    ECG_FIRST_HALF,
    ECG_SECOND_HALF
}ECG_DMAState_t;

extern volatile ECG_DMAState_t dmaReady;

void ECG_Init(void);
uint8_t ECG_LeadOffDetected(void);

#endif
