/**
  ******************************************************************************
  * @file    heater_relay.c
  * @brief   Bang-bang heater relay control (HEATER_RELAY_Pin) with hysteresis,
  *          guard timers, trend awareness, and output abstraction.
  ******************************************************************************
  */
#include "heater_relay.h"
#include "system_state.h"
#include "pt100_adc.h"
#include "main.h"

/* State variables */
static uint32_t s_last_switch_tick = 0u;
static float s_prev_setpoint_c = 0.0f;
static HeaterReason_t s_current_reason = HEATER_REASON_IDLE;

void HeaterRelay_SetOutput(uint8_t on)
{
  uint8_t current_state = (g_system_state.relay_state != 0u) ? 1u : 0u;
  uint8_t target_state  = on ? 1u : 0u;

  HAL_GPIO_WritePin(HEATER_RELAY_GPIO_Port, HEATER_RELAY_Pin, target_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
  g_system_state.relay_state = target_state;

  if (current_state != target_state)
  {
    s_last_switch_tick = HAL_GetTick();
  }
}

void HeaterRelay_Init(void)
{
  HeaterRelay_SetOutput(0u);
  s_last_switch_tick = HAL_GetTick() - HEATER_MIN_OFF_TIME_MS;
  s_prev_setpoint_c = 0.0f;
  s_current_reason = HEATER_REASON_IDLE;
}

void HeaterRelay_ForceOff(void)
{
  HeaterRelay_SetOutput(0u);
  s_current_reason = HEATER_REASON_FORCED_OFF;
}

void HeaterRelay_Process(void)
{
  static SystemMode_t prev_mode = SYS_MODE_IDLE;
  SystemMode_t mode = g_system_state.mode;

  uint8_t is_running        = (mode == SYS_MODE_RUNNING) ? 1u : 0u;
  uint8_t is_degas_heating  = (mode == SYS_MODE_DEGAS && g_system_state.degas_config.temp_ctrl == 1u) ? 1u : 0u;
  uint8_t active_heating    = (is_running != 0u || is_degas_heating != 0u) ? 1u : 0u;

  uint8_t was_running       = (prev_mode == SYS_MODE_RUNNING) ? 1u : 0u;
  uint8_t was_degas_heating = (prev_mode == SYS_MODE_DEGAS && g_system_state.degas_config.temp_ctrl == 1u) ? 1u : 0u;
  uint8_t was_active        = (was_running != 0u || was_degas_heating != 0u) ? 1u : 0u;

  /* 1. Startup transition into active heating bypasses initial off-guard time */
  if (active_heating != 0u && was_active == 0u)
  {
    s_last_switch_tick = HAL_GetTick() - HEATER_MIN_OFF_TIME_MS;
  }
  prev_mode = mode;

  /* 2. Mode Interlock: In IDLE, FAULT, or DEGAS (temp_ctrl=0), enforce OFF */
  if (active_heating == 0u)
  {
    HeaterRelay_ForceOff();
    s_current_reason = (mode == SYS_MODE_FAULT) ? HEATER_REASON_FAULT : HEATER_REASON_IDLE;
    return;
  }

  /* 3. Temperature Sensor Health & Validity Validation */
  if ((g_system_state.fault_flags & (FAULT_PT100_OPEN | FAULT_PT100_SHORT | FAULT_SENSOR_FAULT)) != 0u ||
      PT100_IsTempValid() == 0u)
  {
    HeaterRelay_ForceOff();
    s_current_reason = HEATER_REASON_FAULT;
    return;
  }

  /* 4. Active Target Temperature Determination & Dynamic Setpoint Tracking */
  float setpoint_c = (is_running != 0u) ? g_system_state.setpoint_temp_c : g_system_state.degas_config.target_temp_c;
  if (setpoint_c != s_prev_setpoint_c)
  {
    s_prev_setpoint_c = setpoint_c;
  }

  if (setpoint_c <= 0.0f)
  {
    HeaterRelay_ForceOff();
    s_current_reason = HEATER_REASON_DISABLED;
    return;
  }

  /* 5. Process Temperature and Trend Values */
  float temp_c     = PT100_GetFilteredTemp();
  float trend_c_s  = PT100_GetTempRate();
  uint32_t now     = HAL_GetTick();
  uint32_t elapsed = now - s_last_switch_tick;

  /* 6. Hard Maximum Safety Temperature Interlock */
  if (temp_c >= HEATER_MAX_SAFE_TEMP_C)
  {
    HeaterRelay_ForceOff();
    s_current_reason = HEATER_REASON_TEMP_ABOVE_HYST;
    return;
  }

  /* 7. Hysteresis Decision Layer with Guard Timers & Trend Compensation */
  if (temp_c <= (setpoint_c - HEATER_HYSTERESIS_C))
  {
    /* Temperature is below lower threshold: demands HEATER ON */
    if (g_system_state.relay_state == 0u)
    {
      /* Check trend compensation: if temperature is already rising rapidly near threshold, suppress */
      if (temp_c >= (setpoint_c - (HEATER_HYSTERESIS_C * 1.2f)) && (trend_c_s >= HEATER_TREND_SUPPRESS_SLOPE))
      {
        s_current_reason = HEATER_REASON_TREND_SUPPRESS;
      }
      else if (elapsed >= HEATER_MIN_OFF_TIME_MS)
      {
        HeaterRelay_SetOutput(1u);
        s_current_reason = HEATER_REASON_TEMP_BELOW_HYST;
      }
      else
      {
        s_current_reason = HEATER_REASON_MIN_OFF_WAIT;
      }
    }
    else
    {
      s_current_reason = HEATER_REASON_TEMP_BELOW_HYST;
    }
  }
  else if (temp_c >= (setpoint_c + HEATER_HYSTERESIS_C))
  {
    /* Temperature is above upper threshold: demands HEATER OFF */
    if (g_system_state.relay_state != 0u)
    {
      if (elapsed >= HEATER_MIN_ON_TIME_MS)
      {
        HeaterRelay_SetOutput(0u);
        s_current_reason = HEATER_REASON_TEMP_ABOVE_HYST;
      }
      else
      {
        s_current_reason = HEATER_REASON_MIN_ON_WAIT;
      }
    }
    else
    {
      s_current_reason = HEATER_REASON_TEMP_ABOVE_HYST;
    }
  }
  else
  {
    /* Inside deadband (setpoint - 1.0C ... setpoint + 1.0C): maintain current relay state */
    s_current_reason = HEATER_REASON_DEADBAND_HOLD;
  }
}

HeaterReason_t HeaterRelay_GetReason(void)
{
  return s_current_reason;
}

uint32_t HeaterRelay_GetMinOnRemainMs(void)
{
  if (g_system_state.relay_state == 0u)
  {
    return 0u;
  }
  uint32_t elapsed = HAL_GetTick() - s_last_switch_tick;
  return (elapsed < HEATER_MIN_ON_TIME_MS) ? (HEATER_MIN_ON_TIME_MS - elapsed) : 0u;
}

uint32_t HeaterRelay_GetMinOffRemainMs(void)
{
  if (g_system_state.relay_state != 0u)
  {
    return 0u;
  }
  uint32_t elapsed = HAL_GetTick() - s_last_switch_tick;
  return (elapsed < HEATER_MIN_OFF_TIME_MS) ? (HEATER_MIN_OFF_TIME_MS - elapsed) : 0u;
}

