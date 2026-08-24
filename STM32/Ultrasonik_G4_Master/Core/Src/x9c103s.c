/**
  ******************************************************************************
  * @file    x9c103s.c
  * @brief   Driver source for X9C103S Digital Potentiometer (Dual Frequency
  *          Selection: 28 kHz / 40 kHz) on STM32G4.
  ******************************************************************************
  */
#include "x9c103s.h"
#include "system_state.h"

static uint8_t s_current_step = 0U;
static uint8_t s_current_freq = 28U;

/* ------------------------------------------------------------
 * Frequency sweep configuration
 *
 * Full cycle:
 *   26 -> 27 -> 28 -> 29 -> 30 -> 29 -> 28 -> 27 -> 26
 *
 * 50 ms per frequency point transition
 * 8 transitions = 400 ms complete triangle cycle.
 * ------------------------------------------------------------ */
#define X9C_SWEEP_PERIOD_MS       400U
#define X9C_SWEEP_POINT_MS         50U
#define X9C_SWEEP_INDEX_COUNT       9U

static const int8_t s_sweep_offsets[X9C_SWEEP_INDEX_COUNT] =
{
  -2, -1, 0, 1, 2, 1, 0, -1, -2
};

static uint8_t  s_sweep_enabled       = 0U;
static uint8_t  s_sweep_center_freq   = 28U;
static uint8_t  s_sweep_index         = 0U;
static uint32_t s_sweep_last_tick     = 0U;
static uint8_t  s_step_increment     = DEFAULT_STEP_INCREMENT;
static uint8_t  s_sweep_span_khz     = DEFAULT_SWEEP_SPAN_KHZ;
static uint16_t s_sweep_period_ms    = DEFAULT_SWEEP_PERIOD_MS;

HAL_StatusTypeDef X9C103S_SetStepIncrement(uint8_t inc)
{
  if (inc < 1U || inc > 8U)
  {
    return HAL_ERROR;
  }
  s_step_increment = inc;
  return HAL_OK;
}

uint8_t X9C103S_GetStepIncrement(void)
{
  return s_step_increment;
}

HAL_StatusTypeDef X9C103S_SetSweepSpan(uint8_t span_khz)
{
  if (span_khz < 1U || span_khz > 4U)
  {
    return HAL_ERROR;
  }
  s_sweep_span_khz = span_khz;
  return HAL_OK;
}

uint8_t X9C103S_GetSweepSpan(void)
{
  return s_sweep_span_khz;
}

HAL_StatusTypeDef X9C103S_SetSweepPeriod(uint16_t period_ms)
{
  if (period_ms < 100U || period_ms > 1000U)
  {
    return HAL_ERROR;
  }
  s_sweep_period_ms = period_ms;
  return HAL_OK;
}

uint16_t X9C103S_GetSweepPeriod(void)
{
  return s_sweep_period_ms;
}

extern ADC_HandleTypeDef hadc1;

static uint8_t s_target_step = 40U;
static volatile uint16_t s_pa0_adc_raw = 0U;
static volatile float s_pa0_adc_voltage = 0.0f;
static volatile uint32_t s_total_pulses_sent = 0U;

/**
  * @brief  Cycle-accurate microsecond delay using Cortex-M4 DWT Cycle Counter.
  *         At 170 MHz CPU clock, 1 microsecond = 170 CPU cycles.
  */
static inline void DWT_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void X9C_DelayUs(uint32_t us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t cycles = us * (SystemCoreClock / 1000000UL);
  while ((DWT->CYCCNT - start) < cycles)
  {
    __NOP();
  }
}

