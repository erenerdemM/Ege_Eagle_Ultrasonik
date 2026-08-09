/**
  ******************************************************************************
  * @file    heater_relay.c
  * @brief   Bang-bang heater relay control (HEATER_RELAY_Pin) with hysteresis.
  ******************************************************************************
  */
#include "heater_relay.h"
#include "system_state.h"
#include "main.h"

static void RelaySet(uint8_t on)
{
  HAL_GPIO_WritePin(HEATER_RELAY_GPIO_Port, HEATER_RELAY_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
  g_system_state.relay_state = on ? 1u : 0u;
}

void HeaterRelay_Init(void)
{
  RelaySet(0);
}

void HeaterRelay_Process(void)
{
  if (g_system_state.mode != SYS_MODE_RUNNING)
  {
    RelaySet(0); /* also covers SYS_MODE_FAULT: cut relay per fault policy */
    return;
  }

  float temp_c     = g_system_state.current_temp_c;
  float setpoint_c = g_system_state.setpoint_temp_c;

  if (temp_c <= (setpoint_c - HEATER_HYSTERESIS_C))
  {
    RelaySet(1);
  }
  else if (temp_c >= (setpoint_c + HEATER_HYSTERESIS_C))
  {
    RelaySet(0);
  }
  /* else: inside deadband, hold the previous relay state */
}
