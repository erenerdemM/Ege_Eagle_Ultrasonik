/**
  ******************************************************************************
  * @file    esp32_uart.c
  * @brief   ASCII line protocol over USART3 (huart3) linking the STM32 control
  *          firmware to the ESP32 / Nextion HMI over a shared multi-drop bus.
  *
  * Commands (ESP32 -> STM32), one per line, terminated by '\n' (optional '\r'),
  * addressed with this node's Tank ID (MY_TANK_ID) as a "T<id>:" prefix so up
  * to 10 STM32 slaves can share the same bus. Frames addressed to a different
  * ID (or missing/malformed the prefix) are silently discarded. "T0:" is a
  * universal broadcast address accepted by every slave regardless of its
  * current MY_TANK_ID (used e.g. to (re)assign an ID via SET_ID on a board
  * whose current ID is unknown to the ESP32):
  *   T<id>:SET_TIME:<minutes 0-100>
  *   T<id>:SET_TEMP:<degC 0-90>
  *   T<id>:SET_POWER:<percent 0-100>
  *   T<id>:START
  *   T<id>:STOP        (also acknowledges/clears an active FAULT)
  *   T0:SET_ID:<new_id 1-10>   (broadcast; also works as T<id>:SET_ID:<new_id>)
  *
  * Status telegram (STM32 -> ESP32), sent on ESP32_UART_SendStatus():
  *   STAT,<TankID>,<mode>,<remaining_sec>,<temp_x10>,<relay>,<power_pct>,<fault_flags>\n
  *   temp_x10 is current_temp_c * 10 as an integer (avoids float printf).
  ******************************************************************************
  */
#include "esp32_uart.h"
#include "system_state.h"
#include "main.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef hlpuart1;  // HIL_TEST_MOD: ST-Link VCP (COM11), telemetry echo for HIL test host

/* Setpoint validation limits, matched to legacy .mbas clamps */
#define TIME_MINUTES_MAX   100u
#define TEMP_SETPOINT_MIN  0.0f
#define TEMP_SETPOINT_MAX  90.0f
#define POWER_PCT_MAX      100u

#define RX_LINE_MAX  64u
#define TX_LINE_MAX  64u

static uint8_t rx_byte;
static char rx_line[RX_LINE_MAX];
static volatile uint16_t rx_index = 0;
static volatile uint8_t line_ready = 0;

static char tx_line[TX_LINE_MAX];
static volatile uint8_t tx_busy = 0;

static void ProcessLine(const char *line);

void ESP32_UART_Init(void)
{
  rx_index = 0;
  line_ready = 0;
  tx_busy = 0;
  HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
}

void ESP32_UART_Process(void)
{
  if (!line_ready)
  {
    return;
  }

  char line[RX_LINE_MAX];
  memcpy(line, rx_line, sizeof(line));

  rx_index = 0;
  line_ready = 0;

  ProcessLine(line);
}

