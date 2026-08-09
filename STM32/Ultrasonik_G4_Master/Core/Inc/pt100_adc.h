/**
  ******************************************************************************
  * @file    pt100_adc.h
  * @brief   PT100 temperature acquisition via OPAMP3 (PGA=2) + ADC2.
  ******************************************************************************
  */
#ifndef __PT100_ADC_H
#define __PT100_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

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
 * even if it sits mid-scale instead of pinned to a rail.
 * NOTE: a hardware bias resistor on the OPAMP3 input is still recommended so
 * a disconnected sensor is guaranteed to land outside this window rather
 * than merely floating to whatever stray voltage it happens to pick up. */
#define PT100_VALID_TEMP_MIN_C   (-10.0f)
#define PT100_VALID_TEMP_MAX_C   (110.0f)

/**
  * @brief Starts OPAMP3. Call once after MX_OPAMP3_Init()/MX_ADC2_Init().
  */
void PT100_ADC_Init(void);

/**
  * @brief Performs one ADC2 conversion, updates g_system_state.current_temp_c
  *        and detects open/short PT100 faults. Poll from the main superloop.
  */
void PT100_ADC_Process(void);
#include <stdint.h>
/* HIL_DEEP_DEBUG: raw ADC2 sample from the most recent PT100_ADC_Process() conversion */
uint32_t PT100_ADC_GetLastRaw(void);

#ifdef __cplusplus
}
#endif

#endif /* __PT100_ADC_H */
