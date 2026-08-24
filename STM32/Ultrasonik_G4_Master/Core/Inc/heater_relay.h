/**
  ******************************************************************************
  * @file    heater_relay.h
  * @brief   Bang-bang heater relay control (HEATER_RELAY_Pin) with hysteresis,
  *          guard timers, trend awareness, and output abstraction.
  ******************************************************************************
  */
#ifndef __HEATER_RELAY_H
#define __HEATER_RELAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Deadband, in degrees C, applied symmetrically around active target temperature */
#define HEATER_HYSTERESIS_C          (1.0f)

/* Guard timer constants (in milliseconds) to prevent rapid relay wear/chattering */
#define HEATER_MIN_ON_TIME_MS        (10000U)
#define HEATER_MIN_OFF_TIME_MS       (10000U)

/* Hard thermal safety limit */
#define HEATER_MAX_SAFE_TEMP_C       (95.0f)

/* Positive rate of temperature change (degC/s) threshold for trend suppression in deadband */
#define HEATER_TREND_SUPPRESS_SLOPE  (0.20f)

typedef enum
{
  HEATER_REASON_IDLE = 0,
  HEATER_REASON_DISABLED,
  HEATER_REASON_FAULT,
  HEATER_REASON_TEMP_BELOW_HYST,
  HEATER_REASON_TEMP_ABOVE_HYST,
  HEATER_REASON_DEADBAND_HOLD,
  HEATER_REASON_MIN_ON_WAIT,
  HEATER_REASON_MIN_OFF_WAIT,
  HEATER_REASON_TREND_SUPPRESS,
  HEATER_REASON_FORCED_OFF
} HeaterReason_t;

/**
  * @brief  Initializes relay GPIO and state variable, forcing relay OFF.
  */
void HeaterRelay_Init(void);

/**
  * @brief  Applies +-1.0C hysteresis around active target temperature with Min ON / Min OFF
  *         guard timers and trend compensation. Calls HeaterRelay_ForceOff() when heating is inactive.
  *         Poll from the main superloop.
  */
void HeaterRelay_Process(void);

/**
  * @brief  Forces the relay OFF IMMEDIATELY, bypassing guard timers (for SafeStop and faults).
  */
void HeaterRelay_ForceOff(void);

/**
  * @brief  Low-level atomic output driver helper for PB15 (HEATER_RELAY_Pin).
  */
void HeaterRelay_SetOutput(uint8_t on);

/* State, reason, and guard timer accessors */
HeaterReason_t HeaterRelay_GetReason(void);
uint32_t HeaterRelay_GetMinOnRemainMs(void);
uint32_t HeaterRelay_GetMinOffRemainMs(void);

#ifdef __cplusplus
}
#endif

#endif /* __HEATER_RELAY_H */