static void ProcessLine(const char *line)
{
  char *endptr;

  /* Multi-drop bus addressing: every command is prefixed "T<tank_id>:".
   * Frames not addressed to this node (or with a malformed prefix) are
   * silently discarded so up to 10 STM32 slaves can share this bus. */
  if (line[0] != 'T')
  {
    return;
  }

  long tank_id = strtol(&line[1], &endptr, 10);
  if (endptr == &line[1] || *endptr != ':')
  {
    return; /* malformed address prefix */
  }

  /* T0 is a universal broadcast address, accepted regardless of MY_TANK_ID
   * (e.g. a fresh/unknown-ID board can still be assigned via T0:SET_ID:n). */
  if (tank_id != 0 && (uint8_t)tank_id != MY_TANK_ID)
  {
    return; /* addressed to a different tank on the bus */
  }

  const char *cmd = endptr + 1;

  if (strncmp(cmd, "SET_TIME:", 9) == 0)
  {
    long minutes = strtol(&cmd[9], &endptr, 10);
    if (endptr != &cmd[9])
    {
      if (minutes < 0)
      {
        minutes = 0;
      }
      else if (minutes > TIME_MINUTES_MAX)
      {
        minutes = TIME_MINUTES_MAX;
      }
      g_system_state.setpoint_time_minutes = (uint16_t)minutes;
    }
  }
  else if (strncmp(cmd, "SET_TEMP:", 9) == 0)
  {
    float temp_c = strtof(&cmd[9], &endptr);
    if (endptr != &cmd[9])
    {
      if (temp_c < TEMP_SETPOINT_MIN)
      {
        temp_c = TEMP_SETPOINT_MIN;
      }
      else if (temp_c > TEMP_SETPOINT_MAX)
      {
        temp_c = TEMP_SETPOINT_MAX;
      }
      g_system_state.setpoint_temp_c = temp_c;
    }
  }
  else if (strncmp(cmd, "SET_POWER:", 10) == 0)
  {
    long power = strtol(&cmd[10], &endptr, 10);
    if (endptr != &cmd[10])
    {
      if (power < 0)
      {
        power = 0;
      }
      else if (power > POWER_PCT_MAX)
      {
        power = POWER_PCT_MAX;
      }
      g_system_state.setpoint_power_pct = (uint8_t)power;
    }
  }
  else if (strcmp(cmd, "START") == 0)
  {
    if (g_system_state.mode != SYS_MODE_FAULT)
    {
      g_system_state.mode = SYS_MODE_RUNNING;
    }
  }
  else if (strcmp(cmd, "STOP") == 0)
  {
    /* STOP also acts as a manual fault acknowledge/reset */
    g_system_state.fault_flags = FAULT_NONE;
    g_system_state.mode = SYS_MODE_IDLE;
  }
  else if (strncmp(cmd, "SET_ID:", 7) == 0)
  {
    long new_id = strtol(&cmd[7], &endptr, 10);
    if (endptr != &cmd[7] && new_id >= 1 && new_id <= 10)
    {
      /* Persists to Flash and updates MY_TANK_ID immediately; this reply
       * telegram (and all further ones) will be stamped with the new ID. */
      TankId_SaveOverride((uint8_t)new_id);
    }
  }
  /* Unrecognized commands are silently ignored */
}

void ESP32_UART_SendStatus(void)
{
  if (tx_busy)
  {
    return; /* previous telegram still in flight, skip this cycle */
  }

  const char *mode_str;
  switch (g_system_state.mode)
  {
    case SYS_MODE_RUNNING: mode_str = "RUNNING"; break;
    case SYS_MODE_FAULT:   mode_str = "FAULT";   break;
    default:                mode_str = "IDLE";    break;
  }

  int temp_x10 = (int)(g_system_state.current_temp_c * 10.0f);

  int len = snprintf(tx_line, TX_LINE_MAX, "STAT,%u,%s,%u,%d,%u,%u,%u\n",
                      (unsigned int)MY_TANK_ID,
                      mode_str,
                      (unsigned int)g_system_state.remaining_seconds,
                      temp_x10,
                      (unsigned int)g_system_state.relay_state,
                      (unsigned int)g_system_state.actual_power_pct,
                      (unsigned int)g_system_state.fault_flags);

  if (len <= 0)
  {
    return;
  }

  tx_busy = 1;
  HAL_UART_Transmit_IT(&huart3, (uint8_t *)tx_line, (uint16_t)len);

  /* HIL_TEST_MOD: mirror the same STAT telegram onto the ST-Link VCP (COM11) so the
   * HIL test host can verify this slave's own telemetry/fault state without needing
   * physical access to the shared ESP32<->STM32 bus. */
  HAL_UART_Transmit(&hlpuart1, (uint8_t *)tx_line, (uint16_t)len, 10);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART3)
  {
    return;
  }

  if (!line_ready)
  {
    if (rx_byte == '\n')
    {
      /* strip a preceding '\r' if present */
      if (rx_index > 0 && rx_line[rx_index - 1] == '\r')
      {
        rx_index--;
      }
      rx_line[rx_index] = '\0';
      line_ready = 1;
    }
    else if (rx_index < (RX_LINE_MAX - 1))
    {
      rx_line[rx_index++] = (char)rx_byte;
    }
    else
    {
      /* line too long, discard it and resync on the next '\n' */
      rx_index = 0;
    }
  }
  /* if line_ready is still set (previous line not yet consumed), incoming
   * bytes are dropped until ESP32_UART_Process() catches up */

  HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART3)
  {
    return;
  }

  tx_busy = 0;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART3)
  {
    return;
  }

  /* Overrun/framing/noise errors abort the pending HAL_UART_Receive_IT();
   * discard the partial line and immediately re-arm so a single glitch
   * never permanently silences the link (RX must never be left dead). */
  rx_index = 0;
  HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
}
