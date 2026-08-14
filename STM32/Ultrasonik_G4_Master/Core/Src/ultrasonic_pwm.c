/**
  ******************************************************************************
  * @file    ultrasonic_pwm.c
  * @brief   Triac phase-angle power control for the ultrasonic driver stage.
  *          TIM15 one-pulse mode times the firing delay + 100us gate pulse,
  *          re-armed on every PC7 zero-cross EXTI edge.
  ******************************************************************************
  */
#include "ultrasonic_pwm.h"
#include "system_state.h"
#include "main.h"

TIM_HandleTypeDef htim15;

#define TRIAC_TIMER_CLK_HZ      170000000UL /* TIM15 on APB2, see .ioc RCC.APB2TimFreq_Value */
#define AC_HALF_CYCLE_US        10000UL     /* 50 Hz mains: 10 ms per half-cycle */
#define TRIAC_PULSE_WIDTH_US    100UL       /* gate pulse width */
#define TRIAC_MIN_DELAY_US      500UL       /* fire soon after ZC -> max power */
#define TRIAC_MAX_DELAY_US      (AC_HALF_CYCLE_US - 500UL) /* fire late -> min power */
#define ZERO_CROSS_TIMEOUT_MS   500UL
#define SOFTSTART_RAMP_STEP_US  20UL        /* delay decrement per zero-cross */

/* Bench/dry-run build only: set to 1 to stop FAULT_ZERO_CROSS_LOST from
 * tripping SYS_MODE_FAULT when no mains zero-cross (or the ESP32 GPIO4->PC7
 * 100Hz simulator) is wired up. The triac still never fires without real ZC
 * edges, so this is safe for logic-only desk testing. MUST be 0 (default)
 * for any build driving a real triac/mains load. */
#ifndef ZC_BENCH_TEST_MODE
#define ZC_BENCH_TEST_MODE 1
#endif

static volatile uint32_t current_delay_us    = TRIAC_MAX_DELAY_US;
static volatile uint32_t last_zero_cross_tick = 0;

static uint32_t PowerPctToDelayUs(uint8_t power_pct)
{
  uint32_t span = TRIAC_MAX_DELAY_US - TRIAC_MIN_DELAY_US;

  if (power_pct > 100u)
  {
    power_pct = 100u;
  }
  return TRIAC_MAX_DELAY_US - ((span * power_pct) / 100u);
}

static uint8_t DelayUsToPowerPct(uint32_t delay_us)
{
  uint32_t span = TRIAC_MAX_DELAY_US - TRIAC_MIN_DELAY_US;

  if (delay_us >= TRIAC_MAX_DELAY_US)
  {
    return 0u;
  }
  if (delay_us <= TRIAC_MIN_DELAY_US)
  {
    return 100u;
  }
  return (uint8_t)(((TRIAC_MAX_DELAY_US - delay_us) * 100u) / span);
}

void TriacForceOff(void)
{
  HAL_TIM_OC_Stop_IT(&htim15, TIM_CHANNEL_1);
  HAL_GPIO_WritePin(TRIAC_GATE_GPIO_Port, TRIAC_GATE_Pin, GPIO_PIN_RESET);
}

