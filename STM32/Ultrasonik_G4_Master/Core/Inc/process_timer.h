/**
  ******************************************************************************
  * @file    process_timer.h
  * @brief   1 Hz process countdown timer, auto-stops the process at 0.
  ******************************************************************************
  */
#ifndef __PROCESS_TIMER_H
#define __PROCESS_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief Resets the internal 1 Hz tick reference. Call once at startup.
  */
void ProcessTimer_Init(void);

/**
  * @brief Loads remaining_seconds from setpoint_time_minutes on every
  *        ->SYS_MODE_RUNNING transition, decrements it once per second while
  *        running, and forces SYS_MODE_IDLE (auto-stop) when it reaches 0.
  *        Poll from the main superloop.
  */
void ProcessTimer_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* __PROCESS_TIMER_H */
