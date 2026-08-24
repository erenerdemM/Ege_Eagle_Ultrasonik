/**
  ******************************************************************************
  * @file    pt100_adc.c
  * @brief   PT100 temperature acquisition, validation, digital filtering, and trend calculation.
  ******************************************************************************
  */
#include "pt100_adc.h"
#include "system_state.h"
#include "main.h"
#include <stdint.h>

#ifndef PT100_BENCH_TEST_MODE
#define PT100_BENCH_TEST_MODE 1
#endif

extern ADC_HandleTypeDef hadc2;
extern OPAMP_HandleTypeDef hopamp3;

/* 12-bit ADC raw thresholds treated as a disconnected/shorted PT100 */
#define ADC_RAW_OPEN_THRESHOLD    4090u
#define ADC_RAW_SHORT_THRESHOLD   5u

#define ADC_RAW_VALID_MIN  ((uint32_t)(((PT100_VALID_TEMP_MIN_C) - (PT100_CAL_OFFSET)) / (PT100_CAL_SLOPE)))
#define ADC_RAW_VALID_MAX  ((uint32_t)(((PT100_VALID_TEMP_MAX_C) - (PT100_CAL_OFFSET)) / (PT100_CAL_SLOPE)))

#define PT100_SAMPLE_INTERVAL_MS  (50u)

/* Internal module state */
static volatile uint32_t s_last_adc_raw = 0u;
static float s_raw_temp_c = 0.0f;
static float s_filtered_temp_c = 0.0f;
static uint8_t s_filter_initialized = 0u;

static float s_prev_temp_c = 0.0f;
static uint32_t s_last_sample_tick = 0u;
static uint32_t s_last_trend_tick = 0u;
static float s_temp_rate_c_per_s = 0.0f;

static Pt100Status_t s_sensor_status = PT100_STATUS_OPEN;

void PT100_ADC_Init(void)
{
  HAL_OPAMP_Start(&hopamp3);
  s_last_adc_raw = 0u;
  s_raw_temp_c = 0.0f;
  s_filtered_temp_c = 0.0f;
  s_filter_initialized = 0u;
  s_prev_temp_c = 0.0f;
  s_last_sample_tick = HAL_GetTick() - PT100_SAMPLE_INTERVAL_MS;
  s_last_trend_tick = HAL_GetTick();
  s_temp_rate_c_per_s = 0.0f;
  s_sensor_status = PT100_STATUS_OPEN;
}

void PT100_ADC_Process(void)
{
  uint32_t now = HAL_GetTick();
  if ((now - s_last_sample_tick) < PT100_SAMPLE_INTERVAL_MS)
  {
    return;
  }
  s_last_sample_tick = now;

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
  s_last_adc_raw = adc_raw;

  /* 1. Validation Layer */
  if (adc_raw >= ADC_RAW_OPEN_THRESHOLD)
  {
    s_sensor_status = PT100_STATUS_OPEN;
  }
  else if (adc_raw <= ADC_RAW_SHORT_THRESHOLD)
  {
    s_sensor_status = PT100_STATUS_SHORT;
  }
  else if (adc_raw < ADC_RAW_VALID_MIN || adc_raw > ADC_RAW_VALID_MAX)
  {
    s_sensor_status = PT100_STATUS_OUT_OF_RANGE;
  }
  else
  {
    s_sensor_status = PT100_STATUS_VALID;
  }

  if (s_sensor_status != PT100_STATUS_VALID)
  {
#if PT100_BENCH_TEST_MODE
    /* Bench test fallback: If physical PT100 is disconnected on bench, mock safe ambient (25.0 C) */
    g_system_state.fault_flags &= (uint8_t)~(FAULT_PT100_OPEN | FAULT_PT100_SHORT);
    s_raw_temp_c = 25.0f;
    if (s_filter_initialized == 0u)
    {
      s_filtered_temp_c = 25.0f;
      s_prev_temp_c = 25.0f;
      s_filter_initialized = 1u;
    }
    else
    {
      s_filtered_temp_c = s_filtered_temp_c + (PT100_FILTER_ALPHA * (25.0f - s_filtered_temp_c));
    }
    g_system_state.current_temp_c = s_filtered_temp_c;
    return;
#else
    uint8_t fault = (s_sensor_status == PT100_STATUS_SHORT) ? FAULT_PT100_SHORT : FAULT_PT100_OPEN;
    g_system_state.current_temp_c = 0.0f;
    s_raw_temp_c = 0.0f;
    s_filtered_temp_c = 0.0f;
    s_filter_initialized = 0u;
    SystemState_SafeStop(STOP_REASON_SENSOR_FAULT);
    g_system_state.fault_flags |= fault;
    return;
#endif
  }

  /* 2. Acquisition & Conversion */
  g_system_state.fault_flags &= (uint8_t)~(FAULT_PT100_OPEN | FAULT_PT100_SHORT);
  s_raw_temp_c = ((float)adc_raw * PT100_CAL_SLOPE) + PT100_CAL_OFFSET;

  /* 3. Digital Filtering (Exponential Moving Average) */
  if (s_filter_initialized == 0u)
  {
    s_filtered_temp_c = s_raw_temp_c;
    s_prev_temp_c = s_raw_temp_c;
    s_last_trend_tick = HAL_GetTick();
    s_filter_initialized = 1u;
  }
  else
  {
    s_filtered_temp_c = s_filtered_temp_c + (PT100_FILTER_ALPHA * (s_raw_temp_c - s_filtered_temp_c));
  }

  g_system_state.current_temp_c = s_filtered_temp_c;

  /* 4. Temperature Trend (dT/dt in degC/s) */
  uint32_t dt_ms = now - s_last_trend_tick;
  if (dt_ms >= 1000u)
  {
    float dt_s = (float)dt_ms / 1000.0f;
    s_temp_rate_c_per_s = (s_filtered_temp_c - s_prev_temp_c) / dt_s;
    s_prev_temp_c = s_filtered_temp_c;
    s_last_trend_tick = now;
  }
}

float PT100_GetRawTemp(void)
{
  return s_raw_temp_c;
}

float PT100_GetFilteredTemp(void)
{
  return s_filtered_temp_c;
}

float PT100_GetTempRate(void)
{
  return s_temp_rate_c_per_s;
}

Pt100Status_t PT100_GetStatus(void)
{
  return s_sensor_status;
}

uint8_t PT100_IsTempValid(void)
{
  return (s_sensor_status == PT100_STATUS_VALID) ? 1u : 0u;
}

uint32_t PT100_ADC_GetLastRaw(void)
{
  return s_last_adc_raw;
}
