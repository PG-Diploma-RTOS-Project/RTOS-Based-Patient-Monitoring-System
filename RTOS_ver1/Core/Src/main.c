/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "font.h"
#include "mlx90614.h"
#include "st7735.h"     // Replaces character lcd.h with ST7735 SPI Display driver
#include "temperture.h" // Holds TemperatureData_t struct definition
#include "max30102.h"
#include "semphr.h"
#include "ecg.h"

#define FINGER_THRESHOLD 2000

TaskHandle_t TFTTaskHandle = NULL;

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
typedef struct
{
    float objectTemp;
    float ambientTemp;
    uint16_t heartRate;
    float spo2;
    uint16_t ecgVal;
    uint8_t ecgLeadOff;
} SystemData_t;

SystemData_t gSystemData;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */
QueueHandle_t xTFTQueue;
QueueHandle_t xUARTQueue;

SemaphoreHandle_t xDataMutex;

TaskHandle_t ECGTaskHandle = NULL;
TaskHandle_t UARTTaskHandle = NULL;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C2_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */
void vECGTask(void *pvParameters);
void vMax30102Task(void *pvParameters);
void vTemperatureTask(void *pvParameters);
void vTFTTask(void *pvParameters);
void vUARTTask(void *pvParameters);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void vECGTask(void *argument)
{
    ECG_Init(); // Starts TIM2 & ADC DMA

    while(1)
    {
        // Wait for notification from ADC DMA interrupt
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Always notify the UART task so vUARTTask can process the DMA buffer
        // and handle lead-off state cleanly ($E,0 or $E,val)
        if(UARTTaskHandle != NULL)
        {
            xTaskNotifyGive(UARTTaskHandle);
        }
    }
}

void vMax30102Task(void *pvParameters)
{
    MAX30102_Data_t sample;

    static uint32_t redBuffer[100];
    static uint32_t irBuffer[100];
    static uint8_t idx = 0;

    while(MAX30102_Init() != HAL_OK)
    {
        HAL_UART_Transmit(&huart2, (uint8_t*)"MAX30102 Init Retry...\r\n", 24, 100);
        HAL_I2C_DeInit(&hi2c1);
        vTaskDelay(pdMS_TO_TICKS(50));
        HAL_I2C_Init(&hi2c1);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    while(1)
    {
        if(MAX30102_ReadSample(&sample) == HAL_OK)
        {
            if(sample.ir < FINGER_THRESHOLD)
            {
                xSemaphoreTake(xDataMutex, portMAX_DELAY);

                gSystemData.heartRate = 0;
                gSystemData.spo2 = 0.0f;

                idx = 0;
                memset(redBuffer, 0, sizeof(redBuffer));
                memset(irBuffer, 0, sizeof(irBuffer));

                SystemData_t snapshot = gSystemData;
                xSemaphoreGive(xDataMutex);

                xQueueOverwrite(xTFTQueue, &snapshot);
                xQueueOverwrite(xUARTQueue, &snapshot);

                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }

            redBuffer[idx] = sample.red;
            irBuffer[idx]  = sample.ir;

            Calculate_HeartRate(sample.ir);

            xSemaphoreTake(xDataMutex, portMAX_DELAY);
            gSystemData.heartRate = heartRate;

            if(++idx >= 100)
            {
                gSystemData.spo2 = Calculate_SPO2(redBuffer, irBuffer);
                idx = 0;
            }

            SystemData_t snapshot = gSystemData;
            xSemaphoreGive(xDataMutex);

            xQueueOverwrite(xTFTQueue, &snapshot);
            xQueueOverwrite(xUARTQueue, &snapshot);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void vTFTTask(void *pvParameters)
{
    SystemData_t data;
    static SystemData_t oldData = {0};

    ST7735_Init(&hspi1);

    ST7735_FillScreen(ST7735_BLACK);
    ST7735_WriteString(5, 10, "--VITAL MON--", Font_8x12, ST7735_WHITE,  ST7735_BLACK);
    ST7735_WriteString(5, 40, "OBJ :",        Font_8x12, ST7735_WHITE,  ST7735_BLACK);
    ST7735_WriteString(5, 60, "AMB :",        Font_8x12, ST7735_GREEN,  ST7735_BLACK);
    ST7735_WriteString(5, 100,"HR  :",        Font_8x12, ST7735_RED,    ST7735_BLACK);
    ST7735_WriteString(5, 120,"SpO2:",        Font_8x12, ST7735_YELLOW, ST7735_BLACK);

    while(1)
    {
        if(xQueueReceive(xTFTQueue, &data, portMAX_DELAY) == pdPASS)
        {
            char str[32];

            if(oldData.objectTemp != data.objectTemp)
            {
                snprintf(str, sizeof(str), "%.1f C   ", data.objectTemp);
                ST7735_WriteString(45, 40, str, Font_8x12, ST7735_WHITE, ST7735_BLACK);
                oldData.objectTemp = data.objectTemp;
            }

            if(oldData.ambientTemp != data.ambientTemp)
            {
                snprintf(str, sizeof(str), "%.1f C   ", data.ambientTemp);
                ST7735_WriteString(45, 60, str, Font_8x12, ST7735_GREEN, ST7735_BLACK);
                oldData.ambientTemp = data.ambientTemp;
            }

            if(oldData.heartRate != data.heartRate)
            {
                snprintf(str, sizeof(str), "%3d BPM   ", data.heartRate);
                ST7735_WriteString(45, 100, str, Font_8x12, ST7735_RED, ST7735_BLACK);
                oldData.heartRate = data.heartRate;
            }

            if(oldData.spo2 != data.spo2)
            {
                if(data.spo2 == 0.0f)
                {
                    ST7735_WriteString(45, 120, "--- %%    ", Font_8x12, ST7735_YELLOW, ST7735_BLACK);
                }
                else
                {
                    snprintf(str, sizeof(str), "%.1f %%   ", data.spo2);
                    ST7735_WriteString(45, 120, str, Font_8x12, ST7735_YELLOW, ST7735_BLACK);
                }
                oldData.spo2 = data.spo2;
            }
        }
    }
}

void vUARTTask(void *argument)
{
    SystemData_t vitalsSnapshot = {0};
    char txBuffer[96];

    while(1)
    {
        // Clear Overrun Errors on USART2 if any occurred
        if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE)) {
            __HAL_UART_CLEAR_OREFLAG(&huart2);
        }

        // Non-blocking attempt to refresh latest sensor snapshot
        xQueueReceive(xUARTQueue, &vitalsSnapshot, 0);

        // Wait for DMA half/full transfer notification from ISR
        if(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10)) > 0)
        {
            uint16_t startIdx = (dmaReady == ECG_FIRST_HALF) ? 0 : ECG_BUFFER_SIZE;

            // Check Lead-Off Detection pins (PA1 = LO+, PA4 = LO-)
            uint8_t isLeadOff = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_SET) ||
                                (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_SET);

            for(int i = startIdx; i < (startIdx + ECG_BUFFER_SIZE); i += 4)
            {
                // Force ADC value to 0 if ECG leads are disconnected
                uint16_t ecgVal = isLeadOff ? 0 : adcBuffer[i];

                int len = snprintf(txBuffer, sizeof(txBuffer),
                                   "$D,%u,%.2f,%.2f,%d,%.2f\r\n",
                                   ecgVal,
                                   vitalsSnapshot.objectTemp,
                                   vitalsSnapshot.ambientTemp,
                                   vitalsSnapshot.heartRate,
                                   vitalsSnapshot.spo2);

                HAL_UART_Transmit(&huart2, (uint8_t*)txBuffer, len, 10);
            }

            dmaReady = ECG_NONE;
        }
    }
}

