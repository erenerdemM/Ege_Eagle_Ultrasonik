# EAGLEULTRASONİK — PHASE 6.2 MASTER NETLIST (SINGLE SOURCE OF TRUTH)

```text
MASTER NETLIST
STATUS: VERIFIED & FROZEN

VOLTAGE:
3.3V DC / 5V DC ONLY

220V AC MAINS:
ABSOLUTELY FORBIDDEN

MOC3021 MAINS CONNECTION:
FORBIDDEN DURING BENCH TEST
```

---

## 1. FORENSIC MASTER SIGNAL NETLIST

Aşağıdaki tablo projedeki tüm fiziksel sinyal ağlarının (Net ID) tek ve mutlak doğruluk kaynağıdır (Single Source of Truth):

| Net ID | Source MCU / Device | Source GPIO / Pin | Board Connector Pin | Resistor ID & Value | Target Component / Module | Target Pin | Direction | Signal Voltage | Function / Purpose | Mode |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :-: | :-: | :--- | :-: |
| **NET-RS485-TX1** | STM32 | PB10 | Morpho CN10 Pin 25 | NONE | MAX485 #1 (STM32 Node)| Pin 4 (DI) | Çıkış | 3.3V Logic | USART3 TX Data Stream | BENCH & PROD |
| **NET-RS485-RX1** | STM32 | PB11 | Morpho CN10 Pin 18 | NONE | MAX485 #1 (STM32 Node)| Pin 1 (RO) | Giriş | 5.0V CMOS (FT)| USART3 RX Data Stream | BENCH & PROD |
| **NET-RS485-DE1** | STM32 | PB1 | Morpho CN7 Pin 24 | NONE | MAX485 #1 (STM32 Node)| Pins 2 & 3 (DE,/RE)| Çıkış | 3.3V Logic | Transceiver Direction Control | BENCH & PROD |
| **NET-RS485-TX2** | ESP32-S3 | GPIO8 | ESP32 GPIO Header | NONE | MAX485 #2 (ESP32 Node)| Pin 4 (DI) | Çıkış | 3.3V Logic | UART1 TX Data Stream | BENCH & PROD |
| **NET-RS485-RX2** | ESP32-S3 | GPIO18 | ESP32 GPIO Header | **R-DIV-UPPER (10k$\Omega$)**| MAX485 #2 (ESP32 Node)| Pin 1 (RO) | Giriş | **3.214V Max DC**| UART1 RX Data Stream (Safe) | BENCH & PROD |
| **NET-RS485-DE2** | ESP32-S3 | GPIO5 | ESP32 GPIO Header | NONE | MAX485 #2 (ESP32 Node)| Pins 2 & 3 (DE,/RE)| Çıkış | 3.3V Logic | Transceiver Direction Control | BENCH & PROD |
| **NET-RS485-BUS-A**| MAX485 #1 | Pin 6 (A) | Terminal Block A | **R-TERM1 (120$\Omega$)** | MAX485 #2 (ESP32 Node)| Pin 6 (A) | Diferansiyel| $2.5\dots3.5\text{V}$ | RS485 Differential Non-Inverting | BENCH & PROD |
| **NET-RS485-BUS-B**| MAX485 #1 | Pin 7 (B) | Terminal Block B | **R-TERM2 (120$\Omega$)** | MAX485 #2 (ESP32 Node)| Pin 7 (B) | Diferansiyel| $1.5\dots2.5\text{V}$ | RS485 Differential Inverting | BENCH & PROD |
| **NET-X9C-CS** | STM32 | PB12 | Morpho CN10 Pin 16 | NONE | X9C103S Digipot | Pin 7 (CS) | Çıkış | 3.3V Logic | Chip Select Input | BENCH & PROD |
| **NET-X9C-UD** | STM32 | PB13 | Morpho CN10 Pin 30 | NONE | X9C103S Digipot | Pin 2 (U/D) | Çıkış | 3.3V Logic | Up/Down Direction Control | BENCH & PROD |
| **NET-X9C-INC** | STM32 | PB14 | Morpho CN10 Pin 28 | NONE | X9C103S Digipot | Pin 1 (INC) | Çıkış | 3.3V Logic | Step Increment Clock Pulse | BENCH & PROD |
| **NET-X9C-VW** | X9C103S | Pin 5 (VW) | Wiper Terminal | **R-X9C-VW (1k$\Omega$)** | STM32 Microcontroller | PA0 (CN7 Pin 28) | Giriş | $0.0\dots3.3\text{V}$ | Pot Wiper ADC1_IN1 Readback | BENCH & PROD |
| **NET-X9C-VH** | Nucleo 3.3V | CN7 Pin 16 | 3.3V Rail | NONE | X9C103S Digipot | Pin 3 (VH) | Besleme | **3.30V DC** | Pot High Reference Voltage | BENCH & PROD |
| **NET-X9C-VL** | Common GND | Bus Rail | GND Rail | NONE | X9C103S Digipot | Pin 4 (VL/VSS) | Besleme | 0.0V DC | Pot Low Reference / Chip GND | BENCH & PROD |
| **NET-DIP-SW1** | STM32 | PC8 | Morpho CN10 Pin 2 | Dahili Pull-Up | DIP Switch 1 | SW1 Terminal | Giriş | 3.3V Logic | Tank ID Bit 0 (Active-LOW) | BENCH & PROD |
| **NET-DIP-SW2** | STM32 | PC9 | Morpho CN10 Pin 1 | Dahili Pull-Up | DIP Switch 2 | SW2 Terminal | Giriş | 3.3V Logic | Tank ID Bit 1 (Active-LOW) | BENCH & PROD |
| **NET-DIP-SW3** | STM32 | PC10 | Morpho CN10 Pin 3 | Dahili Pull-Up | DIP Switch 3 | SW3 Terminal | Giriş | 3.3V Logic | Tank ID Bit 2 (Active-LOW) | BENCH & PROD |
| **NET-DIP-SW4** | STM32 | PC11 | Morpho CN10 Pin 5 | Dahili Pull-Up | DIP Switch 4 | SW4 Terminal | Giriş | 3.3V Logic | Tank ID Bit 3 (Active-LOW) | BENCH & PROD |
| **NET-HEATER-OUT**| STM32 | PB15 | Morpho CN10 Pin 26 | **R-HEATER-FB (1k$\Omega$)**| STM32 Microcontroller | PA4 (CN7 Pin 32) | Çıkış/Giriş| 3.3V Logic | Isıtıcı Lojik Sinyal Loopback | **BENCH ONLY** |
| **NET-TRIAC-OUT** | STM32 | PC6 | Morpho CN10 Pin 4 | **R-TRIAC-FB (1k$\Omega$)** | STM32 Microcontroller | PA6 (CN10 Pin 13)| Çıkış/Giriş| 3.3V Logic | TIM15 Soft-start PWM Loopback | **BENCH ONLY** |
| **NET-ZC-SIM** | ESP32-S3 | GPIO4 | ESP32 GPIO Header | **R-ZC-SIM (1k$\Omega$)** | STM32 Microcontroller | PC7 (CN5-2/CN10-19)| Çıkış/Giriş| 3.3V Logic | **100Hz Kare Dalga EXTI7 Sim**| **BENCH ONLY** |
| **NET-HMI-TX** | ESP32-S3 | GPIO17 | ESP32 GPIO Header | NONE | Nextion HMI Display | Sarı Kablo (RX) | Çıkış | 3.3V Logic | UART2 HMI Transmit Stream | BENCH & PROD |
| **NET-HMI-RX** | Nextion HMI | Mavi Kablo | Display TX Cable | NONE | ESP32-S3 Board | GPIO16 | Giriş | 3.3V Logic | UART2 HMI Touch Event Receive | BENCH & PROD |
| **NET-PT100-OP** | OPAMP3 Output| OPAMP Out | OPAMP3 Pin | NONE | STM32 Microcontroller | PA1 (CN7 Pin 30) | Giriş | $0.0\dots3.3\text{V}$ | Sıcaklık Sensörü ADC2_IN2 | BENCH & PROD |

