#include "bsp.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_gpio.h"

// Toggle between CMSIS-RTOS v2 (1) and Bare-Metal Superloop (0)
#ifndef BSP_USE_RTOS
#define BSP_USE_RTOS 1
#endif

#if BSP_USE_RTOS
#include "cmsis_os2.h"
#endif

#define DEBOUNCE_TIME_MS 50  // 50 mS

typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
    uint8_t active_high;
} LED_Hardware_t;

static const LED_Hardware_t led_config[BSP_LED_COUNT] = {
    [BSP_LED1] = { GPIOA, GPIO_PIN_6,  0 },
    [BSP_LED2]   = { GPIOA, GPIO_PIN_7, 0 },
};

typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
    uint8_t active_high;
    uint32_t pull_mode;
    IRQn_Type irq_n;
} Button_Hardware_t;

static const Button_Hardware_t button_config[BSP_BUTTON_COUNT] = {
    [BSP_BUTTON_K0] = { GPIOE, GPIO_PIN_4, 0, GPIO_PULLUP, EXTI4_IRQn },
    [BSP_BUTTON_K1] = { GPIOE, GPIO_PIN_3, 0, GPIO_PULLUP, EXTI3_IRQn }
};

// Shared Runtime State
static BSP_Button_Callback_t button_callbacks[BSP_BUTTON_COUNT] = {NULL};
static uint32_t last_valid_press[BSP_BUTTON_COUNT] = {0};

#if BSP_USE_RTOS
static osMessageQueueId_t button_queue_id = NULL;
static osThreadId_t button_thread_id = NULL;
#else
static volatile bool button_irq_flag[BSP_BUTTON_COUNT] = {false};
static volatile uint32_t button_irq_time[BSP_BUTTON_COUNT] = {0};
#endif

// --- Shared Debounce & Callback Helper ---
static void BSP_HandleButtonEvent(BSP_Button_TypeDef button, uint32_t event_tick) {
    if ((event_tick - last_valid_press[button]) >= DEBOUNCE_TIME_MS) {
        last_valid_press[button] = event_tick;

        if (BSP_Button_IsPressed(button) && button_callbacks[button] != NULL) {
            button_callbacks[button](button);
        }
    }
}

#if BSP_USE_RTOS
// --- CMSIS-RTOS v2 Background Thread ---
static void BSP_ButtonThread(void* argument) {
    BSP_Button_TypeDef button;

    while (1) {
        // block for an ISR message
        if (osMessageQueueGet(button_queue_id, &button, NULL, osWaitForever) == osOK) {
            BSP_HandleButtonEvent(button, osKernelGetTickCount());
        }
    }
}
#endif

// --- Clock & GPIO Initialization ---
static void BSP_EnablePortClock(GPIO_TypeDef* port) {
    if (port == GPIOA)      __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
    else if (port == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
    else if (port == GPIOE) __HAL_RCC_GPIOE_CLK_ENABLE();
}

static void BSP_LED_InitAll(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    for (uint8_t i = 0; i < BSP_LED_COUNT; i++) {
        BSP_EnablePortClock(led_config[i].port);
        GPIO_InitStruct.Pin = led_config[i].pin;
        HAL_GPIO_Init(led_config[i].port, &GPIO_InitStruct);
        BSP_LED_Off((BSP_LED_TypeDef)i);
    }
}

static void BSP_Button_InitAll(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    for (uint8_t i = 0; i < BSP_BUTTON_COUNT; i++) {
        BSP_EnablePortClock(button_config[i].port);

        GPIO_InitStruct.Pin  = button_config[i].pin;
        GPIO_InitStruct.Pull = button_config[i].pull_mode;
        GPIO_InitStruct.Mode = button_config[i].active_high ? GPIO_MODE_IT_RISING : GPIO_MODE_IT_FALLING;
        HAL_GPIO_Init(button_config[i].port, &GPIO_InitStruct);

        // Priority 6 is safe for both CMSIS-RTOS v2 ISRs and bare-metal
        HAL_NVIC_SetPriority(button_config[i].irq_n, 6, 0);
        HAL_NVIC_EnableIRQ(button_config[i].irq_n);
    }
}

// --- STM32 HAL External Interrupt Callback ---
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    for (uint8_t i = 0; i < BSP_BUTTON_COUNT; i++) {
        if (button_config[i].pin == GPIO_Pin) {
            BSP_Button_TypeDef button = (BSP_Button_TypeDef)i;

#if BSP_USE_RTOS
            // osMessageQueuePut is ISR-safe when timeout == 0
            if (button_queue_id != NULL) {
                osMessageQueuePut(button_queue_id, &button, 0U, 0U);
            }
#else
            button_irq_time[button] = HAL_GetTick();
            button_irq_flag[button] = true;
#endif
            break;
        }
    }
}

// --- Public BSP API ---
void BSP_Init(void) {
    BSP_LED_InitAll();

#if BSP_USE_RTOS
    button_queue_id = osMessageQueueNew(10, sizeof(BSP_Button_TypeDef), NULL);

    const osThreadAttr_t button_thread_attr = {
        .name = "BSP_BtnTask",
        .stack_size = 512 * 4, // CMSIS-RTOS v2 specifies stack size in BYTES
        .priority = (osPriority_t)osPriorityAboveNormal
    };
    button_thread_id = osThreadNew(BSP_ButtonThread, NULL, &button_thread_attr);
#endif

    BSP_Button_InitAll();
}

void BSP_Process(void) {
#if !BSP_USE_RTOS
    for (uint8_t i = 0; i < BSP_BTN_COUNT; i++) {
        if (button_irq_flag[i]) {
            button_irq_flag[i] = false;
            BSP_HandleButtonEvent((BSP_BUTTON_TypeDef)i, button_irq_time[i]);
        }
    }
#endif
}

void BSP_Button_RegisterCallback(BSP_Button_TypeDef button, BSP_Button_Callback_t callback) {
    if (button < BSP_BUTTON_COUNT) {
        button_callbacks[button] = callback;
    }
}

void BSP_LED_On(BSP_LED_TypeDef led) {
    if (led >= BSP_LED_COUNT) {
        return;
    }

    HAL_GPIO_WritePin(led_config[led].port, led_config[led].pin,
                      led_config[led].active_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void BSP_LED_Off(BSP_LED_TypeDef led) {
    if (led >= BSP_LED_COUNT) {
        return;
    }

    HAL_GPIO_WritePin(led_config[led].port, led_config[led].pin,
                      led_config[led].active_high ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void BSP_LED_Toggle(BSP_LED_TypeDef led) {
    if (led >= BSP_LED_COUNT) { 
        return;
    }

    HAL_GPIO_TogglePin(led_config[led].port, led_config[led].pin);
}

bool BSP_Button_IsPressed(BSP_Button_TypeDef button) {
    if (button >= BSP_BUTTON_COUNT) { 
        return false;
    }

    GPIO_PinState raw = HAL_GPIO_ReadPin(button_config[button].port, button_config[button].pin);
    return button_config[button].active_high ? (raw == GPIO_PIN_SET) : (raw == GPIO_PIN_RESET);
}
