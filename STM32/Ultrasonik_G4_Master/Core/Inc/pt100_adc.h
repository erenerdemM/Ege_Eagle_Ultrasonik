/**
  ******************************************************************************
  * @file    pt100_adc.h
  * @brief   PT100 temperature acquisition, validation, digital filtering, and trend calculation.
  ******************************************************************************
  */
#ifndef __PT100_ADC_H
#define __PT100_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Linear calibration: temp_c = (adc_raw * PT100_CAL_SLOPE) + PT100_CAL_OFFSET.
 * Derive both constants from a two-point calibration against a reference
 * thermometer at the OPAMP3/ADC2 gain configured in MX_OPAMP3_Init(). */
#define PT100_CAL_SLOPE   (0.0327f)
#define PT100_CAL_OFFSET  (-20.0f)

/* Realistic RTD operating window for this ultrasonic bath: setpoints are
 * clamped to 0-90 degC (see esp32_uart.c TEMP_SETPOINT_MAX) and indoor
 * ambient never drops much below 0 degC. Any ADC reading translating to a
 * temperature outside this margin-padded window cannot be a genuine PT100
 * reading and is treated as an open/disconnected sensor (see pt100_adc.c),
 * even if it sits mid-scale instead of pinned to a rail. */
#define PT100_VALID_TEMP_MIN_C   (-10.0f)
#define PT100_VALID_TEMP_MAX_C   (110.0f)

/* Low-pass exponential filter smoothing factor (0.0 < alpha <= 1.0) */
#define PT100_FILTER_ALPHA       (0.25f)

typedef enum
{
  PT100_STATUS_VALID = 0,
  PT100_STATUS_OPEN,
  PT100_STATUS_SHORT,
  PT100_STATUS_OUT_OF_RANGE
} Pt100Status_t;

/**
  * @brief Starts OPAMP3 and initializes filter state. Call once after MX_OPAMP3_Init()/MX_ADC2_Init().
  */
void PT100_ADC_Init(void);

/**
  * @brief Performs one ADC2 conversion, validates sensor integrity, updates
  *        g_system_state.current_temp_c, applies low-pass filtering, and computes dT/dt.
  *        Poll from the main superloop.
  */
void PT100_ADC_Process(void);

/* Temperature and sensor status accessors */
float PT100_GetRawTemp(void);
float PT100_GetFilteredTemp(void);
float PT100_GetTempRate(void);
Pt100Status_t PT100_GetStatus(void);
uint8_t PT100_IsTempValid(void);

/* HIL_DEEP_DEBUG: raw ADC2 sample from the most recent PT100_ADC_Process() conversion */
uint32_t PT100_ADC_GetLastRaw(void);

#ifdef __cplusplus
}
#endif

#endif /* __PT100_ADC_H */

