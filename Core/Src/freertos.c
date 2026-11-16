/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "bsp.h"

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

// --- Button Callback Handlers ---
static void Test_OnK0Button(BSP_Button_TypeDef btn) {
    BSP_LED_Toggle(BSP_LED1);
}

static void Test_OnK1Button(BSP_Button_TypeDef btn) {
    BSP_LED_Toggle(BSP_LED2);
}

// --- Shared Test Setup ---
static void BSP_Test_RegisterHandlers(void) {
    BSP_Button_RegisterCallback(BSP_BUTTON_K0, Test_OnK0Button);
    BSP_Button_RegisterCallback(BSP_BUTTON_K1, Test_OnK1Button);
}

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  BSP_Test_RegisterHandlers();

  for (uint8_t i = 0; i < BSP_LED_COUNT; i++) {
      BSP_LED_On((BSP_LED_TypeDef)i);
      osDelay(1000);
      BSP_LED_Off((BSP_LED_TypeDef)i);
  }

  /* Infinite loop */
  for(;;)
  {
    osDelay(500);
  }
  /* USER CODE END StartDefaultTask */
}


