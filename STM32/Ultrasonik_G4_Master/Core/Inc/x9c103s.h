/**
  ******************************************************************************
  * @file    x9c103s.h
  * @brief   Driver header for X9C103S Digital Potentiometer (Dual Frequency
  *          Selection: 28 kHz / 40 kHz) on STM32G4.
  ******************************************************************************
  */
#ifndef __X9C103S_H
#define __X9C103S_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Dual frequency preset step values (0..99 wiper range) */
#define X9C_STEP_28KHZ 40U
#define X9C_STEP_40KHZ 90U
#define X9C_MAX_STEPS   100U

/**
  * @brief  Initializes the X9C103S digital potentiometer pins and resets wiper
  *         to step 0, then sets default 28 kHz position (step 40).
  */
void X9C103S_Init(void);

/**
  * @brief  Moves wiper to a specific step position (0-99).
  * @param  step: Target wiper step (0-99).
  * @retval HAL status.
  */
HAL_StatusTypeDef X9C103S_SetStep(uint8_t step);

/**
  * @brief  Sets dual frequency position (28 kHz -> step 40, 40 kHz -> step 90).
  * @param  freq_khz: 28 or 40.
  * @retval HAL_OK if valid frequency set, HAL_ERROR if unsupported value.
  */
HAL_StatusTypeDef X9C103S_SetFrequency(uint8_t freq_khz);

/**
  * @brief  Returns the current wiper step position (0-99).
  * @retval uint8_t wiper step.
  */
uint8_t X9C103S_GetCurrentStep(void);

/**
  * @brief  Returns the currently selected frequency (28 or 40).
  * @retval uint8_t frequency in kHz.
  */
uint8_t X9C103S_GetCurrentFrequency(void);

#ifdef __cplusplus
}
#endif

#endif /* __X9C103S_H */
