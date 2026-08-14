/**
  ******************************************************************************
  * @file    x9c103s.c
  * @brief   Driver source for X9C103S Digital Potentiometer (Dual Frequency
  *          Selection: 28 kHz / 40 kHz) on STM32G4.
  ******************************************************************************
  */
#include "x9c103s.h"

static uint8_t s_current_step = 0U;
static uint8_t s_current_freq = 28U;

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

uint8_t X9C103S_GetCurrentStep(void)
{
  return s_current_step;
}

uint8_t X9C103S_GetCurrentFrequency(void)
{
  return s_current_freq;
}
