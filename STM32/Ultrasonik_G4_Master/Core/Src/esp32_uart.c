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
  *   STAT,<TankID>,<mode>,<remaining_sec>,<temp_x10>,<relay>,<power_pct>,<frequency_khz>,<fault_flags>,<prov_state>\n
  *   temp_x10 is current_temp_c * 10 as an integer (avoids float printf).
  ******************************************************************************
  */
#include "esp32_uart.h"
#include "system_state.h"
#include "heater_control.h"
#include "x9c103s.h"
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
#define RX_SILENCE_TIMEOUT_MS 3000U

#define UART_FIFO_SIZE 256U
static uint8_t rx_byte;
static volatile uint8_t s_rx_fifo[UART_FIFO_SIZE];
static volatile uint16_t s_rx_fifo_head = 0U;
static volatile uint16_t s_rx_fifo_tail = 0U;
static char s_rx_line_buf[RX_LINE_MAX];
static uint16_t s_rx_line_len = 0U;

static char tx_line[TX_LINE_MAX];
static volatile uint8_t tx_busy = 0;

static uint32_t s_last_rx_tick_ms = 0U;
static uint8_t s_discover_pending = 0U;
static uint32_t s_discover_start_tick = 0U;
static uint32_t s_discover_delay_ms = 0U;

static BusDiagnostics_t g_bus_diag = {0};

const BusDiagnostics_t* ESP32_UART_GetDiagnostics(void)
{
  return &g_bus_diag;
}

static void ProcessLine(const char *line);

static void RS485_Transmit_Blocking(const uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
  if (pData == NULL || Size == 0U)
  {
    return;
  }

  /* Guard 1: Timeout-bounded check against stuck/stale background interrupt transmission */
  uint32_t wait_start = HAL_GetTick();
  while (tx_busy)
  {
    if ((HAL_GetTick() - wait_start) >= Timeout)
    {
      tx_busy = 0; /* Force clear lockup */
      g_bus_diag.tx_nack_count++;
      break;
    }
  }

  /* Claim TX lock */
  tx_busy = 1;

  RS485_TX_ENABLE();
  HAL_StatusTypeDef status = HAL_UART_Transmit(&huart3, (uint8_t *)pData, Size, Timeout);

  if (status == HAL_OK)
  {
    /* Guard 2: Timeout-bounded wait for UART Transmission Complete (TC) flag */
    uint32_t tc_start = HAL_GetTick();
    while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_TC) == RESET)
    {
      if ((HAL_GetTick() - tc_start) >= Timeout)
      {
        g_bus_diag.tx_nack_count++;
        break;
      }
    }
  }
  else
  {
    g_bus_diag.tx_nack_count++;
  }

  /* Always restore transceiver to RX mode and release TX lock */
  RS485_RX_ENABLE();
  tx_busy = 0;
}

void ESP32_UART_Init(void)
{
  RS485_RX_ENABLE();
  s_rx_fifo_head = 0U;
  s_rx_fifo_tail = 0U;
  s_rx_line_len = 0U;
  tx_busy = 0;
  s_discover_pending = 0;
  s_discover_start_tick = 0;
  s_discover_delay_ms = 0;
  s_last_rx_tick_ms = HAL_GetTick();
  HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
}

uint32_t ESP32_UART_GetLastRxTick(void)
{
  return s_last_rx_tick_ms;
}

