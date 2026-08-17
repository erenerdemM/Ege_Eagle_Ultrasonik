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

static uint8_t X9C103S_SweepStepForFrequency(uint8_t freq_khz)
{
  /*
   * Two-point calibration:
   *   28 kHz -> step 40
   *   40 kHz -> step 90
   *
   * Linear interpolation is used for the intermediate sweep points.
   */
  if (freq_khz <= 28U)
  {
    int32_t delta = (int32_t)freq_khz - 28;
    int32_t step = 40 + ((delta * 50) / 12);

    if (step < 0)
    {
      step = 0;
    }

    if (step > 99)
    {
      step = 99;
    }

    return (uint8_t)step;
  }

  {
    int32_t delta = (int32_t)freq_khz - 28;
    int32_t step = 40 + ((delta * 50 + 6) / 12);

    if (step < 0)
    {
      step = 0;
    }

    if (step > 99)
    {
      step = 99;
    }

    return (uint8_t)step;
  }
}

/**
  * @brief  Short microsecond-scale delay helper for X9C103S pulse timing.
  *         STM32G4 operates at 170 MHz (1 us ~ 170 cycles).
  * @param  us: Delay duration in microseconds.
  */
static void X9C_DelayUs(uint32_t us)
{
  uint32_t count = us * 45U;
  for (volatile uint32_t i = 0U; i < count; i++)
  {
    __asm__ volatile("");
  }
}

void X9C103S_Init(void)
{
  /* Protect initial wiper zeroing sequence from Zero-Cross / Timer interrupt preemption */
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  /* Ensure CS and INC are idle HIGH, U/D LOW */
  HAL_GPIO_WritePin(X9C_CS_GPIO_Port, X9C_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(X9C_INC_GPIO_Port, X9C_INC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(X9C_UD_GPIO_Port, X9C_UD_Pin, GPIO_PIN_RESET); /* Down */

  X9C_DelayUs(5U); /* t_ID setup time (>= 2.9 us) */

  /* Reset wiper to known zero position by sending 100 DOWN pulses */
  HAL_GPIO_WritePin(X9C_CS_GPIO_Port, X9C_CS_Pin, GPIO_PIN_RESET);
  X9C_DelayUs(3U); /* t_CI setup time (>= 100 ns) */

  for (uint8_t i = 0U; i < X9C_MAX_STEPS; i++)
  {
    HAL_GPIO_WritePin(X9C_INC_GPIO_Port, X9C_INC_Pin, GPIO_PIN_RESET);
    X9C_DelayUs(3U); /* t_INC LOW width (>= 1 us) */
    HAL_GPIO_WritePin(X9C_INC_GPIO_Port, X9C_INC_Pin, GPIO_PIN_SET);
    X9C_DelayUs(3U); /* t_INC HIGH width (>= 1 us) */
  }

  /* Deselect CS while INC is HIGH to latch position */
  HAL_GPIO_WritePin(X9C_CS_GPIO_Port, X9C_CS_Pin, GPIO_PIN_SET);
  X9C_DelayUs(10U); /* t_CPH deselect time (>= 10 us) */

  __set_PRIMASK(primask);

  s_current_step = 0U;
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

  if (target == s_current_step)
  {
    return HAL_OK;
  }

  uint8_t count;
  GPIO_PinState ud_state;

  if (target > s_current_step)
  {
    ud_state = GPIO_PIN_SET; /* UP */
    count = (uint8_t)(target - s_current_step);
  }
  else
  {
    ud_state = GPIO_PIN_RESET; /* DOWN */
    count = (uint8_t)(s_current_step - target);
  }

  /* 1. Set U/D direction pin and wait setup time (Interrupts unblocked) */
  HAL_GPIO_WritePin(X9C_UD_GPIO_Port, X9C_UD_Pin, ud_state);
  X9C_DelayUs(5U); /* t_ID setup time (>= 2.9 us) */

  /* 2. Select device (CS LOW) (Interrupts unblocked) */
  HAL_GPIO_WritePin(X9C_CS_GPIO_Port, X9C_CS_Pin, GPIO_PIN_RESET);
  X9C_DelayUs(3U); /* t_CI setup time (>= 100 ns) */

  /* 3. Send step pulses with per-pulse micro critical sections (<10us blackout per pulse) */
  for (uint8_t i = 0U; i < count; i++)
  {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    HAL_GPIO_WritePin(X9C_INC_GPIO_Port, X9C_INC_Pin, GPIO_PIN_RESET);
    X9C_DelayUs(3U); /* t_INC LOW width (>= 1 us) */
    HAL_GPIO_WritePin(X9C_INC_GPIO_Port, X9C_INC_Pin, GPIO_PIN_SET);
    X9C_DelayUs(3U); /* t_INC HIGH width (>= 1 us) */

    __set_PRIMASK(primask);
  }

  /* 4. Deselect device (CS HIGH while INC is HIGH) to store/latch position (Interrupts unblocked) */
  HAL_GPIO_WritePin(X9C_CS_GPIO_Port, X9C_CS_Pin, GPIO_PIN_SET);
  X9C_DelayUs(10U); /* t_CPH deselect time (>= 10 us) */

  s_current_step = target;
  return HAL_OK;
}

HAL_StatusTypeDef X9C103S_SetFrequency(uint8_t freq_khz)
{
  if (freq_khz == 28U)
  {
    (void)X9C103S_SetStep(X9C_STEP_28KHZ);
    s_current_freq = 28U;
    return HAL_OK;
  }
  else if (freq_khz == 40U)
  {
    (void)X9C103S_SetStep(X9C_STEP_40KHZ);
    s_current_freq = 40U;
    return HAL_OK;
  }
  else
  {
    return HAL_ERROR;
  }
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
    s_sweep_enabled = 0U;
    s_sweep_index = 0U;
    s_sweep_last_tick = HAL_GetTick();

    /* Always restore exact center frequency when sweep stops. */
    (void)X9C103S_SetFrequency(s_sweep_center_freq);
  }
}

uint8_t X9C103S_IsSweepEnabled(void)
{
  return s_sweep_enabled;
}

void X9C103S_SweepProcess(void)
{
  if (s_sweep_enabled == 0U)
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