---

## 2. EXPLICIT TWO-TERMINAL RESISTOR MASTER TABLE

Projede kullanılan 9 adet direncin her iki terminali net olarak tanımlanmıştır:

| Resistor ID | Nominal Value | Terminal A (Node 1) | Terminal B (Node 2) | Purpose / Electrical Function | Bench / Production |
| :--- | :-: | :--- | :--- | :--- | :-: |
| **R-TERM1** | **120$\Omega$** | MAX485 #1 Pin 6 (A) | MAX485 #1 Pin 7 (B) | RS485 Bus Node 1 Line Termination ($120\Omega$) | BENCH & PROD |
| **R-TERM2** | **120$\Omega$** | MAX485 #2 Pin 6 (A) | MAX485 #2 Pin 7 (B) | RS485 Bus Node 2 Line Termination ($120\Omega$) | BENCH & PROD |
| **R-DIV-UPPER**| **10k$\Omega$** | MAX485 #2 Pin 1 (RO) | ESP32 GPIO18 Pin | 5V $\to$ 3.21V Voltage Divider Upper Resistor | BENCH & PROD |
| **R-DIV-LOWER**| **18k$\Omega$** | ESP32 GPIO18 Pin | Common Signal GND Bus | 5V $\to$ 3.21V Voltage Divider Lower Resistor | BENCH & PROD |
| **R-ZC-SIM** | **1k$\Omega$** | ESP32 GPIO4 Pin | STM32 PC7 (CN5-2) | 100Hz Zero-Cross Signal Protection Resistor | **BENCH ONLY** |
| **R-X9C-VW** | **1k$\Omega$** | X9C103S Pin 5 (VW) | STM32 PA0 (CN7-28) | Wiper Output Current Limiter / PA0 ADC Protection| BENCH & PROD |
| **R-HEATER-FB**| **1k$\Omega$** | STM32 PB15 (CN10-26) | STM32 PA4 (CN7-32) | Heater Output Logic Readback Loopback | **BENCH ONLY** |
| **R-TRIAC-FB** | **1k$\Omega$** | STM32 PC6 (CN10-4) | STM32 PA6 (CN10-13) | Triac TIM15 PWM Waveform Readback Loopback | **BENCH ONLY** |
| **R-MOC-LED** | **150$\Omega$** | STM32 PC6 (CN10-4) | MOC3021 Pin 1 (Anode) | Optocoupler LED Current Limiting ($14.3\text{mA}$) | **PRODUCTION ONLY**|

