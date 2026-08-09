/**
  ******************************************************************************
  * @file    process_timer.c
  * @brief   1 Hz process countdown timer, auto-stops the process at 0.
  ******************************************************************************
  */
#include "process_timer.h"
#include "system_state.h"
#include "main.h"

static uint32_t     last_tick_ms;
static SystemMode_t prev_mode = SYS_MODE_IDLE;

void ProcessTimer_Init(void)
{
  last_tick_ms = HAL_GetTick();
  prev_mode = g_system_state.mode;
}

void ProcessTimer_Process(void)
{
  SystemMode_t mode = g_system_state.mode;

  /* Reload the countdown on every ->RUNNING transition (START command) */
  if ((mode == SYS_MODE_RUNNING) && (prev_mode != SYS_MODE_RUNNING))
  {
    g_system_state.remaining_seconds = (uint16_t)(g_system_state.setpoint_time_minutes * 60u);
    last_tick_ms = HAL_GetTick();

    if (g_system_state.remaining_seconds == 0u)
    {
      /* 0-minute setpoint: nothing to run, auto-stop immediately */
      g_system_state.mode = SYS_MODE_IDLE;
      mode = SYS_MODE_IDLE;
    }
  }
  prev_mode = mode;

  if (mode != SYS_MODE_RUNNING)
  {
    return;
  }

  if ((HAL_GetTick() - last_tick_ms) >= 1000u)
  {
    last_tick_ms += 1000u;

    if (g_system_state.remaining_seconds > 0u)
    {
      g_system_state.remaining_seconds--;
    }

    if (g_system_state.remaining_seconds == 0u)
    {
      g_system_state.mode = SYS_MODE_IDLE; /* auto-stop */
    }
  }
}
