/**
  ******************************************************************************
  * @file    heater_control.h
  * @brief   Dual-Mode Heater Controller: Mechanical Relay (Bang-Bang) + DC SSR (PID / Time-Proportional).
  ******************************************************************************
  */
#ifndef __HEATER_CONTROL_H
#define __HEATER_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "system_state.h"

/* SSR Time-Proportional PWM Configuration */
#define SSR_WINDOW_MS                (2000U)  /* 2.0 second time-proportional window */
#define SSR_MIN_ON_MS                (50U)    /* 50 ms minimum pulse width to protect DC SSR */
#define SSR_MIN_OFF_MS               (50U)    /* 50 ms minimum off period */

/* Default PID Constants for 12V DC bath heater */
#define HEATER_PID_DEFAULT_KP        (10.0f)  /* % duty per degC error */
#define HEATER_PID_DEFAULT_KI        (0.20f)  /* % duty per degC-sec */
#define HEATER_PID_DEFAULT_KD        (15.0f)  /* % duty per (degC/sec) */
#define HEATER_PID_I_MAX             (100.0f) /* Integral anti-windup clamp upper bound */
#define HEATER_PID_I_MIN             (0.0f)   /* Integral anti-windup clamp lower bound */

typedef struct
{
  float kp;
  float ki;
  float kd;
  float integral;
  float prev_error;
  float p_term;
  float i_term;
  float d_term;
  float output_pct;
} HeaterPID_t;

/**
  * @brief Initializes dual-mode heater controller, forcing all outputs and states OFF.
  */
void HeaterControl_Init(void);

/**
  * @brief Periodic process loop for heater control. Dispatches to RELAY or SSR engine.
  *        Poll from main superloop.
  */
void HeaterControl_Process(void);

/**
  * @brief Forces heater output OFF immediately and resets controller states (for SafeStop & faults).
  */
void HeaterControl_ForceOff(void);

/**
  * @brief Sets the active heater mode (RELAY or SSR).
  */
void HeaterControl_SetMode(HeaterMode_t mode);

/**
  * @brief Returns the active heater mode.
  */
HeaterMode_t HeaterControl_GetMode(void);

/**
  * @brief Returns the current SSR calculated duty cycle percentage (0..100%).
  */
float HeaterControl_GetDutyPct(void);

/**
  * @brief Returns current PID diagnostic terms.
  */
void HeaterControl_GetPIDTerms(float *p, float *i, float *d, float *err, float *duty);

/**
  * @brief Low-level atomic physical output driver for PB15 (HEATER_RELAY_Pin).
  */
void HeaterControl_SetPhysicalOutput(uint8_t on);

#ifdef __cplusplus
}
#endif

#endif /* __HEATER_CONTROL_H */
