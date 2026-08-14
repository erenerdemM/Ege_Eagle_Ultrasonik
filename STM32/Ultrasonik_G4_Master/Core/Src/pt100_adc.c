/**
  ******************************************************************************
  * @file    pt100_adc.c
  * @brief   PT100 temperature acquisition via OPAMP3 (PGA=2) + ADC2.
  ******************************************************************************
  */
#include "pt100_adc.h"
#include "system_state.h"
#include "main.h"
#include <stdint.h>

extern ADC_HandleTypeDef hadc2;
extern OPAMP_HandleTypeDef hopamp3;


/* 12-bit ADC raw thresholds treated as a disconnected/shorted PT100 */
#define ADC_RAW_OPEN_THRESHOLD    4090u
#define ADC_RAW_SHORT_THRESHOLD   5u

/* adc_raw = (temp_c - PT100_CAL_OFFSET) / PT100_CAL_SLOPE, derived from the
 * realistic RTD window in pt100_adc.h. Any reading outside [MIN,MAX] (even
 * if not pinned to a rail) is treated as an open/floating input. */
#define ADC_RAW_VALID_MIN  ((uint32_t)(((PT100_VALID_TEMP_MIN_C) - (PT100_CAL_OFFSET)) / (PT100_CAL_SLOPE)))
#define ADC_RAW_VALID_MAX  ((uint32_t)(((PT100_VALID_TEMP_MAX_C) - (PT100_CAL_OFFSET)) / (PT100_CAL_SLOPE)))

/* HIL_DEEP_DEBUG: raw ADC2 sample, mirrored for white-box readout via PT100_ADC_GetLastRaw() */
static volatile uint32_t last_adc_raw = 0u;

void PT100_ADC_Init(void)
{
  HAL_OPAMP_Start(&hopamp3);
}

void PT100_ADC_Process(void)
{
  uint32_t adc_raw;

  if (HAL_ADC_Start(&hadc2) != HAL_OK)
  {
    return;
  }

  if (HAL_ADC_PollForConversion(&hadc2, 10) != HAL_OK)
  {
    HAL_ADC_Stop(&hadc2);
    return;
  }

  adc_raw = HAL_ADC_GetValue(&hadc2);
  HAL_ADC_Stop(&hadc2);
  last_adc_raw = adc_raw;  // HIL_DEEP_DEBUG

  if (adc_raw >= ADC_RAW_OPEN_THRESHOLD || adc_raw <= ADC_RAW_SHORT_THRESHOLD ||
      adc_raw < ADC_RAW_VALID_MIN || adc_raw > ADC_RAW_VALID_MAX)
  {
#if ZC_BENCH_TEST_MODE
    /* Bench test mode: Disconnected PT100 sensor on bench setup defaults to ambient (25.0 C) */
    g_system_state.fault_flags &= (uint8_t)~(FAULT_PT100_OPEN | FAULT_PT100_SHORT);
    g_system_state.current_temp_c = 25.0f;
    return;
#else
    /* Pinned to a rail (open/short) or outside the realistically expected
     * RTD range for this bath: an unbiased/floating input reading a
     * plausible-looking mid-scale value is just as invalid as a rail value,
     * so it must not be trusted at face value. */
    uint8_t fault = (adc_raw <= ADC_RAW_SHORT_THRESHOLD) ? FAULT_PT100_SHORT : FAULT_PT100_OPEN;
    g_system_state.current_temp_c = 0.0f; /* ESP32 shows "--.-" instead of a false/stale reading */
    SystemState_SafeStop(STOP_REASON_SENSOR_FAULT);
    g_system_state.fault_flags |= fault;
    return;
#endif
  }

  g_system_state.fault_flags &= (uint8_t)~(FAULT_PT100_OPEN | FAULT_PT100_SHORT);
  g_system_state.current_temp_c = ((float)adc_raw * PT100_CAL_SLOPE) + PT100_CAL_OFFSET;
}

/* HIL_DEEP_DEBUG */
uint32_t PT100_ADC_GetLastRaw(void)
{
  return last_adc_raw;
}
