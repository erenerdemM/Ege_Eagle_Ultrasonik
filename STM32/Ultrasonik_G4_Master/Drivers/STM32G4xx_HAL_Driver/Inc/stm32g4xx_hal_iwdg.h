/**
  ******************************************************************************
  * @file    stm32g4xx_hal_iwdg.h
  * @brief   Header file of IWDG HAL module.
  ******************************************************************************
  */
#ifndef __STM32G4xx_HAL_IWDG_H
#define __STM32G4xx_HAL_IWDG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal_def.h"

typedef struct
{
  uint32_t Prescaler;  /*!< Select the prescaler of the IWDG.
                            This parameter can be a value of @ref IWDG_Prescaler */
  uint32_t Reload;     /*!< Specifies the IWDG Down-counter Reload value.
                            This parameter must be a number between Min_Data = 0 and Max_Data = 0x0FFF */
  uint32_t Window;     /*!< Specifies the window value to be compared to the down-counter.
                            This parameter must be a number between Min_Data = 0 and Max_Data = 0x0FFF */
} IWDG_InitTypeDef;

typedef struct
{
  IWDG_TypeDef                 *Instance;  /*!< Register base address    */
  IWDG_InitTypeDef             Init;      /*!< IWDG required parameters */
} IWDG_HandleTypeDef;

/* Prescaler Constants */
#define IWDG_PRESCALER_4            0x00000000U
#define IWDG_PRESCALER_8            IWDG_PR_PR_0
#define IWDG_PRESCALER_16           IWDG_PR_PR_1
#define IWDG_PRESCALER_32           (IWDG_PR_PR_1 | IWDG_PR_PR_0)
#define IWDG_PRESCALER_64           IWDG_PR_PR_2
#define IWDG_PRESCALER_128          (IWDG_PR_PR_2 | IWDG_PR_PR_0)
#define IWDG_PRESCALER_256          (IWDG_PR_PR_2 | IWDG_PR_PR_1)

#define IWDG_WINDOW_DISABLE         IWDG_RLR_RL

/* Key register values */
#define IWDG_KEY_RELOAD             0x0000AAAAU
#define IWDG_KEY_ENABLE             0x0000CCCCU
#define IWDG_KEY_WRITE_ACCESS_ENABLE 0x00005555U
#define IWDG_KEY_WRITE_ACCESS_DISABLE 0x00000000U

HAL_StatusTypeDef HAL_IWDG_Init(IWDG_HandleTypeDef *hiwdg);
HAL_StatusTypeDef HAL_IWDG_Refresh(IWDG_HandleTypeDef *hiwdg);

#ifdef __cplusplus
}
#endif

#endif /* __STM32G4xx_HAL_IWDG_H */
