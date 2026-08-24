/**
  ******************************************************************************
  * @file    heater_control.c
  * @brief   Dual-Mode Heater Controller: Mechanical Relay (Bang-Bang) + DC SSR (PID / Time-Proportional).
  ******************************************************************************
  */
#include "heater_control.h"
#include "heater_relay.h"
#include "pt100_adc.h"
#include "system_state.h"
#include "main.h"

/* Internal SSR Controller State */
static HeaterPID_t s_pid;
static uint32_t s_ssr_window_start_tick = 0u;
static uint32_t s_last_pid_calc_tick = 0u;
static uint8_t s_ssr_physical_pin_state = 0u;

void HeaterControl_SetPhysicalOutput(uint8_t on)
{
  uint8_t current_state = (g_system_state.relay_state != 0u) ? 1u : 0u;
  uint8_t target_state  = on ? 1u : 0u;

  HAL_GPIO_WritePin(HEATER_RELAY_GPIO_Port, HEATER_RELAY_Pin, target_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
  g_system_state.relay_state = target_state;
  s_ssr_physical_pin_state   = target_state;
}

void HeaterControl_Init(void)
{
  /* Physical pin forced OFF on boot */
  HeaterControl_SetPhysicalOutput(0u);

  /* Reset Relay sub-engine */
  HeaterRelay_Init();

  /* Initialize SSR PID structure with robust defaults */
  s_pid.kp          = HEATER_PID_DEFAULT_KP;
  s_pid.ki          = HEATER_PID_DEFAULT_KI;
  s_pid.kd          = HEATER_PID_DEFAULT_KD;
  s_pid.integral    = 0.0f;
  s_pid.prev_error  = 0.0f;
  s_pid.p_term      = 0.0f;
  s_pid.i_term      = 0.0f;
  s_pid.d_term      = 0.0f;
  s_pid.output_pct  = 0.0f;

  s_ssr_window_start_tick = HAL_GetTick();
  s_last_pid_calc_tick    = HAL_GetTick();
  s_ssr_physical_pin_state = 0u;
}

void HeaterControl_ForceOff(void)
{
  /* 1. Immediate hardware pin cutoff */
  HeaterControl_SetPhysicalOutput(0u);

  /* 2. Force Relay controller sub-engine OFF */
  HeaterRelay_ForceOff();

  /* 3. Reset SSR PID integrator and terms (Anti-windup & Bumpless reset) */
  s_pid.integral   = 0.0f;
  s_pid.p_term     = 0.0f;
  s_pid.i_term     = 0.0f;
  s_pid.d_term     = 0.0f;
  s_pid.output_pct = 0.0f;
  s_pid.prev_error = 0.0f;
  s_ssr_physical_pin_state = 0u;
}

void HeaterControl_SetMode(HeaterMode_t mode)
{
  if (g_system_state.heater_mode != mode)
  {
    /* Bumpless transfer: cleanly force off previous controller before switching */
    HeaterControl_ForceOff();
    g_system_state.heater_mode = mode;
  }
}

HeaterMode_t HeaterControl_GetMode(void)
{
  return g_system_state.heater_mode;
}

float HeaterControl_GetDutyPct(void)
{
  if (g_system_state.heater_mode == HEATER_MODE_RELAY)
  {
    return (g_system_state.relay_state != 0u) ? 100.0f : 0.0f;
  }
  return s_pid.output_pct;
}

void HeaterControl_GetPIDTerms(float *p, float *i, float *d, float *err, float *duty)
{
  if (p != NULL)    *p    = s_pid.p_term;
  if (i != NULL)    *i    = s_pid.i_term;
  if (d != NULL)    *d    = s_pid.d_term;
  if (err != NULL)  *err  = s_pid.prev_error;
  if (duty != NULL) *duty = s_pid.output_pct;
}

static void SSR_Process(float setpoint_c, float temp_c, uint32_t now)
{
  /* 1. Periodic PID Loop Execution (10 Hz = 100 ms) */
  uint32_t dt_ms = now - s_last_pid_calc_tick;
  if (dt_ms >= 100u)
  {
    float dt_s = (float)dt_ms / 1000.0f;
    s_last_pid_calc_tick = now;

    float error = setpoint_c - temp_c;
    s_pid.prev_error = error;

    /* Proportional Term */
    s_pid.p_term = s_pid.kp * error;

    /* Integral Term with Anti-Windup (Conditional Clamping) */
    float next_integral = s_pid.integral + (s_pid.ki * error * dt_s);
    if (next_integral < HEATER_PID_I_MIN)
    {
      next_integral = HEATER_PID_I_MIN;
    }
    else if (next_integral > HEATER_PID_I_MAX)
    {
      next_integral = HEATER_PID_I_MAX;
    }

    /* Only accumulate integral if output is not already saturated against error */
    if ((s_pid.output_pct < 100.0f || error < 0.0f) && (s_pid.output_pct > 0.0f || error > 0.0f))
    {
      s_pid.integral = next_integral;
    }
    s_pid.i_term = s_pid.integral;

    /* Filtered Derivative Term (D = -Kd * dT/dt to avoid setpoint kick) */
    float temp_rate_c_per_s = PT100_GetTempRate();
    s_pid.d_term = -s_pid.kd * temp_rate_c_per_s;

    /* Total PID Output Calculation */
    float raw_output = s_pid.p_term + s_pid.i_term + s_pid.d_term;

    /* Output Saturation [0.0% ... 100.0%] */
    if (raw_output < 0.0f)
    {
      s_pid.output_pct = 0.0f;
    }
    else if (raw_output > 100.0f)
    {
      s_pid.output_pct = 100.0f;
    }
    else
    {
      s_pid.output_pct = raw_output;
    }
  }

  /* 2. Time-Proportional PWM Output Generation (SSR_WINDOW_MS = 2000 ms) */
  uint32_t window_elapsed = now - s_ssr_window_start_tick;
  if (window_elapsed >= SSR_WINDOW_MS)
  {
    s_ssr_window_start_tick = now;
    window_elapsed = 0u;
  }

  uint32_t on_time_ms = (uint32_t)((s_pid.output_pct / 100.0f) * (float)SSR_WINDOW_MS);

  /* Minimum ON / OFF Pulse Protection to prevent unnecessary switching jitter */
  if (on_time_ms < SSR_MIN_ON_MS)
  {
    on_time_ms = 0u;
  }
  else if ((SSR_WINDOW_MS - on_time_ms) < SSR_MIN_OFF_MS)
  {
    on_time_ms = SSR_WINDOW_MS;
  }

  uint8_t target_state = (window_elapsed < on_time_ms) ? 1u : 0u;
  HeaterControl_SetPhysicalOutput(target_state);
}

void HeaterControl_Process(void)
{
  SystemMode_t mode = g_system_state.mode;

  uint8_t is_running        = (mode == SYS_MODE_RUNNING) ? 1u : 0u;
  uint8_t is_degas_heating  = (mode == SYS_MODE_DEGAS && g_system_state.degas_config.temp_ctrl == 1u) ? 1u : 0u;
  uint8_t active_heating    = (is_running != 0u || is_degas_heating != 0u) ? 1u : 0u;

  /* 1. Safety Interlocks: IDLE, FAULT, or DEGAS (temp_ctrl=0) enforce immediate OFF */
  if (active_heating == 0u)
  {
    HeaterControl_ForceOff();
    return;
  }

  /* 2. Sensor Integrity Validation */
  if ((g_system_state.fault_flags & (FAULT_PT100_OPEN | FAULT_PT100_SHORT | FAULT_SENSOR_FAULT)) != 0u ||
      PT100_IsTempValid() == 0u)
  {
    HeaterControl_ForceOff();
    return;
  }

  /* 3. Active Target Temperature Determination */
  float setpoint_c = (is_running != 0u) ? g_system_state.setpoint_temp_c : g_system_state.degas_config.target_temp_c;
  if (setpoint_c <= 0.0f)
  {
    HeaterControl_ForceOff();
    return;
  }

  /* 4. Temperature and Hard Thermal Safety Check */
  float temp_c = PT100_GetFilteredTemp();
  if (temp_c >= HEATER_MAX_SAFE_TEMP_C)
  {
    HeaterControl_ForceOff();
    return;
  }

  /* 5. Dispatch to Selected Controller Engine */
  uint32_t now = HAL_GetTick();

  if (g_system_state.heater_mode == HEATER_MODE_RELAY)
  {
    /* Mechanical Relay Bang-Bang Engine */
    HeaterRelay_Process();
  }
  else
  {
    /* Solid State Relay PID + Time-Proportional Engine */
    SSR_Process(setpoint_c, temp_c, now);
  }
}
