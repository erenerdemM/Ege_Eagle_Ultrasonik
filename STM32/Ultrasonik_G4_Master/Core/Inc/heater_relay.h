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
#define HEATER_HYSTERESIS_C  (1.0f)

/**
  * @brief Forces the relay off. Call once after MX_GPIO_Init().
  */
void HeaterRelay_Init(void);

/**
  * @brief Applies +-1.0C hysteresis around setpoint_temp_c while
  *        SYS_MODE_RUNNING, else forces the relay off. Poll from main().
  */
void HeaterRelay_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* __HEATER_RELAY_H */
