# PHASE 6.2 — MCU PIN AND CONNECTOR REFERENCE
**Project:** EAGLEULTRASONİK — Industrial Ultrasonic Cleaner Controller  
**Document:** ST NUCLEO-G474RE & ESP32-S3 Pin & Header Cross-Reference  
**Status:** FULLY VERIFIED AGAINST ST UM2505 & STM32G474 DATASHEET  

---

## 1. NUCLEO-G474RE MCU PIN TO BOARD CONNECTOR MATRIX

| MCU GPIO | MCU Silicon Pin (LQFP64) | Arduino Header Equivalent | ST Morpho Header | Connector Number | Exact Connector Pin | Signal Function in Firmware | Authority Source / Evidence |
| :-: | :-: | :--- | :--- | :-: | :-: | :--- | :--- |
| **PA0** | Pin 14 | Arduino A0 | Morpho CN7 Pin 28 | CN7 | CN7-28 | ADC1_IN1 (X9C Wiper Voltage Readback) | main.c:L514, ST UM2505 Table 19 |
| **PA1** | Pin 15 | Arduino A1 | Morpho CN7 Pin 30 / CN10 Pin 11 | CN7 / CN10 | CN7-30 / CN10-11 | OPAMP3_INP (PT100 Temperature Sensor) | hal_msp.c:L161, ST UM2505 Table 19 |
| **PA2** | Pin 16 | ST-Link VCP TX | Morpho CN7 Pin 35 | CN7 | CN7-35 | LPUART1_TX (COM11 Debug Telemetry Stream) | main.c:L825, ST UM2505 Table 19 |
| **PA3** | Pin 17 | ST-Link VCP RX | Morpho CN7 Pin 37 | CN7 | CN7-37 | LPUART1_RX (COM11 Debug Command Stream) | main.c:L825, ST UM2505 Table 19 |
| **PA4** | Pin 20 | Arduino A2 | Morpho CN7 Pin 32 / CN10 Pin 17 | CN7 / CN10 | CN7-32 / CN10-17 | HEATER_TEST_FB_Pin — Firmware Diagnostic Feedback Loopback Input from PB15 | main.h:L102, main.c:L830, ST UM2505 Table 19 |
| **PA5** | Pin 21 | Arduino D13 | Morpho CN10 Pin 11 | CN10 | CN10-11 | GPIO Output (LD2 On-Board Green LED) | main.h:L84, ST UM2505 Table 19 |
| **PA6** | Pin 22 | Arduino D12 | Morpho CN7 Pin 34 / CN10 Pin 13 | CN7 / CN10 | CN7-34 / CN10-13 | TRIAC_TEST_FB_Pin — Firmware Diagnostic Feedback Loopback Input from PC6 | main.h:L104, main.c:L830, ST UM2505 Table 19 |
| **PB1** | Pin 27 | Arduino A6 | Morpho CN7 Pin 24 / CN10 Pin 24 | CN7 / CN10 | CN7-24 / CN10-24 | GPIO Output (RS485 #1 DE/RE Enable) | main.h:L102, main.c:L812, esp32_uart.c |
| **PB10** | Pin 29 | Arduino D6 | Morpho CN7 Pin 25 / CN10 Pin 25 | CN7 / CN10 | CN7-25 / CN10-25 | USART3_TX (RS485 #1 DI Input Drive) | hal_msp.c:L301, ST UM2505 Table 19 |
| **PB11** | Pin 30 | ST Morpho | Morpho CN10 Pin 18 | CN10 | CN10-18 | USART3_RX (RS485 #1 RO Output, FT_f 5V Pin)| hal_msp.c:L304, ST DS12288 Table 13 |
| **PB12** | Pin 33 | ST Morpho | Morpho CN10 Pin 16 | CN10 | CN10-16 | GPIO Output (X9C CS Chip Select) | main.h:L86, main.c:L846 |
| **PB13** | Pin 34 | ST Morpho | Morpho CN10 Pin 30 | CN10 | CN10-30 | GPIO Output (X9C U/D Direction) | main.h:L88, main.c:L847 |
| **PB14** | Pin 35 | ST Morpho | Morpho CN10 Pin 28 | CN10 | CN10-28 | GPIO Output (X9C INC Step Pulse) | main.h:L90, main.c:L847 |
| **PB15** | Pin 36 | ST Morpho | Morpho CN10 Pin 26 | CN10 | CN10-26 | GPIO Output (HEATER_RELAY Control) | main.h:L92, main.c:L840 |
| **PC6** | Pin 37 | Arduino D3 | Morpho CN10 Pin 4 | CN10 | CN10-4 | GPIO Output (TRIAC_GATE Firing Pulse) | main.h:L105, main.c:L853 |
| **PC7** | Pin 38 | Arduino D9 (CN5-2)| Morpho CN10 Pin 19 | CN5 / CN10 | CN5-2 / CN10-19 | EXTI7 Input (Zero-Cross 100Hz, EXTI9_5_IRQHandler) | main.h:L107, stm32g4xx_it.c:L233, ST UM2505 Table 19 |
| **PC8** | Pin 39 | ST Morpho | Morpho CN10 Pin 2 | CN10 | CN10-2 | GPIO Input w/ Pull-up (DIP_SW1 Bit 0) | main.h:L111, main.c:L860 |
| **PC9** | Pin 40 | ST Morpho | Morpho CN10 Pin 1 | CN10 | CN10-1 | GPIO Input w/ Pull-up (DIP_SW2 Bit 1) | main.h:L113, main.c:L860 |
| **PC10** | Pin 51 | ST Morpho | Morpho CN7 Pin 1 | CN7 | CN7-1 | GPIO Input w/ Pull-up (DIP_SW3 Bit 2) | main.h:L115, main.c:L860 |
| **PC11** | Pin 52 | ST Morpho | Morpho CN7 Pin 2 | CN7 | CN7-2 | GPIO Input w/ Pull-up (DIP_SW4 Bit 3) | main.h:L117, main.c:L860 |

---

## 2. CRITICAL CONNECTOR CORRECTIONS & SPECIAL NOTES

> [!IMPORTANT]
> **PC7 DUAL HEADER MAPPING:**  
> PC7 is physical Pin 38 on the LQFP64 MCU die. Nucleo-G474RE routes this single pin to BOTH **CN5 Pin 2 (Arduino D9)** AND **CN10 Pin 19 (Morpho)**. They are NOT two separate GPIOs. Connecting to either header pin accesses the exact same EXTI7 interrupt line.

> [!WARNING]
> **DIP SWITCH PIN CORRECTION:**  
> Previous legacy reports incorrectly claimed DIP switches were on PB4, PB5, PB6. In the authoritative firmware source (`main.h` lines 111-118 and `main.c` line 98), DIP switches are mapped strictly to **PC8, PC9, PC10, PC11**.
