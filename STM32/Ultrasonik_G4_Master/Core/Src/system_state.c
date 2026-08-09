/**
  ******************************************************************************
  * @file    system_state.c
  * @brief   Definition and default initialization of the shared g_system_state.
  ******************************************************************************
  */
#include "system_state.h"

volatile SystemState_t g_system_state;

void SystemState_Init(void)
{
  g_system_state.setpoint_time_minutes = 0;
  g_system_state.setpoint_temp_c       = 0.0f;
  g_system_state.setpoint_power_pct    = 0;

  g_system_state.current_temp_c    = 0.0f;
  g_system_state.remaining_seconds = 0;
  g_system_state.relay_state       = 0;
  g_system_state.actual_power_pct  = 0;

  g_system_state.mode        = SYS_MODE_IDLE;
  g_system_state.fault_flags = FAULT_NONE;
}
