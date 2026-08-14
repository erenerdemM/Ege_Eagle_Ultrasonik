# EAGLEULTRASONİK — PHASE 6.2 FINAL WIRING SUMMARY

**Document Version:** 1.0.0
**Status:** READY FOR PHYSICAL WIRING

This document is a concise, single-page physical assembly reference for the technician.

## 1. STM32 GPIO ➔ TARGET
| STM32 Pin | Signal | Target |
| :--- | :--- | :--- |
| **PA0** | ADC IN (X9C Wiper) | X9C103S Pin 5 (VW) |
| **PB10** | USART3 TX (RS485) | RS485 Module A DI |
| **PB11** | USART3 RX (RS485) | RS485 Module A RO |
| **PB1** | RS485 DE/RE | RS485 Module A DE/RE |
| **PB12** | X9C CS | X9C103S Pin 7 (CS) |
| **PB13** | X9C U/D | X9C103S Pin 2 (U/D) |
| **PB14** | X9C INC | X9C103S Pin 1 (INC) |
| **PC7** | Zero-Cross IN | ESP32 GPIO4 |

## 2. ESP32 GPIO ➔ TARGET
| ESP32 Pin | Signal | Target |
| :--- | :--- | :--- |
| **GPIO4** | ZC Sim Out | STM32 PC7 |
| **GPIO8** | UART1 TX (RS485) | RS485 Module B DI |
| **GPIO18** | UART1 RX (RS485) | RS485 Module B RO |
| **GPIO5** | RS485 DE/RE | RS485 Module B DE/RE |
| **GPIO17** | HMI TX | Nextion RX |
| **GPIO16** | HMI RX | Nextion TX |

## 3. X9C103S PINOUT
| X9C Pin | Name | Connection |
| :--- | :--- | :--- |
| **1** | INC | STM32 PB14 |
| **2** | U/D | STM32 PB13 |
| **3** | VH | 3.3V DC Rail |
| **4** | VSS | COMMON GND BUS |
| **5** | VW | STM32 PA0 |
| **6** | VL | COMMON GND BUS |
| **7** | CS | STM32 PB12 |
| **8** | VCC | 5.0V DC Rail |

## 4. RS485 BUS
- **Module A (STM32):** DI -> PB10, RO -> PB11, DE/RE -> PB1.
- **Module B (ESP32):** DI -> GPIO8, RO -> GPIO18, DE/RE -> GPIO5.
- **Bus:** Line A to Line A, Line B to Line B (Twisted Pair). Shield to Common GND.
- **Termination:** 120Ω resistor across A and B on both modules.

## 5. NEXTION HMI
- **RX:** ESP32 GPIO17
- **TX:** ESP32 GPIO16
- **Power:** Independent USB/5V supply. GND to Common GND Bus.

## 6. DIP SWITCH (PRODUCTION ONLY)
- **Pins:** PB4, PB5, PB6 are used for Hardware ID input via DIP Switches.
- Note: In bench mode, these pins are used for loopback testing.

## 7. BENCH LOOPBACKS (BENCH TEST ONLY)
These are for automated self-tests and **MUST BE REMOVED** in production:
- **Heater Relay:** STM32 PB15 ➔ STM32 PA4
- **Triac Gate:** STM32 PC6 ➔ STM32 PA6
- **X9C CS:** STM32 PB12 ➔ STM32 PB4
- **X9C U/D:** STM32 PB13 ➔ STM32 PB5
- **X9C INC:** STM32 PB14 ➔ STM32 PB6

## 8. RESISTORS
| ID | Value | Terminal A | Terminal B |
| :--- | :--- | :--- | :--- |
| R1 | 1kΩ | STM32 PA0 | X9C103S Pin 5 (VW) |
| R2 | 1kΩ | STM32 PB10 | RS485 Module A DI |
| R3 | 1kΩ | STM32 PB11 | RS485 Module A RO |
| R4 | 1kΩ | ESP32 GPIO4 | STM32 PC7 |
| R5 | 1kΩ | ESP32 GPIO8 | RS485 Module B DI |
| R6 | 1kΩ | ESP32 GPIO18 | RS485 Module B RO |
| R7 | 1kΩ | ESP32 GPIO17 | Nextion RX |
| R8 | 1kΩ | ESP32 GPIO16 | Nextion TX |
| R_Term_A | 120Ω | RS485 Mod A (A) | RS485 Mod A (B) |
| R_Term_B | 120Ω | RS485 Mod B (A) | RS485 Mod B (B) |
| R_L1 | 1kΩ | STM32 PB15 | STM32 PA4 |
| R_L2 | 1kΩ | STM32 PC6 | STM32 PA6 |
| R_L3 | 1kΩ | STM32 PB12 | STM32 PB4 |
| R_L4 | 1kΩ | STM32 PB13 | STM32 PB5 |
| R_L5 | 1kΩ | STM32 PB14 | STM32 PB6 |

## 9. POWER
| Source | Voltage | Target |
| :--- | :--- | :--- |
| NUCLEO 3.3V Rail | 3.3V | X9C103S Pin 3 (VH) |
| NUCLEO 5V Rail | 5.0V | X9C103S Pin 8 (VCC) |
| Independent USB | 5.0V | Nextion HMI VCC |
| Common GND Bus | 0V | X9C103S Pin 4 (VSS) & Pin 6 (VL) |
| Common GND Bus | 0V | RS485 Shield / Common |
| Common GND Bus | 0V | Nextion HMI GND |

## 10. STEP-BY-STEP ASSEMBLY ORDER
1. **Power & Ground Rails:** Establish 3.3V, 5.0V, and Common GND buses on the breadboard. Verify continuity.
2. **X9C103S Module:** Mount the digital potentiometer. Connect Power (Pins 3, 8 to rails) and GND (Pins 4, 6 to Common GND).
3. **RS485 Modules:** Mount Module A (STM32 side) and Module B (ESP32 side). Connect DE/RE pins. Install 120Ω terminating resistors (R_Term_A, R_Term_B).
4. **Resistors Placement:** Place all 1kΩ current-limiting resistors (R1 through R8) on their respective paths.
5. **STM32 Wiring:** Connect STM32 GPIO pins (PA0, PB10, PB11, PB1, PB12, PB13, PB14, PC7) through their respective resistors to target modules.
6. **ESP32 Wiring:** Connect ESP32 GPIO pins (GPIO4, GPIO8, GPIO18, GPIO5, GPIO17, GPIO16) through their respective resistors to target modules.
7. **Nextion HMI:** Connect independent 5V power, common GND, and UART RX/TX lines to ESP32.
8. **Bench Loopbacks (Test Only):** Install R_L1 through R_L5 for HIL simulated testing.
9. **Final Inspection:** Perform continuity checks on all connections before applying power.
