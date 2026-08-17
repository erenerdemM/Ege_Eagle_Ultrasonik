/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
/* Tank ID Flash override and Commissioning (see main.c) */
uint8_t TankId_Load(uint8_t *out_state);
uint8_t TankId_SaveAndVerifyOverride(uint8_t new_id, uint8_t state);
uint8_t TankId_EraseOverride(void);
void TankId_StartStaging(void);
void TankId_ProcessStagingTimeout(void);
void TankId_CancelStaging(void);
void TankId_ConfirmStaging(uint8_t final_id);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define B1_Pin GPIO_PIN_13
#define B1_GPIO_Port GPIOC
#define B1_EXTI_IRQn EXTI15_10_IRQn
#define RCC_OSC32_IN_Pin GPIO_PIN_14
#define RCC_OSC32_IN_GPIO_Port GPIOC
#define RCC_OSC32_OUT_Pin GPIO_PIN_15
#define RCC_OSC32_OUT_GPIO_Port GPIOC
#define RCC_OSC_IN_Pin GPIO_PIN_0
#define RCC_OSC_IN_GPIO_Port GPIOF
#define RCC_OSC_OUT_Pin GPIO_PIN_1
#define RCC_OSC_OUT_GPIO_Port GPIOF
#define LPUART1_TX_Pin GPIO_PIN_2
#define LPUART1_TX_GPIO_Port GPIOA
#define LPUART1_RX_Pin GPIO_PIN_3
#define LPUART1_RX_GPIO_Port GPIOA
#define LD2_Pin GPIO_PIN_5
#define LD2_GPIO_Port GPIOA
#define X9C_CS_Pin GPIO_PIN_12
#define X9C_CS_GPIO_Port GPIOB
#define X9C_UD_Pin GPIO_PIN_13
#define X9C_UD_GPIO_Port GPIOB
#define X9C_INC_Pin GPIO_PIN_14
#define X9C_INC_GPIO_Port GPIOB
#define HEATER_RELAY_Pin GPIO_PIN_15
#define HEATER_RELAY_GPIO_Port GPIOB
#define T_SWDIO_Pin GPIO_PIN_13
#define T_SWDIO_GPIO_Port GPIOA
#define T_SWCLK_Pin GPIO_PIN_14
#define T_SWCLK_GPIO_Port GPIOA
#define T_SWO_Pin GPIO_PIN_3
#define T_SWO_GPIO_Port GPIOB

/* BENCH TEST PHYSICAL LOOPBACK PINS */
#define HEATER_TEST_FB_Pin GPIO_PIN_4
#define HEATER_TEST_FB_GPIO_Port GPIOA
#define TRIAC_TEST_FB_Pin GPIO_PIN_6
#define TRIAC_TEST_FB_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */
#define RS485_DE_Pin GPIO_PIN_1
#define RS485_DE_GPIO_Port GPIOB

#define TRIAC_GATE_Pin GPIO_PIN_6
#define TRIAC_GATE_GPIO_Port GPIOC
#define ZERO_CROSS_Pin GPIO_PIN_7
#define ZERO_CROSS_GPIO_Port GPIOC


/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