void ESP32_UART_Process(void)
{
  if (g_system_state.mode == SYS_MODE_RUNNING || g_system_state.mode == SYS_MODE_DEGAS)
  {
    uint32_t now = HAL_GetTick();
    if ((now - s_last_rx_tick_ms) > RX_SILENCE_TIMEOUT_MS)
    {
      g_bus_diag.rx_timeout_count++;
      SystemState_SafeStop(STOP_REASON_COMM_TIMEOUT);
    }
  }

  /* Process pending discovery response timer */
  if (s_discover_pending != 0U && ((HAL_GetTick() - s_discover_start_tick) >= s_discover_delay_ms))
  {
    s_discover_pending = 0U;
    if (g_system_state.prov_state == PROV_STATE_UNCOMMISSIONED && MY_TANK_ID == 0U)
    {
      char hw_uid[25];
      SystemState_GetUID24(hw_uid);
      char ackbuf[80];
      int len = snprintf(ackbuf, sizeof(ackbuf), "DISCOVER_ACK,0,%s\n", hw_uid);
      RS485_Transmit_Blocking((const uint8_t *)ackbuf, (uint16_t)len, 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ackbuf, (uint16_t)len, 10);
    }
  }

  while (s_rx_fifo_head != s_rx_fifo_tail)
  {
    uint8_t c = s_rx_fifo[s_rx_fifo_tail];
    s_rx_fifo_tail = (uint16_t)((s_rx_fifo_tail + 1U) % UART_FIFO_SIZE);

    if (c == '\n' || c == '\r')
    {
      if (s_rx_line_len > 0U)
      {
        s_rx_line_buf[s_rx_line_len] = '\0';
        char process_buf[RX_LINE_MAX];
        memcpy(process_buf, s_rx_line_buf, s_rx_line_len + 1U);
        s_rx_line_len = 0U;
        ProcessLine(process_buf);
      }
    }
    else if (s_rx_line_len < (RX_LINE_MAX - 1U))
    {
      s_rx_line_buf[s_rx_line_len++] = (char)c;
    }
    else
    {
      s_rx_line_len = 0U;
      g_bus_diag.rx_dropped_count++;
    }
  }
}

