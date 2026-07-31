#ifndef INC_ECG_H_
#define INC_ECG_H_

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

#define ECG_BUFFER_SIZE 250

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
