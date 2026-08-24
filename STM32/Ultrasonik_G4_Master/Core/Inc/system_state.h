/**
  ******************************************************************************
  * @file    system_state.h
  * @brief   Shared process/control data contract between esp32_uart.c,
  *          ultrasonic_pwm.c, heater_relay.c, pt100_adc.c and process_timer.c.
  ******************************************************************************
  */
#ifndef __SYSTEM_STATE_H
#define __SYSTEM_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
  SYS_MODE_IDLE = 0,
  SYS_MODE_RUNNING,
  SYS_MODE_FAULT,
  SYS_MODE_DEGAS
} SystemMode_t;

typedef enum
{
  PROV_STATE_UNCOMMISSIONED = 0x00,
  PROV_STATE_STAGING        = 0x01,
  PROV_STATE_ACTIVE         = 0x02
} ProvState_t;

typedef enum
{
  STOP_REASON_USER_STOP = 0,
  STOP_REASON_TIMER_ZERO,
  STOP_REASON_FAULT,
  STOP_REASON_COMM_TIMEOUT,
  STOP_REASON_WATCHDOG_RESET,
  STOP_REASON_SENSOR_FAULT
} StopReason_t;

/* fault_flags bitmask */
#define FAULT_NONE               0x00u
#define FAULT_PT100_OPEN         0x01u /* ADC raw at/near full-scale, or outside the realistic RTD window (disconnected/floating) */
#define FAULT_PT100_SHORT        0x02u /* ADC raw stuck at/near 0 */
#define FAULT_ZERO_CROSS_LOST    0x04u /* no zero-cross EXTI for > 500 ms */
#define FAULT_COMM_TIMEOUT       0x08u /* RX silence > 3000 ms */
#define FAULT_WATCHDOG_RESET     0x10u /* Hardware IWDG reset occurred */
#define FAULT_SENSOR_FAULT       0x20u /* General sensor failure */
#define FAULT_GENERAL            0x40u /* General system fault */

typedef struct
{
  uint16_t duration_minutes; /* Process duration in minutes (prototype default: 15 min) */
  uint8_t  power_pct;        /* Ultrasonic power percentage in DEGAS (prototype default: 100 %) */
  uint8_t  frequency_khz;    /* Base center frequency in DEGAS (prototype default: 28 kHz) */
  uint16_t pulse_on_ms;      /* Ultrasonic ON pulse duration in ms (prototype default: 1000 ms) */
  uint16_t pulse_off_ms;     /* Ultrasonic OFF silent duration in ms (prototype default: 500 ms) */
  uint8_t  temp_ctrl;        /* 0 = OFF (heater forced OFF in DEGAS), 1 = ON */
  float    target_temp_c;    /* Target temperature for DEGAS when temp_ctrl == 1 (prototype default: 50.0 °C) */
} DegasConfig_t;

typedef enum
{
  HEATER_MODE_RELAY = 0,
  HEATER_MODE_SSR   = 1
} HeaterMode_t;

typedef struct
{
  /* Setpoints - written by esp32_uart.c on command reception */
  uint16_t setpoint_time_minutes;
  float    setpoint_temp_c;
  uint8_t  setpoint_power_pct;

  /* DEGAS coherent runtime configuration snapshot */
  DegasConfig_t degas_config;

  /* Live process values - written by control modules, read for telemetry */
  float    current_temp_c;
  uint16_t remaining_seconds;
  uint8_t  relay_state;
  uint8_t  actual_power_pct;

  SystemMode_t mode;
  uint8_t      fault_flags;
  uint8_t      frequency_khz;
  uint32_t     softstart_delay_us;

  /* Dual-mode heater controller configuration */
  HeaterMode_t heater_mode;

  /* Phase 5.2 Commissioning & Hardware Identification */
  ProvState_t  prov_state;
  char         uid24[25]; /* 24-char upper-case hex string + null terminator */
} SystemState_t;


/* All members are single-word aligned (<=32-bit), so plain volatile reads/writes
 * through this global are atomic on Cortex-M4 without extra locking. */
extern volatile SystemState_t g_system_state;

uint32_t HAL_GetUIDWord0(void);
uint32_t HAL_GetUIDWord1(void);
uint32_t HAL_GetUIDWord2(void);
uint16_t CRC16_CCITT(const uint8_t *pData, uint16_t length);
void SystemState_GetUID24(char *out_str25);
uint8_t SystemState_VerifyUID24(const char *payload_uid24);

void SystemState_Init(void);
void SystemState_SafeStop(StopReason_t reason);

#ifdef __cplusplus
}
#endif

#endif /* __SYSTEM_STATE_H */
