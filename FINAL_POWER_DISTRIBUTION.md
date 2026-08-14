# EAGLEULTRASONİK — FINAL POWER DISTRIBUTION SPECIFICATION

**Document Version:** 1.0.0  
**Date:** 2026-08-11  

---

## 1. Power Architecture & Rail Isolation

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
```

> [!CAUTION]
> **ISOLATED 5V POWER RAILS — DO NOT CONNECT 5V RAILS TOGETHER**  
> Nucleo 5V (USB #1), ESP32 5V (USB #2), and Nextion 5V (USB #3) must remain strictly isolated.  
> Connecting active 5V regulators in parallel creates reverse current loop backfeeding ($4.95\text{V} \dots 5.15\text{V}$ voltage offsets) which burns USB transceivers and PC ports.

> [!IMPORTANT]
> **COMMON LOGIC SIGNAL GROUND BUS — MANDATORY CONNECTION**  
> All ground reference points (Nucleo GND, ESP32 GND, Nextion GND, MAX485 #1 GND, MAX485 #2 GND, X9C VSS/VL GND) **MUST be tied together** on a central Common Logic GND Bus.

---

## 2. Power Distribution Table

| Power Rail | Nominal Voltage | Max Current | Source Device | Target Devices Powered | Rail Isolation Status |
| :--- | :-: | :-: | :--- | :--- | :--- |
| **USB #1 Power** | 5.0V DC | 500 mA | PC USB Port #1 | Nucleo-G474RE Board | Independent Supply |
| **Nucleo 5V Rail**| 5.0V DC | 250 mA | Nucleo Regulator | X9C103S VCC (Pin 8), MAX485 #1 VCC (Pin 8) | **Isolated (Do NOT tie to ESP32 5V)** |
| **Nucleo 3.3V Rail**| 3.3V DC | 150 mA | Nucleo 3.3V Reg | STM32 MCU, X9C103S VH (Pin 3 - **Mandatory!**) | Internal MCU Rail |
| **USB #2 Power** | 5.0V DC | 500 mA | PC USB Port #2 | ESP32-S3 Node | Independent Supply |
| **ESP32 5V Rail** | 5.0V DC | 200 mA | ESP32 5V Pin | MAX485 #2 VCC (Pin 8) | **Isolated (Do NOT tie to Nucleo 5V)** |
| **USB #3 Power** | 5.0V DC | 1000 mA | PC USB Port #3 | Nextion HMI Display | Independent Supply |
| **Common GND Bus**| 0.0V DC | Return | Central Bus | ALL GND Pins (Nucleo, ESP32, Nextion, MAX485s, X9C) | **COMMON BUS (Tied Together)** |
