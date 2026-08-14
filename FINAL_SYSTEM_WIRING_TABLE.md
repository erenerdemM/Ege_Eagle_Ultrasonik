# EAGLEULTRASONİK — FINAL SYSTEM WIRING MASTER TABLE

**Document Version:** 3.0.0  
**Phase:** Phase 6.1 / Phase 5.2 — Final Hardware Wiring Master Authority  
**Date:** 2026-08-11  
**Status:** **AUTHORITATIVE MASTER WIRING TABLE — READY FOR PHYSICAL CONNECTION**

---

## 1. Complete Pin-to-Pin Master Wiring Matrix

| # | Source Device | Source Pin | Source Signal | Target Device | Target Pin | Target Signal | Voltage Level | Component / Protection | Signal Direction | Verification Source | Status |
| :-: | :--- | :--- | :--- | :--- | :--- | :--- | :-: | :--- | :--- | :--- | :-: |
| **W01** | STM32 | PB10 (CN10-25) | USART3_TX | MAX485 #1 | Pin 4 (DI) | Driver Input | 3.3V TTL | Direct / 1kΩ Series | STM32 $\to$ MAX485 | `main.c`, Datasheet | **FINAL** |
| **W02** | MAX485 #1 | Pin 1 (RO) | Receiver Output | STM32 | PB11 (CN10-18) | USART3_RX | 5.0V CMOS | Direct (PB11 is 5V FT) | MAX485 $\to$ STM32 | Datasheet Table 17 | **FINAL** |
| **W03** | STM32 | PB1 (CN10-24) | RS485_DE_RE | MAX485 #1 | Pins 2 & 3 (DE,/RE)| Enable | 3.3V CMOS | Direct (High=TX, Low=RX) | STM32 $\to$ MAX485 | `esp32_uart.c`, `main.h` | **FINAL** |
| **W04** | ESP32-S3 | GPIO8 | UART1_TX | MAX485 #2 | Pin 4 (DI) | Driver Input | 3.3V TTL | Direct / 1kΩ Series | ESP32 $\to$ MAX485 | `ekran_kontrol.ino` | **FINAL** |
| **W05** | MAX485 #2 | Pin 1 (RO) | Receiver Output | Divider Node | 10kΩ Top | Voltage Div | 5.0V CMOS | **10kΩ Series Resistor** | MAX485 $\to$ Node | Datasheet Calculation | **FINAL** |
| **W06** | Divider Node| Node Output | Divider Output | ESP32-S3 | GPIO18 | UART1_RX | **3.21V Max**| **18kΩ Pulldown to GND**| Node $\to$ ESP32 | ESP32 Max 3.6V Limit | **FINAL** |
| **W07** | ESP32-S3 | GPIO5 | RS485_DE_PIN | MAX485 #2 | Pins 2 & 3 (DE,/RE)| Enable | 3.3V CMOS | Direct (High=TX, Low=RX) | ESP32 $\to$ MAX485 | `ekran_kontrol.ino` | **FINAL** |
| **W08** | ESP32-S3 | GPIO17 | TXD2 (HMI TX) | Nextion HMI| RX Wire (Yellow) | UART RX | 3.3V TTL | Direct / 1kΩ Series | ESP32 $\to$ Nextion | `ekran_kontrol.ino` | **FINAL** |
| **W09** | Nextion HMI| TX Wire (Blue)| UART TX | ESP32-S3 | GPIO16 | RXD2 (HMI RX) | 3.3V TTL | Direct / 1kΩ Series | Nextion $\to$ ESP32 | `ekran_kontrol.ino` | **FINAL** |
| **W10** | ESP32-S3 | GPIO4 | ZC_SIM_PIN | STM32 | PC7 (CN5-2/CN10-19)| EXTI7 Zero-Cross | 3.3V TTL | **1kΩ Series Resistor** | ESP32 $\to$ STM32 | `hardware_wiring_FINAL_AUTHORITY` | **FINAL** |
| **W11** | STM32 | PB12 | X9C_CS_Pin | X9C103S | Pin 7 (CS) | Chip Select | 3.3V CMOS | Direct | STM32 $\to$ X9C | `x9c103s.c`, Datasheet | **FINAL** |
| **W12** | STM32 | PB13 | X9C_UD_Pin | X9C103S | Pin 2 (U/D) | Up/Down Control | 3.3V CMOS | Direct | STM32 $\to$ X9C | `x9c103s.c`, Datasheet | **FINAL** |
| **W13** | STM32 | PB14 | X9C_INC_Pin | X9C103S | Pin 1 (INC) | Increment Pulse | 3.3V CMOS | Direct | STM32 $\to$ X9C | `x9c103s.c`, Datasheet | **FINAL** |
| **W14** | Nucleo 3V3| 3.3V Rail | 3V3_OUT | X9C103S | Pin 3 (VH) | High Terminal | **3.3V DC** | **MANDATORY 3.3V (NOT 5V!)**| Nucleo $\to$ X9C | PA0 ADC Overvoltage Guard | **FINAL** |
| **W15** | GND Bus | Common GND | GND_REF | X9C103S | Pin 6 (VL) | Low Terminal | 0V | Direct to Common GND | GND Bus $\to$ X9C | FN8158 Datasheet | **FINAL** |
| **W16** | GND Bus | Common GND | GND_REF | X9C103S | Pin 4 (VSS) | Ground | 0V | Direct to Common GND | GND Bus $\to$ X9C | FN8158 Datasheet | **FINAL** |
| **W17** | Nucleo 5V | 5V Rail | 5V_OUT | X9C103S | Pin 8 (VCC) | Power Supply | 5.0V DC | Direct to Nucleo 5V | Nucleo $\to$ X9C | FN8158 Datasheet | **FINAL** |
| **W18** | X9C103S | Pin 5 (VW) | Wiper Output | STM32 | PA0 (CN7-28) | ADC1_IN1 | 0-3.3V DC | **1kΩ Series Resistor** | X9C $\to$ STM32 | `pt100_adc.c`, Protection | **FINAL** |
| **W19** | STM32 | PB15 | HEATER_RELAY | Output / SSR| SSR Input Pin | Relay Actuation | 3.3V CMOS | Active Pull-down (Bench 1k -> PA4) | STM32 $\to$ Actuator | `heater_relay.c` | **FINAL** |
| **W20** | STM32 | PC6 | TRIAC_GATE | Optocoupler | MOC3021 Pin 1 | Gate Firing Pulse| 3.3V Pulse | Active Pull-down (Bench 1k -> PA6) | STM32 $\to$ Actuator | `main.c` | **FINAL** |
| **W21** | GND Bus | Common GND | GND_REF | DIP Switches| Switch COM | Switch Return | 0V | Common GND Bus | GND Bus $\to$ DIP SW | Active-Low Pullup Logic | **FINAL** |
| **W22** | DIP SW 1 | Switch Output 1| SW1_OUT | STM32 | PC8 (Morpho CN10-2)| DIP_SW1_Pin | 3.3V Logic| Internal PULLUP | DIP SW $\to$ STM32 | `main.c` Tank ID Bit 0 | **FINAL** |
| **W23** | DIP SW 2 | Switch Output 2| SW2_OUT | STM32 | PC9 (Morpho CN10-4)| DIP_SW2_Pin | 3.3V Logic| Internal PULLUP | DIP SW $\to$ STM32 | `main.c` Tank ID Bit 1 | **FINAL** |
| **W24** | DIP SW 3 | Switch Output 3| SW3_OUT | STM32 | PC10 (CN10-1) | DIP_SW3_Pin | 3.3V Logic| Internal PULLUP | DIP SW $\to$ STM32 | `main.c` Tank ID Bit 2 | **FINAL** |
| **W25** | DIP SW 4 | Switch Output 4| SW4_OUT | STM32 | PC11 (CN10-3) | DIP_SW4_Pin | 3.3V Logic| Internal PULLUP | DIP SW $\to$ STM32 | `main.c` Tank ID Bit 3 | **FINAL** |
| **W26** | Nucleo 5V | 5V Rail | 5V_OUT | MAX485 #1 | Pin 8 (VCC) | Transceiver Power| 5.0V DC | Direct to Nucleo 5V | Nucleo $\to$ MAX485 #1| MAX485ESA Datasheet | **FINAL** |
| **W27** | GND Bus | Common GND | GND_REF | MAX485 #1 | Pin 5 (GND) | Transceiver Ground| 0V | Direct to Common GND | GND Bus $\to$ MAX485 #1| MAX485ESA Datasheet | **FINAL** |
| **W28** | ESP32 5V | 5V Rail | 5V_OUT | MAX485 #2 | Pin 8 (VCC) | Transceiver Power| 5.0V DC | Direct to ESP32 5V | ESP32 $\to$ MAX485 #2 | MAX485ESA Datasheet | **FINAL** |
| **W29** | GND Bus | Common GND | GND_REF | MAX485 #2 | Pin 5 (GND) | Transceiver Ground| 0V | Direct to Common GND | GND Bus $\to$ MAX485 #2| MAX485ESA Datasheet | **FINAL** |
| **W30** | MAX485 #1 | Pin 6 (A) | Differential A| MAX485 #2 | Pin 6 (A) | Differential A | ±1.5V..5V | **120Ω Term Resistor end A**| Bus Interconnect | Twisted Pair Line A | **FINAL** |
| **W31** | MAX485 #1 | Pin 7 (B) | Differential B| MAX485 #2 | Pin 7 (B) | Differential B | ±1.5V..5V | **120Ω Term Resistor end B**| Bus Interconnect | Twisted Pair Line B | **FINAL** |
| **W32** | GND Bus | Common GND | GND_REF | Common Shield| Shield / Drain Wire| Shield Return | 0V | Bus Shield Drain Wire | Bus Interconnect | Common Reference | **FINAL** |
