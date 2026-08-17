/**
  ******************************************************************************
  * @file    heater_relay.c
  * @brief   Bang-bang heater relay control (HEATER_RELAY_Pin) with hysteresis.
  ******************************************************************************
  */
#include "heater_relay.h"
#include "system_state.h"
#include "main.h"

static uint32_t s_last_switch_tick = 0U;

static void RelaySet(uint8_t on)
{
  uint8_t current_state = (g_system_state.relay_state != 0U) ? 1U : 0U;
  uint8_t target_state  = on ? 1U : 0U;

  if (current_state != target_state)
  {
    HAL_GPIO_WritePin(HEATER_RELAY_GPIO_Port, HEATER_RELAY_Pin, target_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
    g_system_state.relay_state = target_state;
    s_last_switch_tick = HAL_GetTick();
  }
  else
  {
    HAL_GPIO_WritePin(HEATER_RELAY_GPIO_Port, HEATER_RELAY_Pin, target_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
    g_system_state.relay_state = target_state;
  }
}

void HeaterRelay_Init(void)
{
  HAL_GPIO_WritePin(HEATER_RELAY_GPIO_Port, HEATER_RELAY_Pin, GPIO_PIN_RESET);
  g_system_state.relay_state = 0U;
  s_last_switch_tick = HAL_GetTick() - HEATER_MIN_OFF_TIME_MS;
}

void HeaterRelay_ForceOff(void)
{
  RelaySet(0);
}

void HeaterRelay_Process(void)
{
  static SystemMode_t prev_mode = SYS_MODE_IDLE;
  SystemMode_t mode = g_system_state.mode;

  uint8_t is_running        = (mode == SYS_MODE_RUNNING) ? 1U : 0U;
  uint8_t is_degas_heating  = (mode == SYS_MODE_DEGAS && g_system_state.degas_config.temp_ctrl == 1U) ? 1U : 0U;
  uint8_t active_heating    = (is_running != 0U || is_degas_heating != 0U) ? 1U : 0U;

  uint8_t was_running       = (prev_mode == SYS_MODE_RUNNING) ? 1U : 0U;
  uint8_t was_degas_heating = (prev_mode == SYS_MODE_DEGAS && g_system_state.degas_config.temp_ctrl == 1U) ? 1U : 0U;
  uint8_t was_active        = (was_running != 0U || was_degas_heating != 0U) ? 1U : 0U;

  if (active_heating != 0U && was_active == 0U)
  {
    /* On transition into active heating mode, initialize switch tick so heater can engage immediately */
    s_last_switch_tick = HAL_GetTick() - HEATER_MIN_OFF_TIME_MS;
  }
  prev_mode = mode;

  if (active_heating == 0U)
  {
    HeaterRelay_ForceOff();
    return;
  }

  float temp_c     = g_system_state.current_temp_c;
  float setpoint_c = (is_running != 0U) ? g_system_state.setpoint_temp_c : g_system_state.degas_config.target_temp_c;
  uint32_t now     = HAL_GetTick();
  uint32_t elapsed = now - s_last_switch_tick;

  if (temp_c <= (setpoint_c - HEATER_HYSTERESIS_C))
  {
    if ((g_system_state.relay_state == 0U) && (elapsed >= HEATER_MIN_OFF_TIME_MS))
    {
      RelaySet(1);
    }
  }
  else if (temp_c >= (setpoint_c + HEATER_HYSTERESIS_C))
  {
    if ((g_system_state.relay_state != 0U) && (elapsed >= HEATER_MIN_ON_TIME_MS))
    {
      RelaySet(0);
    }
  }
  /* else: inside deadband, hold the previous relay state */
}
