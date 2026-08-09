/**
  ******************************************************************************
  * @file    esp32_uart.h
  * @brief   ASCII line protocol over USART3 (huart3) linking the STM32 control
  *          firmware to the ESP32 / Nextion HMI over a shared multi-drop bus
  *          (up to 10 STM32 slave nodes).
  ******************************************************************************
  */
#ifndef __ESP32_UART_H
#define __ESP32_UART_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* This node's bus address (1-10). Every RX command must be prefixed "T<id>:"
 * to be accepted; every TX status telegram is stamped with this ID so the
 * ESP32 can tell multiple slaves apart on the shared bus. Defined in main.c;
 * set at boot from a Flash override or the DIP switch GPIOs (see TankId_Load /
 * ReadDipSwitchId in main.c), and updated live by the SET_ID command below. */
extern uint8_t MY_TANK_ID;

/**
  * @brief Starts interrupt-driven reception on huart3. Call once after
  *        MX_USART3_UART_Init() and SystemState_Init().
  */
void ESP32_UART_Init(void);

/**
  * @brief Parses and dispatches any fully received command line. Must be
  *        polled from the main superloop (non-blocking, no work if idle).
  */
void ESP32_UART_Process(void);

/**
  * @brief Formats and transmits the current g_system_state as a status
  *        telegram. Call periodically (e.g. every 250-500 ms) from main().
  */
void ESP32_UART_SendStatus(void);

#ifdef __cplusplus
}
#endif

#endif /* __ESP32_UART_H */