void vTemperatureTask(void *pvParameters)
{
    while(1)
    {
        float obj = MLX90614_ReadObjectTemp();
        float amb = MLX90614_ReadAmbientTemp();

        if(obj < -100 || amb < -100)
        {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        xSemaphoreTake(xDataMutex, portMAX_DELAY);
        gSystemData.objectTemp = obj;
        gSystemData.ambientTemp = amb;
        SystemData_t snapshot = gSystemData;
        xSemaphoreGive(xDataMutex);

        xQueueOverwrite(xTFTQueue, &snapshot);
        xQueueOverwrite(xUARTQueue, &snapshot);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  MX_I2C2_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  xTFTQueue  = xQueueCreate(1,sizeof(SystemData_t));
  xUARTQueue = xQueueCreate(1,sizeof(SystemData_t));

  xDataMutex = xSemaphoreCreateMutex();

  if(xTFTQueue==NULL ||
     xUARTQueue==NULL ||
     xDataMutex==NULL)
  {
      Error_Handler();
  }

  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  memset(&gSystemData, 0, sizeof(gSystemData));
  /* Pass handle to xTaskCreate for UARTTaskHandle */
  if(xTaskCreate(vUARTTask, "UART", 512, NULL, 2, &UARTTaskHandle) != pdPASS)
  {
      Error_Handler();
  }

  if(xTaskCreate(vECGTask, "ECG", 256, NULL, 4, &ECGTaskHandle) != pdPASS)
  {
      Error_Handler();
  }

  if(xTaskCreate(vMax30102Task, "MAX30102", 512, NULL, 3, NULL) != pdPASS)
  {
      Error_Handler();
  }

  if(xTaskCreate(vTemperatureTask, "TEMP", 256, NULL, 2, NULL) != pdPASS)
  {
      Error_Handler();
  }

  if(xTaskCreate(vTFTTask, "TFT", 512, NULL, 1, &TFTTaskHandle) != pdPASS)
  {
      Error_Handler();
  }
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 50;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 83;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 3999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA1 PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB2 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END 5 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
