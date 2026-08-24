/**
  ******************************************************************************
  * @file    system_state.c
  * @brief   Definition and default initialization of the shared g_system_state.
  ******************************************************************************
  */
#include "system_state.h"
#include "esp32_uart.h"
#include "ultrasonic_pwm.h"
#include "heater_relay.h"
#include "heater_control.h"
#include "x9c103s.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

volatile SystemState_t g_system_state;

uint32_t HAL_GetUIDWord0(void) { return *(volatile uint32_t *)(0x1FFF7590UL); }
uint32_t HAL_GetUIDWord1(void) { return *(volatile uint32_t *)(0x1FFF7594UL); }
uint32_t HAL_GetUIDWord2(void) { return *(volatile uint32_t *)(0x1FFF7598UL); }

uint16_t CRC16_CCITT(const uint8_t *pData, uint16_t length)
{
  uint16_t crc = 0xFFFFU;
  if (pData == NULL)
  {
    return 0U;
  }
  for (uint16_t i = 0U; i < length; i++)
  {
    crc ^= ((uint16_t)pData[i] << 8);
    for (uint8_t bit = 0U; bit < 8U; bit++)
    {
      if ((crc & 0x8000U) != 0U)
      {
        crc = (uint16_t)((crc << 1) ^ 0x1021U);
      }
      else
      {
        crc <<= 1;
      }
    }
  }
  return crc;
}

void SystemState_GetUID24(char *out_str25)
{
  if (out_str25 == NULL)
  {
    return;
  }

  uint32_t w0 = HAL_GetUIDWord0();
  uint32_t w1 = HAL_GetUIDWord1();
  uint32_t w2 = HAL_GetUIDWord2();

  snprintf(out_str25, 25, "%08X%08X%08X", (unsigned int)w0, (unsigned int)w1, (unsigned int)w2);
}

uint8_t SystemState_VerifyUID24(const char *payload_uid24)
{
  if (payload_uid24 == NULL || strlen(payload_uid24) < 24)
  {
    return 0U;
  }
  char hw_uid[25];
  SystemState_GetUID24(hw_uid);
  return (strncmp(payload_uid24, hw_uid, 24) == 0) ? 1U : 0U;
}

void SystemState_Init(void)
{
  g_system_state.setpoint_time_minutes = 0;
  g_system_state.setpoint_temp_c       = 0.0f;
  g_system_state.setpoint_power_pct    = 0;

  g_system_state.degas_config.duration_minutes = 15U;
  g_system_state.degas_config.power_pct        = 100U;
  g_system_state.degas_config.frequency_khz    = 28U;
  g_system_state.degas_config.pulse_on_ms      = 1000U;
  g_system_state.degas_config.pulse_off_ms     = 500U;
  g_system_state.degas_config.temp_ctrl        = 0U;
  g_system_state.degas_config.target_temp_c    = 50.0f;

  g_system_state.current_temp_c    = 0.0f;
  g_system_state.remaining_seconds = 0;
  g_system_state.relay_state       = 0;
  g_system_state.actual_power_pct  = 0;

  g_system_state.mode               = SYS_MODE_IDLE;
  g_system_state.fault_flags        = FAULT_NONE;
  g_system_state.frequency_khz      = 28U;
  g_system_state.softstart_delay_us = TRIAC_MAX_DELAY_US;

  g_system_state.prov_state         = PROV_STATE_UNCOMMISSIONED;
  g_system_state.heater_mode        = HEATER_MODE_RELAY;
  SystemState_GetUID24((char *)g_system_state.uid24);
}

void SystemState_SafeStop(StopReason_t reason)
{
  /* 1. Set mode & log reason code FIRST so any zero-cross EXTI interrupt
   * arriving concurrently immediately drops the edge instead of re-arming TIM15. */
  switch (reason)
  {
    case STOP_REASON_USER_STOP:
      /* Retain FAULT mode and fault flags if currently in fault state; otherwise transition to IDLE */
      if (g_system_state.mode != SYS_MODE_FAULT)
      {
        g_system_state.mode        = SYS_MODE_IDLE;
        g_system_state.fault_flags = FAULT_NONE;
      }
      g_system_state.remaining_seconds = 0u;
      break;

    case STOP_REASON_TIMER_ZERO:
      g_system_state.mode              = SYS_MODE_IDLE;
      g_system_state.remaining_seconds = 0u;
      break;

    case STOP_REASON_FAULT:
      g_system_state.mode         = SYS_MODE_FAULT;
      g_system_state.fault_flags |= FAULT_GENERAL;
      break;

    case STOP_REASON_COMM_TIMEOUT:
      g_system_state.mode         = SYS_MODE_FAULT;
      g_system_state.fault_flags |= FAULT_COMM_TIMEOUT;
      break;

    case STOP_REASON_WATCHDOG_RESET:
      g_system_state.mode         = SYS_MODE_FAULT;
      g_system_state.fault_flags |= FAULT_WATCHDOG_RESET;
      break;

    case STOP_REASON_SENSOR_FAULT:
      g_system_state.mode         = SYS_MODE_FAULT;
      g_system_state.fault_flags |= FAULT_SENSOR_FAULT;
      break;

    default:
      g_system_state.mode         = SYS_MODE_FAULT;
      g_system_state.fault_flags |= FAULT_GENERAL;
      break;
  }

  /* 2. Cuts Heater Output (RELAY/SSR) OFF immediately (bypassing guard timers & resetting PID) */
  HeaterControl_ForceOff();

  /* 3. Cuts Triac Gate OFF immediately */
  TriacForceOff();


  /* 4. Resets softstart ramp state */
  g_system_state.softstart_delay_us = TRIAC_MAX_DELAY_US;
  g_system_state.actual_power_pct   = 0u;

  /* 5. Disarms sweep and restores center frequency */
  X9C103S_SetSweepEnabled(0U);

  /* 6. Transmit immediate telemetry status update */
  ESP32_UART_SendStatus();
}
