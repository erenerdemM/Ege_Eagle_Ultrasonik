/**
  ******************************************************************************
  * @file    heater_relay.h
  * @brief   Bang-bang heater relay control (HEATER_RELAY_Pin) with hysteresis.
  ******************************************************************************
  */
#ifndef __HEATER_RELAY_H
#define __HEATER_RELAY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Deadband, in degrees C, applied symmetrically around setpoint_temp_c */
#define HEATER_HYSTERESIS_C       (1.0f)

/* Guard timer constants (in milliseconds) to prevent rapid relay wear/chattering */
#define HEATER_MIN_ON_TIME_MS     (10000U)
#define HEATER_MIN_OFF_TIME_MS    (10000U)

/**
  * @brief  Initializes relay GPIO and state variable, forcing relay OFF.
  */
void HeaterRelay_Init(void);

/**
  * @brief  Applies +-1.0C hysteresis around setpoint_temp_c while
  *         SYS_MODE_RUNNING with Min ON / Min OFF guard timers. Calls
  *         HeaterRelay_ForceOff() if mode != SYS_MODE_RUNNING. Poll from main().
  */
void HeaterRelay_Process(void);

/**
  * @brief  Forces the relay OFF IMMEDIATELY, bypassing guard timers.
  */
void HeaterRelay_ForceOff(void);

#ifdef __cplusplus
}
#endif

#endif /* __HEATER_RELAY_H */
