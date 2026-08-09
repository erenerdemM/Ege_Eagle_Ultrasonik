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
  SYS_MODE_FAULT
} SystemMode_t;

/* fault_flags bitmask */
#define FAULT_NONE               0x00u
#define FAULT_PT100_OPEN         0x01u /* ADC raw at/near full-scale, or outside the realistic RTD window (disconnected/floating) */
#define FAULT_PT100_SHORT        0x02u /* ADC raw stuck at/near 0 */
#define FAULT_ZERO_CROSS_LOST    0x04u /* no zero-cross EXTI for > 500 ms */

typedef struct
{
  /* Setpoints - written by esp32_uart.c on command reception */
  uint16_t setpoint_time_minutes;
  float    setpoint_temp_c;
  uint8_t  setpoint_power_pct;

  /* Live process values - written by control modules, read for telemetry */
  float    current_temp_c;
  uint16_t remaining_seconds;
  uint8_t  relay_state;
  uint8_t  actual_power_pct;

  SystemMode_t mode;
  uint8_t      fault_flags;
} SystemState_t;

/* All members are single-word aligned (<=32-bit), so plain volatile reads/writes
 * through this global are atomic on Cortex-M4 without extra locking. */
extern volatile SystemState_t g_system_state;

void SystemState_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __SYSTEM_STATE_H */
