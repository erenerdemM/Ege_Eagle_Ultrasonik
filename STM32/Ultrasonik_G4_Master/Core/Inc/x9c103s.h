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
#define DEFAULT_STEP_INCREMENT 4U
#define DEFAULT_SWEEP_SPAN_KHZ 2U
#define DEFAULT_SWEEP_PERIOD_MS 400U

/**
  * @brief  Sets the parametric sweep step increment (1-8).
  * @param  inc: Step increment (1-8).
  * @retval HAL_OK if valid, HAL_ERROR if out of range.
  */
HAL_StatusTypeDef X9C103S_SetStepIncrement(uint8_t inc);

/**
  * @brief  Returns current parametric sweep step increment (1-8).
  * @retval uint8_t step increment.
  */
uint8_t X9C103S_GetStepIncrement(void);

/**
  * @brief  Sets the sweep span in kHz (1-4).
  * @param  span_khz: Sweep span in kHz (1-4).
  * @retval HAL_OK if valid, HAL_ERROR if out of range.
  */
HAL_StatusTypeDef X9C103S_SetSweepSpan(uint8_t span_khz);

/**
  * @brief  Returns current sweep span in kHz (1-4).
  * @retval uint8_t sweep span.
  */
uint8_t X9C103S_GetSweepSpan(void);

/**
  * @brief  Sets the sweep period in ms (100-1000).
  * @param  period_ms: Sweep period in ms (100-1000).
  * @retval HAL_OK if valid, HAL_ERROR if out of range.
  */
HAL_StatusTypeDef X9C103S_SetSweepPeriod(uint16_t period_ms);

/**
  * @brief  Returns current sweep period in ms (100-1000).
  * @retval uint16_t sweep period in ms.
  */
uint16_t X9C103S_GetSweepPeriod(void);

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
  * @brief  Enables/disables local frequency sweep around the selected
  *         28 kHz or 40 kHz center frequency.
  */
void X9C103S_SetSweepEnabled(uint8_t enabled);

/**
  * @brief  Returns whether frequency sweep is currently enabled.
  */
uint8_t X9C103S_IsSweepEnabled(void);

/**
  * @brief  Runs the non-blocking frequency sweep state machine.
  *         Full triangle cycle = 400 ms.
  */
void X9C103S_SweepProcess(void);

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

/**
  * @brief  Returns the target wiper step position (0-99).
  * @retval uint8_t target wiper step.
  */
uint8_t X9C103S_GetTargetStep(void);

/**
  * @brief  Runs PA0 / ADC1 conversion for X9C103S VW analog voltage acquisition.
  */
void PA0_ADC1_Process(void);

/**
  * @brief  Returns latest PA0 / ADC1 raw 12-bit conversion value (0..4095).
  * @retval uint16_t ADC raw count.
  */
uint16_t PA0_ADC1_GetLastRaw(void);

/**
  * @brief  Returns latest PA0 / ADC1 converted voltage in Volts (0.00V .. 3.30V).
  * @retval float Voltage in Volts.
  */
float PA0_ADC1_GetLastVoltage(void);

/**
  * @brief  Stores current wiper position into non-volatile EEPROM memory (takes 25 ms).
  * @retval HAL status.
  */
HAL_StatusTypeDef X9C103S_StorePosition(void);

/**
  * @brief  Returns total cumulative wiper pulses sent since boot.
  * @retval uint32_t total pulse count.
  */
uint32_t X9C103S_GetTotalPulsesSent(void);

#ifdef __cplusplus
}
#endif

#endif /* __X9C103S_H */
