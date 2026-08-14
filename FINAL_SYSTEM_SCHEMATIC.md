# EAGLEULTRASONİK — FINAL SYSTEM ELECTRICAL SCHEMATIC & ARCHITECTURE

**Document Version:** 3.0.0  
**Phase:** Phase 6.1 / Phase 5.2 — Final Hardware Wiring Master Authority  
**Date:** 2026-08-11  

---

## 1. ASCII System Architecture Schematic

```text
                                        DEVELOPMENT PC
                              ┌──────────────┼──────────────┐
                              │ USB #1       │ USB #2       │ USB #3
                              ▼              ▼              ▼
                           NUCLEO          ESP32         NEXTION
                           (5V/3V3)      (5V VBUS)     (Harici 5V)
                              │              │              │
                           MAX485 #1     MAX485 #2       Display
                              │              │              │
                              └──────────────┴──────────────┘
                                  COMMON LOGIC GND BUS

+---------------------------------------------------------------------------------------------------+
|                                      NUCLEO-G474RE NODE                                           |
|                                                                                                   |
|   PB10 (CN10-25 / USART3_TX)  ---------------------------------> DI  [ MAX485 #1 ]             |
|   PB11 (CN10-18 / USART3_RX)  <--------------------------------- RO  (5V FT Direct Connection)  |
|   PB1  (CN10-24 / RS485_DE)   ---------------------------------> DE  & /RE (Tied Together)     |
|   Nucleo 5V Rail              ---------------------------------> VCC (5.0V Supply)             |
|   Common GND Bus              ---------------------------------> GND                            |
|                                                                                                   |
|   PA0  (CN7-28 / ADC1_IN1)    <--- [ 1kΩ ] --------------------- VW  [ X9C103S Digital Pot ]     |
|   PB12 (CN10-16 / X9C_CS)     ---------------------------------> CS                             |
|   PB13 (CN10-30 / X9C_UD)     ---------------------------------> U/D                            |
|   PB14 (CN10-28 / X9C_INC)    ---------------------------------> INC                            |
|   Nucleo 3.3V Rail            ---------------------------------> VH  (MANDATORY 3.3V!)          |
|   Common GND Bus              ---------------------------------> VL  & VSS                      |
|   Nucleo 5V Rail              ---------------------------------> VCC (5.0V Supply)             |
|                                                                                                   |
|   PC7  (CN5-2 / EXTI7 ZC)     <--- [ 1kΩ ] ---------------------+ [ ESP32-S3 Node ]            |
|   PC8..PC11 (DIP 1..4)        <--- Active-Low switches --------+ [ DIP Switch Block ]           |
|   PB15 (Heater Output)        ---------------------------------> SSR Actuator                   |
|   PC6  (Triac Gate Pulse)     ---------------------------------> MOC3021 Optocoupler            |
+---------------------------------------------------------------------------------------------------+
                                                                   | | |
                                                                   | | |
===================================== DIFFERENTIAL RS485 BUS ===== | | | ===========================
                                                                   | | |
                                                        Line A (+) = | |
                                                        Line B (-) === |
                                                        Common GND ====+
                                                                   | | |
                                                                   | | |
+---------------------------------------------------------------------------------------------------+
|                                        ESP32-S3 NODE                                              |
|                                                                                                   |
|   GPIO8  (UART1_TX)           ---------------------------------> DI  [ MAX485 #2 ]             |
|   GPIO18 (UART1_RX)           <--- [ 10kΩ ] ---+---------------- RO  (5V Output)               |
|                                                |                                                  |
|                                             [ 18kΩ ] (Voltage Divider: 5V -> 3.21V Max)           |
|                                                |                                                  |
|                                               GND                                                 |
|   GPIO5  (RS485_DE_PIN)       ---------------------------------> DE  & /RE (Tied Together)     |
|   GPIO17 (TXD2 / HMI TX)      ---------------------------------> Nextion HMI RX Wire (Yellow)   |
|   GPIO16 (RXD2 / HMI RX)      <--------------------------------- Nextion HMI TX Wire (Blue)     |
|   GPIO4  (ZC_SIM_PIN)         -------------------> [ 1kΩ ] ----> STM32 PC7 (CN5-2/CN10-19)       |
|   ESP32 5V Rail               ---------------------------------> VCC (5.0V Supply)             |
|   Common GND Bus              ---------------------------------> GND                            |
+---------------------------------------------------------------------------------------------------+
```

---

## 2. Power Architecture & Ground Reference

- **Nucleo 5V Rail:** Isolated supply from USB #1. Powers Nucleo MCU, X9C103S VCC (Pin 8), MAX485 #1 VCC (Pin 8).
- **ESP32 5V Rail:** Isolated supply from USB #2. Powers ESP32-S3 node, MAX485 #2 VCC (Pin 8).
- **Nextion 5V Rail:** Isolated supply from USB #3 or external adapter. Powers Nextion HMI display.
- **Common Logic Signal GND Bus:** Central shared 0V reference connecting Nucleo GND, ESP32 GND, Nextion GND, MAX485 #1 GND, MAX485 #2 GND, X9C VSS/VL GND, DIP Switch GND.