static void ProcessLine(const char *line)
{
  char *endptr;

  /* Multi-drop bus addressing: every command is prefixed "T<tank_id>:".
   * Frames not addressed to this node (or with a malformed prefix) are
   * silently discarded so up to 10 STM32 slaves can share this bus. */
  if (line[0] != 'T')
  {
    g_bus_diag.rx_malformed_count++;
    return;
  }

  long tank_id = strtol(&line[1], &endptr, 10);
  if (endptr == &line[1] || *endptr != ':')
  {
    g_bus_diag.rx_malformed_count++;
    return; /* malformed address prefix */
  }

  /* T0 is a universal broadcast address, accepted regardless of MY_TANK_ID
   * (e.g. a fresh/unknown-ID board can still be assigned via T0:SET_ID:n or T0:ASSIGN_ID:n:UID). */
  if (tank_id != 0 && (uint8_t)tank_id != MY_TANK_ID)
  {
    return; /* addressed to a different tank on the bus */
  }

  /* Valid frame for this node: refresh RX watchdog tick */
  s_last_rx_tick_ms = HAL_GetTick();
  g_bus_diag.rx_valid_count++;

  const char *cmd = endptr + 1;

  /* Layer 2 Active Mode Interlock at STM32 Slave Level */
  if (g_system_state.mode == SYS_MODE_RUNNING || g_system_state.mode == SYS_MODE_DEGAS)
  {
    if (strncmp(cmd, "SET_TIME:", 9) == 0  ||
        strncmp(cmd, "SET_TEMP:", 9) == 0  ||
        strncmp(cmd, "SET_POWER:", 10) == 0 ||
        strncmp(cmd, "SET_FREQ:", 9) == 0)
    {
      const char *err_msg = "ERR:LOCKED_ACTIVE_MODE\n";
      RS485_Transmit_Blocking((const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      return;
    }
  }

  /* Requirement 2: Layer 2 SYS_MODE_RUNNING and SYS_MODE_DEGAS Interlock at STM32 Slave Level */
  if (g_system_state.mode == SYS_MODE_RUNNING || g_system_state.mode == SYS_MODE_DEGAS)
  {
    if (strncmp(cmd, "STAGE_ID", 8) == 0  ||
        strncmp(cmd, "ASSIGN_ID", 9) == 0 ||
        strncmp(cmd, "SET_ID:", 7) == 0   ||
        strncmp(cmd, "RESET_ID", 8) == 0  ||
        strncmp(cmd, "DISCOVER", 8) == 0  ||
        strncmp(cmd, "COMMIT_ID", 9) == 0)
    {
      const char *err_msg = "ERR:LOCKED_SYS_RUNNING\n";
      RS485_Transmit_Blocking((const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      return; /* REJECT IMMEDIATELY; NO FLASH TOUCH OR STATE MUTATION */
    }
  }

  if (strcmp(cmd, "SWEEP:ON") == 0)
  {
    if (g_system_state.mode == SYS_MODE_DEGAS)
    {
      const char *err_msg = "ERR:SWEEP_PROHIBITED_IN_DEGAS\n";
      RS485_Transmit_Blocking((const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      return;
    }
    if (g_system_state.mode == SYS_MODE_FAULT)
    {
      const char *err_msg = "ERR:INVALID_SYS_MODE\n";
      RS485_Transmit_Blocking((const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      return;
    }
    X9C103S_SetSweepEnabled(1U);
    {
      char ack_msg[64];
      int ack_len = snprintf(ack_msg, sizeof(ack_msg), "ACK:SWEEP:ON,PERIOD_MS=%u,SPAN=+-%uKHZ\n",
                             (unsigned int)X9C103S_GetSweepPeriod(),
                             (unsigned int)X9C103S_GetSweepSpan());
      RS485_Transmit_Blocking((const uint8_t *)ack_msg, (uint16_t)ack_len, 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ack_msg, (uint16_t)ack_len, 10);
    }
    return;
  }
  else if (strcmp(cmd, "SWEEP:OFF") == 0)
  {
    X9C103S_SetSweepEnabled(0U);
    {
      const char *ack_msg = "ACK:SWEEP:OFF,CENTER_RESTORED\n";
      RS485_Transmit_Blocking((const uint8_t *)ack_msg, (uint16_t)strlen(ack_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ack_msg, (uint16_t)strlen(ack_msg), 10);
    }
    return;
  }
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
  else if (strncmp(cmd, "SET_HEATER_MODE:", 16) == 0)
  {
    const char *val = &cmd[16];
    if (strcmp(val, "SSR") == 0 || strcmp(val, "1") == 0)
    {
      HeaterControl_SetMode(HEATER_MODE_SSR);
      const char *ack_msg = "ACK:HEATER_MODE=SSR\n";
      RS485_Transmit_Blocking((const uint8_t *)ack_msg, (uint16_t)strlen(ack_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ack_msg, (uint16_t)strlen(ack_msg), 10);
    }
    else if (strcmp(val, "RELAY") == 0 || strcmp(val, "0") == 0)
    {
      HeaterControl_SetMode(HEATER_MODE_RELAY);
      const char *ack_msg = "ACK:HEATER_MODE=RELAY\n";
      RS485_Transmit_Blocking((const uint8_t *)ack_msg, (uint16_t)strlen(ack_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ack_msg, (uint16_t)strlen(ack_msg), 10);
    }
    else
    {
      const char *err_msg = "NACK,ERR_INVALID_PARAM\n";
      RS485_Transmit_Blocking((const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
    }
  }
  else if (strcmp(cmd, "START") == 0)
  {

    if (g_system_state.mode == SYS_MODE_FAULT)
    {
      const char *err_msg = "NACK,ERR_FAULT_ACTIVE\n";
      RS485_Transmit_Blocking((const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
    }
    else
    {
      g_system_state.mode = SYS_MODE_RUNNING;
      if (X9C103S_IsSweepEnabled() != 0U)
      {
        /* If sweep was armed in IDLE, trigger endpoint positioning upon entering RUNNING */
        X9C103S_SetSweepEnabled(1U);
      }
    }
  }
  else if (strncmp(cmd, "START_DEGAS", 11) == 0 || strcmp(cmd, "DEGAS") == 0 || strcmp(cmd, "MODE:DEGAS") == 0)
  {
    if (g_system_state.mode != SYS_MODE_FAULT)
    {
      if (cmd[11] == ':')
      {
        /* Parameterized snapshot frame: START_DEGAS:<dur>:<pwr>:<freq>:<on>:<off>:<t_ctrl>:<t_target> */
        unsigned int dur = 0, pwr = 0, freq = 0, on = 0, off = 0, t_ctrl = 0;
        float t_target = 0.0f;
        int parsed = sscanf(cmd + 12, "%u:%u:%u:%u:%u:%u:%f", &dur, &pwr, &freq, &on, &off, &t_ctrl, &t_target);

        if (parsed == 7)
        {
          /* Software boundary validation: arbitrary 28..40 kHz support */
          if (dur >= 1u && dur <= 120u &&
              pwr >= 10u && pwr <= 100u &&
              freq >= 28u && freq <= 40u &&
              on >= 100u && on <= 10000u &&
              off <= 10000u &&
              (off == 0u || off >= 100u) &&
              t_ctrl <= 1u &&
              t_target >= 20.0f && t_target <= 90.0f)
          {
            g_system_state.degas_config.duration_minutes = (uint16_t)dur;
            g_system_state.degas_config.power_pct        = (uint8_t)pwr;
            g_system_state.degas_config.frequency_khz    = (uint8_t)freq;
            g_system_state.degas_config.pulse_on_ms      = (uint16_t)on;
            g_system_state.degas_config.pulse_off_ms     = (uint16_t)off;
            g_system_state.degas_config.temp_ctrl        = (uint8_t)t_ctrl;
            g_system_state.degas_config.target_temp_c    = t_target;

            X9C103S_SetSweepEnabled(0U);
            (void)X9C103S_SetFrequency((uint8_t)freq);
            g_system_state.mode = SYS_MODE_DEGAS;
          }
          /* Out-of-bounds parameters are safely rejected without mode change */
        }
      }
      else if (cmd[11] == '\0' || strcmp(cmd, "DEGAS") == 0 || strcmp(cmd, "MODE:DEGAS") == 0)
      {
        /* Parameterless command: use existing volatile degas_config defaults */
        X9C103S_SetSweepEnabled(0U);
        (void)X9C103S_SetFrequency(g_system_state.degas_config.frequency_khz);
        g_system_state.mode = SYS_MODE_DEGAS;
      }
    }
    else
    {
      const char *err_msg = "NACK,ERR_FAULT_ACTIVE\n";
      RS485_Transmit_Blocking((const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
    }
  }
  else if (strcmp(cmd, "STOP") == 0)
  {
    /* STOP acts as manual user stop, clears transient communication faults, and disarms outputs */
    if (g_system_state.fault_flags == FAULT_COMM_TIMEOUT)
    {
      g_system_state.fault_flags = FAULT_NONE;
    }
    SystemState_SafeStop(STOP_REASON_USER_STOP);
  }
  else if (strcmp(cmd, "CLEAR_FAULT") == 0 || strcmp(cmd, "FAULT_CLEAR") == 0)
  {
    if (g_system_state.mode == SYS_MODE_FAULT)
    {
      uint8_t persistent_hardware_fault = 0U;

      /* Evaluate if PT100 ADC is currently out of valid hardware bounds */
      if ((g_system_state.fault_flags & (FAULT_PT100_OPEN | FAULT_PT100_SHORT)) != 0U)
      {
        if (g_system_state.current_temp_c < -20.0f || g_system_state.current_temp_c > 150.0f)
        {
          persistent_hardware_fault = 1U;
        }
      }

      if (persistent_hardware_fault != 0U)
      {
        const char *err_msg = "NACK:FAULT_PERSISTENT\n";
        RS485_Transmit_Blocking((const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
        HAL_UART_Transmit(&hlpuart1, (const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      }
      else
      {
        g_system_state.fault_flags = FAULT_NONE;
        g_system_state.mode        = SYS_MODE_IDLE;
        const char *ack_msg = "ACK:FAULT_CLEARED\n";
        RS485_Transmit_Blocking((const uint8_t *)ack_msg, (uint16_t)strlen(ack_msg), 10);
        HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ack_msg, (uint16_t)strlen(ack_msg), 10);
        ESP32_UART_SendStatus();
      }
    }
    else
    {
      const char *ack_msg = "ACK:NO_FAULT\n";
      RS485_Transmit_Blocking((const uint8_t *)ack_msg, (uint16_t)strlen(ack_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ack_msg, (uint16_t)strlen(ack_msg), 10);
    }
  }
  else if (strcmp(cmd, "GET_UID") == 0)
  {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "UID24:%s,PROV:%u\n", (const char *)g_system_state.uid24, (unsigned int)g_system_state.prov_state);
    RS485_Transmit_Blocking((const uint8_t *)buf, (uint16_t)len, 10);
    HAL_UART_Transmit(&hlpuart1, (const uint8_t *)buf, (uint16_t)len, 10);
  }
  else if (strncmp(cmd, "STAGE_ID", 8) == 0)
  {
    /* Syntax: STAGE_ID:<UID24> or STAGE_ID */
    const char *payload_uid = (cmd[8] == ':') ? &cmd[9] : NULL;
    char hw_uid[25];
    SystemState_GetUID24(hw_uid);

    /* Requirement 3: Byte-for-byte 24-char hex UID comparison against 0x1FFF7590 register */
    if (payload_uid != NULL && !SystemState_VerifyUID24(payload_uid))
    {
      char errbuf[80];
      int len = snprintf(errbuf, sizeof(errbuf), "NACK,STAGE_ID,ERR_UID_MISMATCH,%s\n", hw_uid);
      RS485_Transmit_Blocking((const uint8_t *)errbuf, (uint16_t)len, 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)errbuf, (uint16_t)len, 10);
      return;
    }

    TankId_StartStaging();

    char ackbuf[80];
    int len = snprintf(ackbuf, sizeof(ackbuf), "ACK,STAGE_ID,%s\n", hw_uid);
    RS485_Transmit_Blocking((const uint8_t *)ackbuf, (uint16_t)len, 10);
    HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ackbuf, (uint16_t)len, 10);
  }
  else if (strncmp(cmd, "ASSIGN_ID:", 10) == 0)
  {
    /* Syntax: ASSIGN_ID:<new_id>:<UID24> */
    long new_id = strtol(&cmd[10], &endptr, 10);
    char hw_uid[25];
    SystemState_GetUID24(hw_uid);

    if (endptr != &cmd[10] && *endptr == ':' && new_id >= 1 && new_id <= 10)
    {
      const char *payload_uid = endptr + 1;

      /* Requirement 3: Byte-for-byte 24-char hex UID comparison against 0x1FFF7590 register */
      if (!SystemState_VerifyUID24(payload_uid))
      {
        char errbuf[80];
        int len = snprintf(errbuf, sizeof(errbuf), "NACK,ASSIGN_ID,ERR_UID_MISMATCH,%s\n", hw_uid);
        RS485_Transmit_Blocking((const uint8_t *)errbuf, (uint16_t)len, 10);
        HAL_UART_Transmit(&hlpuart1, (const uint8_t *)errbuf, (uint16_t)len, 10);
        return;
      }

      /* Requirement 4: Active state rejection - Nodes in PROV_STATE_ACTIVE reject direct ASSIGN_ID */
      if (g_system_state.prov_state == PROV_STATE_ACTIVE)
      {
        char errbuf[80];
        int len = snprintf(errbuf, sizeof(errbuf), "NACK,ASSIGN_ID,ERR_STATE_INVALID,%s\n", hw_uid);
        RS485_Transmit_Blocking((const uint8_t *)errbuf, (uint16_t)len, 10);
        HAL_UART_Transmit(&hlpuart1, (const uint8_t *)errbuf, (uint16_t)len, 10);
        return;
      }

      /* Execute assignment when in PROV_STATE_UNCOMMISSIONED or PROV_STATE_STAGING */
      if (TankId_SaveAndVerifyOverride((uint8_t)new_id, (uint8_t)PROV_STATE_ACTIVE))
      {
        TankId_ConfirmStaging((uint8_t)new_id);
        char ackbuf[80];
        int len = snprintf(ackbuf, sizeof(ackbuf), "ACK,ASSIGN_ID,%u,%s\n", (unsigned int)new_id, hw_uid);
        RS485_Transmit_Blocking((const uint8_t *)ackbuf, (uint16_t)len, 10);
        HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ackbuf, (uint16_t)len, 10);
      }
      else
      {
        char errbuf[80];
        int len = snprintf(errbuf, sizeof(errbuf), "NACK,ASSIGN_ID,ERR_FLASH_VERIFY_FAIL,%s\n", hw_uid);
        RS485_Transmit_Blocking((const uint8_t *)errbuf, (uint16_t)len, 10);
        HAL_UART_Transmit(&hlpuart1, (const uint8_t *)errbuf, (uint16_t)len, 10);
      }
    }
  }
  else if (strncmp(cmd, "RESET_ID", 8) == 0)
  {
    /* Syntax: RESET_ID:<UID24> or RESET_ID */
    const char *payload_uid = (cmd[8] == ':') ? &cmd[9] : NULL;
    char hw_uid[25];
    SystemState_GetUID24(hw_uid);

    /* Requirement 3: Byte-for-byte 24-char hex UID comparison against 0x1FFF7590 register */
    if (payload_uid != NULL && !SystemState_VerifyUID24(payload_uid))
    {
      char errbuf[80];
      int len = snprintf(errbuf, sizeof(errbuf), "NACK,RESET_ID,ERR_UID_MISMATCH,%s\n", hw_uid);
      RS485_Transmit_Blocking((const uint8_t *)errbuf, (uint16_t)len, 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)errbuf, (uint16_t)len, 10);
      return;
    }

    TankId_EraseOverride();

    char ackbuf[80];
    int len = snprintf(ackbuf, sizeof(ackbuf), "ACK,RESET_ID,%s\n", hw_uid);
    RS485_Transmit_Blocking((const uint8_t *)ackbuf, (uint16_t)len, 10);
    HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ackbuf, (uint16_t)len, 10);
  }
  else if (strncmp(cmd, "DISCOVER", 8) == 0)
  {
    /* Requirement 1: Only UNCOMMISSIONED nodes at MY_TANK_ID == 0 respond to DISCOVER.
     * STAGING nodes (PROV_STATE_STAGING) and ACTIVE nodes explicitly IGNORE discovery broadcasts. */
    if (g_system_state.prov_state == PROV_STATE_UNCOMMISSIONED && MY_TANK_ID == 0U)
    {
      uint32_t uid_words[3];
      uid_words[0] = HAL_GetUIDWord0();
      uid_words[1] = HAL_GetUIDWord1();
      uid_words[2] = HAL_GetUIDWord2();

      uint16_t crc = CRC16_CCITT((const uint8_t *)uid_words, 12U);
      uint8_t slot = (uint8_t)(crc % 16U);

      uint16_t rnd_seed = 0U;
      if (cmd[8] == ':')
      {
        rnd_seed = (uint16_t)strtoul(&cmd[9], NULL, 16);
      }
      uint16_t jitter_ms = (rnd_seed > 0U) ? ((crc ^ rnd_seed) % 15U) : 0U;
      uint32_t delay_ms = ((uint32_t)slot * 25U) + (uint32_t)jitter_ms;

      s_discover_pending = 1U;
      s_discover_start_tick = HAL_GetTick();
      s_discover_delay_ms = delay_ms;
    }
  }
  else if (strcmp(cmd, "CANCEL_STAGE") == 0)
  {
    TankId_CancelStaging();
    const char *msg = "LOG:STAGING_CANCELLED\n";
    RS485_Transmit_Blocking((const uint8_t *)msg, (uint16_t)strlen(msg), 10);
    HAL_UART_Transmit(&hlpuart1, (const uint8_t *)msg, (uint16_t)strlen(msg), 10);
  }
  else if (strncmp(cmd, "SET_FREQ:", 9) == 0)
  {
    long freq = strtol(&cmd[9], &endptr, 10);
    if (endptr != &cmd[9] && (freq == 28 || freq == 40))
    {
      if ((uint8_t)freq != g_system_state.frequency_khz)
      {
        /* SWP-GAP-001 / ADR-02 / SWP-REQ-009: Frequency change terminates any active sweep state */
        X9C103S_SetSweepEnabled(0U);

        g_system_state.frequency_khz = (uint8_t)freq;
        (void)X9C103S_SetFrequency((uint8_t)freq);

        const char *log_msg = (freq == 28) ? "LOG:FREQ_28KHZ_SET_STEP_40_4KOHM\n"
                                           : "LOG:FREQ_40KHZ_SET_STEP_90_9KOHM\n";
        HAL_UART_Transmit(&hlpuart1, (const uint8_t *)log_msg, (uint16_t)strlen(log_msg), 10);
      }
    }
    else
    {
      /* Invalid frequency: transmit ERR:INVALID_FREQ and retain current frequency */
      const char *err_msg = "ERR:INVALID_FREQ\n";
      RS485_Transmit_Blocking((const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
    }
  }
  else if (strncmp(cmd, "SET_STEP_INC:", 13) == 0)
  {
    if (g_system_state.mode == SYS_MODE_RUNNING)
    {
      const char *err_msg = "ERR:LOCKED_SYS_RUNNING\n";
      RS485_Transmit_Blocking((const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      return;
    }

    long val = strtol(&cmd[13], &endptr, 10);
    if (endptr != &cmd[13] && val >= 1 && val <= 8)
    {
      (void)X9C103S_SetStepIncrement((uint8_t)val);
      char ackbuf[40];
      int len = snprintf(ackbuf, sizeof(ackbuf), "ACK:STEP_INC:%u\n", (unsigned int)val);
      RS485_Transmit_Blocking((const uint8_t *)ackbuf, (uint16_t)len, 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ackbuf, (uint16_t)len, 10);
    }
    else
    {
      const char *err_msg = "ERR:INVALID_PARAM\n";
      RS485_Transmit_Blocking((const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
    }
  }
  else if (strncmp(cmd, "SET_SWP_SPAN:", 13) == 0 || strncmp(cmd, "SET_SPAN:", 9) == 0)
  {
    if (g_system_state.mode == SYS_MODE_RUNNING)
    {
      const char *err_msg = "ERR:LOCKED_SYS_RUNNING\n";
      RS485_Transmit_Blocking((const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      return;
    }

    const char *p = (strncmp(cmd, "SET_SWP_SPAN:", 13) == 0) ? &cmd[13] : &cmd[9];
    long val = strtol(p, &endptr, 10);
    if (endptr != p && val >= 1 && val <= 4)
    {
      (void)X9C103S_SetSweepSpan((uint8_t)val);
      char ackbuf[40];
      int len = snprintf(ackbuf, sizeof(ackbuf), "ACK:SWP_SPAN:%u\n", (unsigned int)val);
      RS485_Transmit_Blocking((const uint8_t *)ackbuf, (uint16_t)len, 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ackbuf, (uint16_t)len, 10);
    }
    else
    {
      const char *err_msg = "ERR:INVALID_PARAM\n";
      RS485_Transmit_Blocking((const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
    }
  }
  else if (strncmp(cmd, "SET_SWP_PER:", 12) == 0 || strncmp(cmd, "SET_PER:", 8) == 0)
  {
    if (g_system_state.mode == SYS_MODE_RUNNING)
    {
      const char *err_msg = "ERR:LOCKED_SYS_RUNNING\n";
      RS485_Transmit_Blocking((const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      return;
    }

    const char *p = (strncmp(cmd, "SET_SWP_PER:", 12) == 0) ? &cmd[12] : &cmd[8];
    long val = strtol(p, &endptr, 10);
    if (endptr != p && val >= 100 && val <= 1000)
    {
      (void)X9C103S_SetSweepPeriod((uint16_t)val);
      char ackbuf[40];
      int len = snprintf(ackbuf, sizeof(ackbuf), "ACK:SWP_PER:%u\n", (unsigned int)val);
      RS485_Transmit_Blocking((const uint8_t *)ackbuf, (uint16_t)len, 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ackbuf, (uint16_t)len, 10);
    }
    else
    {
      const char *err_msg = "ERR:INVALID_PARAM\n";
      RS485_Transmit_Blocking((const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
    }
  }
  else if (strncmp(cmd, "GET_DIAG", 8) == 0 || strncmp(cmd, "DIAG", 4) == 0)
  {
    if (tank_id == 0)
    {
      return; /* Narrow guard: T0:GET_DIAG MUST NOT generate a response to avoid bus collision */
    }

    char diagbuf[128];
    int len = snprintf(diagbuf, sizeof(diagbuf),
                       "DIAG,%u,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
                       (unsigned int)MY_TANK_ID,
                       (unsigned long)g_bus_diag.rx_valid_count,
                       (unsigned long)g_bus_diag.rx_crc_error_count,
                       (unsigned long)g_bus_diag.rx_malformed_count,
                       (unsigned long)g_bus_diag.rx_timeout_count,
                       (unsigned long)g_bus_diag.rx_dropped_count,
                       (unsigned long)g_bus_diag.tx_frame_count,
                       (unsigned long)g_bus_diag.tx_ack_count,
                       (unsigned long)g_bus_diag.tx_nack_count);
    RS485_Transmit_Blocking((const uint8_t *)diagbuf, (uint16_t)len, 10);
    HAL_UART_Transmit(&hlpuart1, (const uint8_t *)diagbuf, (uint16_t)len, 10);
    g_bus_diag.tx_ack_count++;
  }
  /* Unrecognized commands are silently ignored */
}

void ESP32_UART_SendStatus(void)
{
  if (MY_TANK_ID == 0U || g_system_state.prov_state != PROV_STATE_ACTIVE)
  {
    return; /* Telemetry suppressed for uncommissioned or staging nodes to prevent bus contention */
  }

  if (tx_busy)
  {
    return; /* previous telegram still in flight, skip this cycle */
  }

  const char *mode_str;
  switch (g_system_state.mode)
  {
    case SYS_MODE_RUNNING: mode_str = "RUNNING"; break;
    case SYS_MODE_DEGAS:   mode_str = "DEGAS";   break;
    case SYS_MODE_FAULT:   mode_str = "FAULT";   break;
    default:                mode_str = "IDLE";    break;
  }

  int temp_x10 = (int)(g_system_state.current_temp_c * 10.0f);

  uint8_t sweep_enabled = X9C103S_IsSweepEnabled();
  uint8_t sweep_active = (sweep_enabled != 0U && g_system_state.mode == SYS_MODE_RUNNING) ? 1U : 0U;
  uint8_t swp_st = (uint8_t)((sweep_enabled << 1U) | sweep_active);

  uint8_t current_freq = (g_system_state.mode == SYS_MODE_DEGAS) ? g_system_state.degas_config.frequency_khz : g_system_state.frequency_khz;

  int len = snprintf(tx_line, TX_LINE_MAX, "STAT,%u,%s,%u,%d,%u,%u,%u,%u,%u,%u\n",
                      (unsigned int)MY_TANK_ID,
                      mode_str,
                      (unsigned int)g_system_state.remaining_seconds,
                      temp_x10,
                      (unsigned int)g_system_state.relay_state,
                      (unsigned int)g_system_state.actual_power_pct,
                      (unsigned int)current_freq,
                      (unsigned int)g_system_state.fault_flags,
                      (unsigned int)g_system_state.prov_state,
                      (unsigned int)swp_st);

  if (len <= 0)
  {
    return;
  }

  if (len >= TX_LINE_MAX)
  {
    len = TX_LINE_MAX - 1;
    tx_line[len] = '\0';
  }

  tx_busy = 1;
  g_bus_diag.tx_frame_count++;
  RS485_TX_ENABLE();
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

  uint16_t next_head = (uint16_t)((s_rx_fifo_head + 1U) % UART_FIFO_SIZE);
  if (next_head != s_rx_fifo_tail)
  {
    s_rx_fifo[s_rx_fifo_head] = rx_byte;
    s_rx_fifo_head = next_head;
  }
  else
  {
    g_bus_diag.rx_dropped_count++;
  }

  HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART3)
  {
    return;
  }

  /* TC flag is already handled by HAL_UART_IRQHandler before calling callback */
  RS485_RX_ENABLE();
  tx_busy = 0;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART3)
  {
    return;
  }

  /* Clear all UART error flags (Overrun, Noise, Framing, Parity) */
  __HAL_UART_CLEAR_OREFLAG(huart);
  __HAL_UART_CLEAR_NEFLAG(huart);
  __HAL_UART_CLEAR_FEFLAG(huart);
  __HAL_UART_CLEAR_PEFLAG(huart);
  huart->ErrorCode = HAL_UART_ERROR_NONE;

  /* Overrun/framing/noise errors abort the pending HAL_UART_Receive_IT();
   * discard the partial line and immediately re-arm so a single glitch
   * never permanently silences the link (RX must never be left dead). */
  RS485_RX_ENABLE();
  s_rx_line_len = 0U;
  g_bus_diag.rx_dropped_count++;
  tx_busy = 0; /* Reset TX lockup state so status transmission recovers after error */

  if (HAL_UART_Receive_IT(&huart3, &rx_byte, 1) != HAL_OK)
  {
    /* If re-arm failed due to state lock, force abort receive and re-arm deterministically */
    (void)HAL_UART_AbortReceive(&huart3);
    (void)HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
  }
}
