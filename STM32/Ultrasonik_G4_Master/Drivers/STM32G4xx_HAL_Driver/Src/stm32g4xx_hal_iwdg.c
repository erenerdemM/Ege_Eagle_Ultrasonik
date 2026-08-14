/**
  ******************************************************************************
  * @file    stm32g4xx_hal_iwdg.c
  * @brief   IWDG HAL module driver.
  ******************************************************************************
  */
#include "stm32g4xx_hal.h"

HAL_StatusTypeDef HAL_IWDG_Init(IWDG_HandleTypeDef *hiwdg)
{
  if (hiwdg == NULL)
  {
    return HAL_ERROR;
  }

  /* Enable write access to IWDG_PR and IWDG_RLR registers */
  hiwdg->Instance->KR = IWDG_KEY_WRITE_ACCESS_ENABLE;

  /* Write prescaler */
  hiwdg->Instance->PR = hiwdg->Init.Prescaler;

  /* Write reload value */
  hiwdg->Instance->RLR = hiwdg->Init.Reload;

  /* Wait for register update completion */
  while (hiwdg->Instance->SR != 0U)
  {
  }

  /* Enable IWDG */
  hiwdg->Instance->KR = IWDG_KEY_ENABLE;

  return HAL_OK;
}

HAL_StatusTypeDef HAL_IWDG_Refresh(IWDG_HandleTypeDef *hiwdg)
{
  if (hiwdg == NULL)
  {
    return HAL_ERROR;
  }

  /* Reload IWDG counter */
  hiwdg->Instance->KR = IWDG_KEY_RELOAD;

  return HAL_OK;
}
