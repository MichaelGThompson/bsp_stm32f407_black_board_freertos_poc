#ifndef BSP_H
#define BSP_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    BSP_LED1 = 0,
    BSP_LED2,
    BSP_LED_COUNT
} BSP_LED_TypeDef;

typedef enum {
    BSP_BUTTON_K0 = 0,
    BSP_BUTTON_K1,
    BSP_BUTTON_COUNT
} BSP_Button_TypeDef;

typedef void (*BSP_Button_Callback_t)(BSP_Button_TypeDef button);

void BSP_Init(void);
void BSP_Process(void); // Required in bare-metal superloop; no-op in RTOS mode

// LED API
void BSP_LED_On(BSP_LED_TypeDef led);
void BSP_LED_Off(BSP_LED_TypeDef led);
void BSP_LED_Toggle(BSP_LED_TypeDef led);

// Button API
bool BSP_Button_IsPressed(BSP_Button_TypeDef button);
void BSP_Button_RegisterCallback(BSP_Button_TypeDef button, BSP_Button_Callback_t callback);

#endif