---

## 3. POWER RAIL & GROUND DOMAIN SPECIFICATION

| Domain ID | Power Rail Name | Voltage | Power Source | Connected Devices | Isolation Rule |
| :--- | :--- | :-: | :--- | :--- | :--- |
| **PWR-5V-NUCLEO** | Nucleo 5V Rail | 5.0V DC | USB #1 (Nucleo USB) | STM32 MCU, X9C103S VCC, MAX485 #1 VCC | **STRICTLY ISOLATED** (Do not tie to ESP32/Nextion 5V) |
| **PWR-5V-ESP32** | ESP32 5V Rail | 5.0V DC | USB #2 (ESP32 USB) | ESP32-S3 Board, MAX485 #2 VCC | **STRICTLY ISOLATED** (Do not tie to Nucleo/Nextion 5V) |
| **PWR-5V-HMI** | Nextion 5V Rail | 5.0V DC | USB #3 / Adapter | Nextion HMI Display (Kırmızı Kablo) | **STRICTLY ISOLATED** (Do not tie to Nucleo/ESP32 5V) |
| **PWR-3V3-NUCLEO**| Nucleo 3.3V Rail| 3.3V DC | Nucleo LDO (CN7-16) | X9C103S Pin 3 (VH) | **MANDATORY 3.3V** (Protects PA0 ADC) |
| **GND-COMMON** | Common Signal GND| 0.0V DC | Common Bus Rail | Nucleo GND, ESP32 GND, Nextion GND, MAX485 #1/#2 GND, X9C GND, DIP SW GND | **COMMON BUS** (All grounds tied together) |
