# PHASE 6.2 — RESISTOR TWO-TERMINAL NETLIST
**Project:** EAGLEULTRASONİK — Industrial Ultrasonic Cleaner Controller  
**Document:** Explicit Two-Terminal Resistor Netlist & Electrical Specifications  
**Status:** FULLY VERIFIED  

---

## 1. EXPLICIT RESISTOR NETLIST TABLE

| Resistor ID | Value | Terminal A Name | Terminal B Name | Connected Source (Node A) | Connected Target (Node B) | Purpose / Function | Bench / Production | Voltage A | Voltage B | Electrical Justification |
| :-: | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **R-ZC-SIM** | 1kΩ | Terminal A | Terminal B | ESP32 GPIO4 Pin | STM32 PC7 Pin (CN5-2 / CN10-19) | Limits zero-cross simulation pulse current to 3.3mA | BENCH ONLY | 0 / 3.3V | 0 / 3.3V | Prevents GPIO damage if both pins are driven simultaneously. |
| **R-HEATER-FB** | 1kΩ | Terminal A | Terminal B | STM32 PB15 Pin | STM32 PA4 Pin | Heater SSR output physical loopback readback | BENCH ONLY | 0 / 3.3V | 0 / 3.3V | Protects PA4 input and limits short-circuit current to 3.3mA. |
| **R-TRIAC-FB** | 1kΩ | Terminal A | Terminal B | STM32 PC6 Pin | STM32 PA6 Pin | Triac gate pulse physical loopback readback | BENCH ONLY | 0 / 3.3V | 0 / 3.3V | Protects PA6 input and limits gate pulse current to 3.3mA. |
| **R-X9C-VW-FB** | 1kΩ | Terminal A | Terminal B | X9C Pin 5 (VW) | STM32 PA0 (ADC1_IN1) Pin | Wiper output current limiter & PA0 ADC protection | BOTH | 0 to 3.3V | 0 to 3.3V | Keeps wiper current < 3.3mA (max rating 44mA) and protects PA0 ADC. |
| **R-RS485-1-TX**| 1kΩ | Terminal A | Terminal B | STM32 PB10 Pin | MAX485 #1 Pin 4 (DI) | UART TX protection against driver conflict | BOTH | 0 / 3.3V | 0 / 3.3V | Prevents bus driver damage during reset or floating states. |
| **R-RS485-1-RX**| 1kΩ | Terminal A | Terminal B | MAX485 #1 Pin 1 (RO) | STM32 PB11 Pin (FT_f) | 5V RS485 RO noise & current limiter to FT pin | BOTH | 0 / 5.0V | 0 / 5.0V | Limits input current to STM32 PB11 5V-tolerant pin. |
| **R-RS485-2-TX**| 1kΩ | Terminal A | Terminal B | ESP32 GPIO8 Pin | MAX485 #2 Pin 4 (DI) | ESP32 UART TX protection resistor | BOTH | 0 / 3.3V | 0 / 3.3V | Protects ESP32 GPIO8 during startup configuration. |
| **R-DIV-TOP** | 10kΩ | Terminal A | Terminal B | MAX485 #2 Pin 1 (RO) | ESP32 GPIO18 Pin | Top resistor of 5V to 3.3V voltage divider | BOTH | 0 / 5.0V | 0 / 3.21V | Steps MAX485 5V RO output down to 3.21V safe for ESP32. |
| **R-DIV-BOT** | 18kΩ | Terminal A | Terminal B | ESP32 GPIO18 Pin | COMMON GND Bus | Bottom resistor of 5V to 3.3V voltage divider | BOTH | 0 / 3.21V | 0V | Provides exact 18k / 28k attenuation ratio to GND. |
| **R-TERM-1** | 120Ω | Terminal A | Terminal B | MAX485 #1 Pin 6 (A) | MAX485 #1 Pin 7 (B) | Differential RS485 transmission line termination | BOTH | Diff A | Diff B | Matches 120Ω characteristic impedance of RS485 twisted pair. |
| **R-MOC-PROD** | 180Ω | Terminal A | Terminal B | STM32 PC6 Pin | MOC3021 Pin 1 (Anode) | Triac optocoupler LED current driver resistor | PRODUCTION ONLY | 0 / 3.3V | 1.2V | Drives MOC3021 internal LED at (3.3V - 1.2V)/180Ω = 11.6mA. |

---

## 2. BENCH VS PRODUCTION RESISTOR COUNT SUMMARY

- **Total Resistors on Bench Setup:** 10 Resistors (R-ZC-SIM, R-HEATER-FB, R-TRIAC-FB, R-X9C-VW-FB, R-RS485-1-TX, R-RS485-1-RX, R-RS485-2-TX, R-DIV-TOP, R-DIV-BOT, R-TERM-1).
- **Total Resistors in Final Production:** 8 Resistors (R-X9C-VW-FB, R-RS485-1-TX, R-RS485-1-RX, R-RS485-2-TX, R-DIV-TOP, R-DIV-BOT, R-TERM-1, R-MOC-PROD).
- **Loopback Resistors to Remove for Production:** 3 Resistors (R-ZC-SIM, R-HEATER-FB, R-TRIAC-FB).
