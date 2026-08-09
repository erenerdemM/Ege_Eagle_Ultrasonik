/**
  ******************************************************************************
  * @file    ultrasonic_pwm.h
  * @brief   Triac phase-angle power control for the ultrasonic driver stage.
  *          TIM15 one-pulse mode times the firing delay + 100us gate pulse,
  *          re-armed on every PC7 zero-cross EXTI edge.
  ******************************************************************************
  */
#ifndef __ULTRASONIC_PWM_H
#define __ULTRASONIC_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"

/* Exposed so stm32g4xx_it.c can route the TIM15 IRQ into the HAL handler */
extern TIM_HandleTypeDef htim15;

/**
  * @brief Configures the triac gate GPIO, the PC7 zero-cross EXTI input and
  *        TIM15 (one-pulse, IT-only). Call once after MX_GPIO_Init().
  */
void UltrasonicPWM_Init(void);

/**
  * @brief Ramps the firing delay toward the setpoint_power_pct target
  *        (soft-start only ramps the delay DOWN), monitors zero-cross
  *        presence (>500 ms silence => SYS_MODE_FAULT) and force-cuts the
  *        triac whenever not SYS_MODE_RUNNING. Poll from the main superloop.
  */
void UltrasonicPWM_Process(void);

/* HIL_DEEP_DEBUG: current triac firing delay in microseconds (500=max power, ~9500=min power) */
uint32_t UltrasonicPWM_GetCurrentDelayUs(void);

#ifdef __cplusplus
}
#endif

#endif /* __ULTRASONIC_PWM_H */