void X9C103S_Init(void)
{
  DWT_Init();

  /* Protect initial wiper zeroing sequence from Zero-Cross / Timer interrupt preemption */
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  /* 1. Ensure CS and INC are idle HIGH, U/D LOW (DOWN direction) */
  HAL_GPIO_WritePin(X9C_CS_GPIO_Port, X9C_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(X9C_INC_GPIO_Port, X9C_INC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(X9C_UD_GPIO_Port, X9C_UD_Pin, GPIO_PIN_RESET); /* Down */

  X9C_DelayUs(10U); /* t_ID / t_DI setup time (>= 2.9 us) */

  /* 2. Select device (CS LOW) to begin wiper zeroing */
  HAL_GPIO_WritePin(X9C_CS_GPIO_Port, X9C_CS_Pin, GPIO_PIN_RESET);
  X9C_DelayUs(5U); /* t_CI setup time (>= 100 ns) */

  /* 3. Send 100 DOWN pulses to drive wiper to physical terminal VL (Tap 0) */
  for (uint8_t i = 0U; i < X9C_MAX_STEPS; i++)
  {
    /* Negative edge (HIGH -> LOW) steps wiper DOWN */
    HAL_GPIO_WritePin(X9C_INC_GPIO_Port, X9C_INC_Pin, GPIO_PIN_RESET);
    X9C_DelayUs(5U); /* t_IL LOW width (>= 1 us) */

    if (i < (X9C_MAX_STEPS - 1U))
    {
      HAL_GPIO_WritePin(X9C_INC_GPIO_Port, X9C_INC_Pin, GPIO_PIN_SET);
      X9C_DelayUs(5U); /* t_IH HIGH width (>= 1 us) */
    }
  }

  /* 4. Datasheet NO-STORE Deselect:
   * Keep INC LOW while taking CS HIGH to prevent triggering an unwanted 20ms EEPROM write. */
  X9C_DelayUs(5U); /* t_IC setup time (>= 1 us) */
  HAL_GPIO_WritePin(X9C_CS_GPIO_Port, X9C_CS_Pin, GPIO_PIN_SET); /* CS HIGH -> Standby (NO STORE) */
  X9C_DelayUs(5U); /* t_CPH NO-STORE deselect time (>= 100 ns) */

  /* Return INC to idle HIGH */
  HAL_GPIO_WritePin(X9C_INC_GPIO_Port, X9C_INC_Pin, GPIO_PIN_SET);
  X9C_DelayUs(10U);

  __set_PRIMASK(primask);

  s_current_step = 0U;
  s_target_step = X9C_STEP_28KHZ;
  s_step_increment = DEFAULT_STEP_INCREMENT;

  /* Set default frequency (28 kHz -> step 40) directly after reset */
  (void)X9C103S_SetFrequency(28U);
}

HAL_StatusTypeDef X9C103S_SetStep(uint8_t target_step)
{
  uint8_t target = target_step;

  if (target >= X9C_MAX_STEPS)
  {
    target = (uint8_t)(X9C_MAX_STEPS - 1U);
  }

  s_target_step = target;

  if (target == s_current_step)
  {
    return HAL_OK;
  }

  uint8_t count;
  GPIO_PinState ud_state;

  if (target > s_current_step)
  {
    ud_state = GPIO_PIN_SET; /* UP towards VH */
    count = (uint8_t)(target - s_current_step);
  }
  else
  {
    ud_state = GPIO_PIN_RESET; /* DOWN towards VL */
    count = (uint8_t)(s_current_step - target);
  }

  /* 1. Set U/D direction pin and wait setup time (t_DI >= 2.9 us) */
  HAL_GPIO_WritePin(X9C_UD_GPIO_Port, X9C_UD_Pin, ud_state);
  X9C_DelayUs(10U);

  /* 2. Select device (CS LOW) (t_CI >= 100 ns) */
  HAL_GPIO_WritePin(X9C_CS_GPIO_Port, X9C_CS_Pin, GPIO_PIN_RESET);
  X9C_DelayUs(5U);

  /* 3. Send step pulses with per-pulse micro critical sections (<15us blackout per pulse) */
  for (uint8_t i = 0U; i < count; i++)
  {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    /* Negative edge (HIGH -> LOW) triggers one wiper step */
    HAL_GPIO_WritePin(X9C_INC_GPIO_Port, X9C_INC_Pin, GPIO_PIN_RESET);
    X9C_DelayUs(5U); /* t_IL LOW width (>= 1 us) */

    if (i < (count - 1U))
    {
      HAL_GPIO_WritePin(X9C_INC_GPIO_Port, X9C_INC_Pin, GPIO_PIN_SET);
      X9C_DelayUs(5U); /* t_IH HIGH width (>= 1 us) */
    }

    __set_PRIMASK(primask);
  }

  s_total_pulses_sent += count;

  /* 4. Deselect device using DATASHEET NO-STORE MODE:
   * Keep INC LOW while taking CS HIGH to prevent triggering an unwanted 20ms EEPROM write. */
  X9C_DelayUs(5U); /* t_IC setup time (>= 1 us) */
  HAL_GPIO_WritePin(X9C_CS_GPIO_Port, X9C_CS_Pin, GPIO_PIN_SET); /* CS HIGH -> Standby (NO STORE) */
  X9C_DelayUs(5U); /* t_CPH NO-STORE deselect time (>= 100 ns) */

  /* Return INC to idle HIGH */
  HAL_GPIO_WritePin(X9C_INC_GPIO_Port, X9C_INC_Pin, GPIO_PIN_SET);
  X9C_DelayUs(10U);

  s_current_step = target;
  return HAL_OK;
}

HAL_StatusTypeDef X9C103S_StorePosition(void)
{
  /* Datasheet Store Mode:
   * 1. CS LOW with INC HIGH
   * 2. CS transitions HIGH while INC is HIGH
   * 3. Must wait t_CPH_STORE (>= 20 ms) before any subsequent operation
   */
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  HAL_GPIO_WritePin(X9C_INC_GPIO_Port, X9C_INC_Pin, GPIO_PIN_SET);
  X9C_DelayUs(10U);
  HAL_GPIO_WritePin(X9C_CS_GPIO_Port, X9C_CS_Pin, GPIO_PIN_RESET);
  X9C_DelayUs(10U);
  HAL_GPIO_WritePin(X9C_CS_GPIO_Port, X9C_CS_Pin, GPIO_PIN_SET);

  __set_PRIMASK(primask);

  HAL_Delay(25U); /* 25 ms EEPROM non-volatile write wait (tCPH_STORE >= 20 ms) */
  return HAL_OK;
}

uint32_t X9C103S_GetTotalPulsesSent(void)
{
  return s_total_pulses_sent;
}

HAL_StatusTypeDef X9C103S_SetFrequency(uint8_t freq_khz)
{
  if (freq_khz < 28U || freq_khz > 40U)
  {
    return HAL_ERROR;
  }

  /* Linear calibration mapping across 28..40 kHz range (28 kHz -> Step 40, 40 kHz -> Step 90) */
  uint8_t target_step;
  if (freq_khz == 28U)
  {
    target_step = X9C_STEP_28KHZ; /* Exact Step 40 */
  }
  else if (freq_khz == 40U)
  {
    target_step = X9C_STEP_40KHZ; /* Exact Step 90 */
  }
  else
  {
    target_step = (uint8_t)(X9C_STEP_28KHZ + (((uint16_t)(freq_khz - 28U) * 50U + 6U) / 12U));
  }

  HAL_StatusTypeDef status = X9C103S_SetStep(target_step);
  if (status != HAL_OK)
  {
    return status;
  }

  s_current_freq = freq_khz;
  s_sweep_center_freq = freq_khz;
  return HAL_OK;
}

void PA0_ADC1_Process(void)
{
  if (HAL_ADC_Start(&hadc1) == HAL_OK)
  {
    if (HAL_ADC_PollForConversion(&hadc1, 2) == HAL_OK)
    {
      s_pa0_adc_raw = (uint16_t)HAL_ADC_GetValue(&hadc1);
      s_pa0_adc_voltage = ((float)s_pa0_adc_raw * 3.30f) / 4095.0f;
    }
    HAL_ADC_Stop(&hadc1);
  }
}

uint16_t PA0_ADC1_GetLastRaw(void)
{
  return s_pa0_adc_raw;
}

float PA0_ADC1_GetLastVoltage(void)
{
  return s_pa0_adc_voltage;
}

uint8_t X9C103S_GetTargetStep(void)
{
  return s_target_step;
}

void X9C103S_SetSweepEnabled(uint8_t enabled)
{
  if (enabled != 0U)
  {
    /* Sweep can only be enabled around the actual selected center. */
    if (s_current_freq != 28U && s_current_freq != 40U)
    {
      s_current_freq = 28U;
      (void)X9C103S_SetFrequency(28U);
    }

    s_sweep_center_freq = s_current_freq;
    s_sweep_index = 0U;
    s_sweep_last_tick = HAL_GetTick();
    s_sweep_enabled = 1U;

    /*
     * Parametric start immediately at the lower endpoint:
     * target_step = BASE_STEP + (offset * STEP_INCREMENT)
     * Wiper is only driven when system mode is SYS_MODE_RUNNING.
     * In SYS_MODE_IDLE, sweep selection is armed (sweep_enabled = 1) without wiper stepping.
     */
    if (g_system_state.mode == SYS_MODE_RUNNING)
    {
      int16_t base_step = (s_sweep_center_freq == 40U) ? (int16_t)X9C_STEP_40KHZ : (int16_t)X9C_STEP_28KHZ;
      int16_t target_step = base_step + ((int16_t)s_sweep_offsets[s_sweep_index] * (int16_t)s_step_increment);

      if (target_step < 0)
      {
        target_step = 0;
      }
      if (target_step >= (int16_t)X9C_MAX_STEPS)
      {
        target_step = (int16_t)(X9C_MAX_STEPS - 1U);
      }

      (void)X9C103S_SetStep((uint8_t)target_step);
    }
  }
  else
  {
    uint8_t was_enabled = s_sweep_enabled;
    s_sweep_enabled = 0U;
    s_sweep_index = 0U;
    s_sweep_last_tick = HAL_GetTick();

    /* Always restore exact center frequency when sweep stops. */
    if (was_enabled != 0U || s_current_step != ((s_sweep_center_freq == 40U) ? X9C_STEP_40KHZ : X9C_STEP_28KHZ))
    {
      (void)X9C103S_SetFrequency(s_sweep_center_freq);
    }
  }
}

uint8_t X9C103S_IsSweepEnabled(void)
{
  return s_sweep_enabled;
}

void X9C103S_SweepProcess(void)
{
  if (s_sweep_enabled == 0U || g_system_state.mode != SYS_MODE_RUNNING)
  {
    return;
  }

  uint32_t now = HAL_GetTick();
  uint32_t point_ms = (uint32_t)(s_sweep_period_ms / (X9C_SWEEP_INDEX_COUNT - 1U));
  if (point_ms == 0U)
  {
    point_ms = 50U;
  }

  if ((uint32_t)(now - s_sweep_last_tick) < point_ms)
  {
    return;
  }

  s_sweep_last_tick = now;

  /*
   * Advance through:
   * -2, -1, 0, +1, +2, +1, 0, -1, -2
   */
  if (s_sweep_index < (X9C_SWEEP_INDEX_COUNT - 1U))
  {
    s_sweep_index++;
  }
  else
  {
    s_sweep_index = 0U;
  }

  int16_t base_step = (s_sweep_center_freq == 40U) ? (int16_t)X9C_STEP_40KHZ : (int16_t)X9C_STEP_28KHZ;
  int16_t target_step = base_step + ((int16_t)s_sweep_offsets[s_sweep_index] * (int16_t)s_step_increment);

  if (target_step < 0)
  {
    target_step = 0;
  }
  if (target_step >= (int16_t)X9C_MAX_STEPS)
  {
    target_step = (int16_t)(X9C_MAX_STEPS - 1U);
  }

  (void)X9C103S_SetStep((uint8_t)target_step);
}

uint8_t X9C103S_GetCurrentStep(void)
{
  return s_current_step;
}

uint8_t X9C103S_GetCurrentFrequency(void)
{
  return s_current_freq;
}