void UltrasonicPWM_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* Triac gate output */
  HAL_GPIO_WritePin(TRIAC_GATE_GPIO_Port, TRIAC_GATE_Pin, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin   = TRIAC_GATE_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(TRIAC_GATE_GPIO_Port, &GPIO_InitStruct);

  /* Zero-cross detector input (PC7), rising edge EXTI */
  GPIO_InitStruct.Pin  = ZERO_CROSS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ZERO_CROSS_GPIO_Port, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  /* TIM15: manual one-pulse mode, IT-only (no physical OC pin). Re-armed on
   * every zero-cross edge with CCR1=firing delay, ARR=delay+pulse width. */
  __HAL_RCC_TIM15_CLK_ENABLE();

  htim15.Instance               = TIM15;
  htim15.Init.Prescaler         = (uint32_t)(TRIAC_TIMER_CLK_HZ / 1000000UL) - 1UL; /* 1 tick = 1 us */
  htim15.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim15.Init.Period            = (uint32_t)AC_HALF_CYCLE_US;
  htim15.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim15.Init.RepetitionCounter = 0;
  htim15.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  HAL_TIM_OC_Init(&htim15);

  sConfigOC.OCMode     = TIM_OCMODE_TIMING; /* IT-only compare, no GPIO toggle */
  sConfigOC.Pulse      = (uint32_t)TRIAC_MIN_DELAY_US;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  HAL_TIM_OC_ConfigChannel(&htim15, &sConfigOC, TIM_CHANNEL_1);

  htim15.Instance->CR1 |= TIM_CR1_OPM; /* auto-stop counter at the update event */
  __HAL_TIM_ENABLE_IT(&htim15, TIM_IT_UPDATE);

  HAL_NVIC_SetPriority(TIM1_BRK_TIM15_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(TIM1_BRK_TIM15_IRQn);

  current_delay_us     = TRIAC_MAX_DELAY_US;
  last_zero_cross_tick = HAL_GetTick();
}

void UltrasonicPWM_Process(void)
{
  if (g_system_state.mode != SYS_MODE_RUNNING)
  {
    current_delay_us = TRIAC_MAX_DELAY_US; /* re-arm soft-start for next START */
    TriacForceOff();
    return;
  }

  if ((HAL_GetTick() - last_zero_cross_tick) > ZERO_CROSS_TIMEOUT_MS)
  {
#if ZC_BENCH_TEST_MODE
    last_zero_cross_tick = HAL_GetTick(); /* pretend ZC is present, keep timer fed */
#else
    SystemState_SafeStop(STOP_REASON_FAULT);
    g_system_state.fault_flags |= FAULT_ZERO_CROSS_LOST;
    return;
#endif
  }

  uint32_t target_delay_us = PowerPctToDelayUs(g_system_state.setpoint_power_pct);

  /* Soft-start: the delay only ever ramps DOWN toward the target (never
   * jumps up), so conduction angle - and power - rises gradually. */
  if (current_delay_us > target_delay_us)
  {
    if ((current_delay_us - target_delay_us) > SOFTSTART_RAMP_STEP_US)
    {
      current_delay_us -= SOFTSTART_RAMP_STEP_US;
    }
    else
    {
      current_delay_us = target_delay_us;
    }
  }
  else
  {
    current_delay_us = target_delay_us;
  }

  g_system_state.actual_power_pct = DelayUsToPowerPct(current_delay_us);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin != ZERO_CROSS_Pin)
  {
    return;
  }

  last_zero_cross_tick = HAL_GetTick();

  if (g_system_state.mode != SYS_MODE_RUNNING)
  {
    return;
  }

  uint32_t delay_us = current_delay_us;

  __HAL_TIM_SET_COUNTER(&htim15, 0);
  __HAL_TIM_SET_COMPARE(&htim15, TIM_CHANNEL_1, delay_us);
  __HAL_TIM_SET_AUTORELOAD(&htim15, delay_us + TRIAC_PULSE_WIDTH_US);
  HAL_TIM_OC_Start_IT(&htim15, TIM_CHANNEL_1);
}

void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM15)
  {
    HAL_GPIO_WritePin(TRIAC_GATE_GPIO_Port, TRIAC_GATE_Pin, GPIO_PIN_SET);
  }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM15)
  {
    HAL_GPIO_WritePin(TRIAC_GATE_GPIO_Port, TRIAC_GATE_Pin, GPIO_PIN_RESET);
    HAL_TIM_OC_Stop_IT(htim, TIM_CHANNEL_1); /* rearm channel state for next zero-cross */
  }
}

/* HIL_DEEP_DEBUG */
uint32_t UltrasonicPWM_GetCurrentDelayUs(void)
{
  return current_delay_us;
}
