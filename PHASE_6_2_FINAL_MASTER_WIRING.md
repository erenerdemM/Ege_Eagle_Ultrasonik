# PHASE 6.2 — FINAL MASTER PHYSICAL WIRING TABLE
**Project:** EAGLEULTRASONİK — Industrial Ultrasonic Cleaner Controller  
**Document:** Single Authoritative Master Wiring Table  
**Status:** FULLY VERIFIED & READY FOR PHYSICAL WIRING  

---

## 1. MASTER WIRING TABLE

| ID | Source Device | Source MCU GPIO / Pin | Source Board Connector | Source Connector Pin | Signal Name | Resistor ID | Resistor Value | Resistor Terminal A | Resistor Terminal B | Target Device | Target Pin | Target Connector | Direction | Voltage | Purpose | Bench / Production | Verification Method | Pass Criteria | Evidence Source |
| :-: | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **NET-001** | NUCLEO-G474RE | PB10 | CN10 | Pin 25 | STM32_TX3 | R-RS485-1-TX | 1kΩ | PB10 Node | DI Node | MAX485 #1 | Pin 4 (DI) | Module Terminal | OUT -> IN | 0/3.3V | RS485 #1 Driver Input | BOTH | Oscilloscope / Logic Analyzer | 3.3V UART signal present | main.h:L100, DS12288 |
| **NET-002** | MAX485 #1 | Pin 1 (RO) | Module Terminal | RO | RS485_1_RO | R-RS485-1-RX | 1kΩ | RO Node | PB11 Node | NUCLEO-G474RE | PB11 | CN10 Pin 18 | OUT -> IN | 0/5.0V | RS485 #1 Receiver Output (FT pin) | BOTH | Multimeter / UART RX | Telemetry frames received | MAX485 DS, DS12288 |
| **NET-003** | NUCLEO-G474RE | PB1 | CN10 | Pin 24 | RS485_1_DE | DIRECT | 0Ω | PB1 Node | DE/RE Node | MAX485 #1 | Pin 2 & 3 (DE//RE) | Module Terminal | OUT -> IN | 0/3.3V | RS485 #1 Transmit/Receive Enable | BOTH | Multimeter DC V | HIGH during TX, LOW during RX | main.h:L102, esp32_uart.c |
| **NET-004** | MAX485 #1 | Pin 6 (A) | Screw Terminal | A | RS485_BUS_A | R-TERM-1 | 120Ω | Bus A Node | Bus B Node | MAX485 #2 | Pin 6 (A) | Screw Terminal | BIDIR | Differential | RS485 Differential Line A | BOTH | Oscilloscope A vs B | Differential voltage pulses | MAX485 DS, ISO 8482 |
| **NET-005** | MAX485 #1 | Pin 7 (B) | Screw Terminal | B | RS485_BUS_B | R-TERM-1 | 120Ω | Bus A Node | Bus B Node | MAX485 #2 | Pin 7 (B) | Screw Terminal | BIDIR | Differential | RS485 Differential Line B | BOTH | Oscilloscope A vs B | Differential voltage pulses | MAX485 DS, ISO 8482 |
| **NET-006** | ESP32-S3 | GPIO8 | Header | GPIO8 | ESP32_TX1 | R-RS485-2-TX | 1kΩ | GPIO8 Node | DI Node | MAX485 #2 | Pin 4 (DI) | Module Terminal | OUT -> IN | 0/3.3V | RS485 #2 Driver Input | BOTH | Oscilloscope / Logic Analyzer | 3.3V UART signal present | ekran_kontrol.ino:L29 |
| **NET-007** | MAX485 #2 | Pin 1 (RO) | Module Terminal | RO | RS485_2_RO | R-DIV-TOP | 10kΩ | RO Node | GPIO18 Node | ESP32-S3 | GPIO18 | Header | OUT -> IN | 0/5.0V -> 0/3.21V | Voltage Divider Top (5V to 3.3V) | BOTH | Multimeter DC V | 3.21V HIGH at GPIO18 | ESP32-S3 DS, MAX485 DS |
| **NET-008** | ESP32-S3 | GPIO18 | Header | GPIO18 | ESP32_RX1 | R-DIV-BOT | 18kΩ | GPIO18 Node | GND Node | GND Bus | GND | Common GND | IN <- OUT | 0/3.21V | Voltage Divider Bottom to GND | BOTH | Multimeter DC V | 3.21V HIGH ratio | ESP32-S3 DS |
| **NET-009** | ESP32-S3 | GPIO5 | Header | GPIO5 | RS485_2_DE | DIRECT | 0Ω | GPIO5 Node | DE/RE Node | MAX485 #2 | Pin 2 & 3 (DE//RE) | Module Terminal | OUT -> IN | 0/3.3V | RS485 #2 Transmit/Receive Enable | BOTH | Multimeter DC V | HIGH during TX, LOW during RX | ekran_kontrol.ino:L30 |
| **NET-010** | ESP32-S3 | GPIO17 | Header | GPIO17 | HMI_TXD2 | DIRECT | 0Ω | GPIO17 Node | Nextion RX | Nextion HMI | RX Pin | Blue Wire | OUT -> IN | 0/3.3V | HMI Command Transmission | BOTH | Nextion Screen Update | Commands accepted | ekran_kontrol.ino:L7 |
| **NET-011** | Nextion HMI | TX Pin | Yellow Wire | TX | HMI_RXD2 | DIRECT | 0Ω | Nextion TX Node| GPIO16 Node | ESP32-S3 | GPIO16 | Header | OUT -> IN | 0/3.3V | HMI Touch Event Transmission | BOTH | ESP32 Serial Log | Touch events logged | ekran_kontrol.ino:L6 |
| **NET-012** | ESP32-S3 | GPIO4 | Header | GPIO4 | ZC_SIM_OUT | R-ZC-SIM | 1kΩ | GPIO4 Node | PC7 Node | NUCLEO-G474RE | PC7 | CN5-2 / CN10-19 | OUT -> IN | 0/3.3V | 100Hz Zero-Cross Simulation Loopback | BENCH ONLY | Frequency Meter / EXTI ISR | 100 Hz square wave, zero_cross lost clear | main.h:L107, ekran_kontrol.ino:L52 |
| **NET-013** | NUCLEO-G474RE | PB12 | CN10 | Pin 16 | X9C_CS | DIRECT | 0Ω | PB12 Node | CS Pin Node | X9C103S | Pin 7 (CS) | DIP-8 Pin 7 | OUT -> IN | 0/3.3V | Digital Pot Chip Select | BOTH | Logic Analyzer | CS LOW during wiper steps | main.h:L86, x9c103s.c |
| **NET-014** | NUCLEO-G474RE | PB13 | CN10 | Pin 30 | X9C_UD | DIRECT | 0Ω | PB13 Node | U/D Pin Node | X9C103S | Pin 2 (U/D) | DIP-8 Pin 2 | OUT -> IN | 0/3.3V | Digital Pot Up/Down Direction | BOTH | Logic Analyzer | HIGH = Increment, LOW = Decrement | main.h:L88, x9c103s.c |
| **NET-015** | NUCLEO-G474RE | PB14 | CN10 | Pin 28 | X9C_INC | DIRECT | 0Ω | PB14 Node | INC Pin Node | X9C103S | Pin 1 (INC) | DIP-8 Pin 1 | OUT -> IN | 0/3.3V | Digital Pot Step Pulse | BOTH | Logic Analyzer | Falling edge steps wiper | main.h:L90, x9c103s.c |
| **NET-016** | X9C103S | Pin 5 (VW) | DIP-8 Pin 5 | VW | X9C_WIPER | R-X9C-VW-FB | 1kΩ | VW Node | PA0 Node | NUCLEO-G474RE | PA0 | CN7 Pin 28 | OUT -> IN | 0 to 3.3V | Wiper Voltage ADC Readback | BOTH | ADC Voltage Measurement | Step 40 ~1.32V, Step 90 ~2.97V | main.c:L514, x9c103s.c |
| **NET-017** | NUCLEO-G474RE | PB15 | CN10 | Pin 26 | HEATER_OUT | R-HEATER-FB | 1kΩ | PB15 Node | PA4 Node | NUCLEO-G474RE | PA4 | CN7 Pin 32 / CN10 Pin 17 | OUT -> IN | 0/3.3V | Heater Output Loopback Readback | BENCH ONLY | Read PA4 GPIO | PA4 == PB15 state | main.h:L92, main.c:L839 |
| **NET-018** | NUCLEO-G474RE | PC6 | CN10 | Pin 4 | TRIAC_GATE | R-TRIAC-FB | 1kΩ | PC6 Node | PA6 Node | NUCLEO-G474RE | PA6 | CN7 Pin 34 / CN10 Pin 13 | OUT -> IN | 0/3.3V | Triac Gate Firing Pulse Loopback | BENCH ONLY | Read PA6 / Oscilloscope | Gate pulse pulses on PA6 | main.h:L105, main.c:L853 |
| **NET-019** | DIP Switch | SW1 Output | Switch Pin 1 | DIP_1 | DIP_SW1_SIG | DIRECT | 0Ω | SW1 Node | PC8 Node | NUCLEO-G474RE | PC8 | CN10 Pin 2 | IN <- OUT | 0/3.3V | Tank ID Bit 0 (Pull-up) | BOTH | GPIO Read | ON = 0V, OFF = 3.3V | main.h:L111, main.c:L98 |
| **NET-020** | DIP Switch | SW2 Output | Switch Pin 2 | DIP_2 | DIP_SW2_SIG | DIRECT | 0Ω | SW2 Node | PC9 Node | NUCLEO-G474RE | PC9 | CN10 Pin 1 | IN <- OUT | 0/3.3V | Tank ID Bit 1 (Pull-up) | BOTH | GPIO Read | ON = 0V, OFF = 3.3V | main.h:L113, main.c:L98 |
| **NET-021** | DIP Switch | SW3 Output | Switch Pin 3 | DIP_3 | DIP_SW3_SIG | DIRECT | 0Ω | SW3 Node | PC10 Node | NUCLEO-G474RE | PC10 | CN7 Pin 1 | IN <- OUT | 0/3.3V | Tank ID Bit 2 (Pull-up) | BOTH | GPIO Read | ON = 0V, OFF = 3.3V | main.h:L115, main.c:L860 |
| **NET-022** | DIP Switch | SW4 Output | Switch Pin 4 | DIP_4 | DIP_SW4_SIG | DIRECT | 0Ω | SW4 Node | PC11 Node | NUCLEO-G474RE | PC11 | CN7 Pin 2 | IN <- OUT | 0/3.3V | Tank ID Bit 3 (Pull-up) | BOTH | GPIO Read | ON = 0V, OFF = 3.3V | main.h:L117, main.c:L860 |

---

## 2. POWER CONNECTIONS TABLE

| Device | Supply Source | Supply Pin | Voltage | GND Pin | GND Net | Is 5V Shared? | Is 3.3V Shared? | Reason / Authority Rule |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **NUCLEO-G474RE** | Development PC (USB #1) | CN7 Pin 18 (5V) / 3.3V | 5.0V / 3.3V | CN7 Pin 8 / CN10 Pin 20 | COMMON GND | NO | NO | Independent USB supply; GND shared for signal reference. |
| **ESP32-S3-N16R8** | Development PC (USB #2) | VIN / 5V | 5.0V / 3.3V internal | GND Pin | COMMON GND | NO | NO | Independent USB supply; 5V rails must NOT be shorted to avoid regulator backfeeding. |
| **Nextion NX4832T035** | Development PC (USB #3) / Ext 5V | Red Wire (VCC) | 5.0V DC | Black Wire (GND) | COMMON GND | NO | NO | Dedicated 5V 500mA power supply; 5V isolated, GND connected to Common GND Bus. |
| **X9C103S Digital Pot** | NUCLEO 5V & 3.3V Rails | Pin 8 (VCC=5V), Pin 3 (VH=3.3V) | 5.0V VCC / 3.3V VH | Pin 4 (VSS), Pin 6 (VL) | COMMON GND | NO | NO | VCC supplied from Nucleo 5V; VH connected strictly to 3.3V to guarantee PA0 ADC voltage safety! |
| **MAX485 Module #1** | NUCLEO 5V Rail | VCC Pin | 5.0V DC | GND Pin | COMMON GND | YES (with Nucleo) | NO | MAX485 requires 5V VCC for full RS485 differential output drive compliance. |
| **MAX485 Module #2** | ESP32 5V Rail | VCC Pin | 5.0V DC | GND Pin | COMMON GND | YES (with ESP32) | NO | MAX485 requires 5V VCC; RO output divided to 3.21V before ESP32 GPIO18. |

---

## 3. PHYSICAL WIRING SAFETY DIRECTIVES
> [!CAUTION]
> 1. **5V RAIL ISOLATION:** Never connect Nucleo 5V, ESP32 5V, and Nextion 5V together.
> 2. **COMMON GROUND BUS:** Connect Nucleo GND, ESP32 GND, Nextion GND, X9C VSS/VL, and MAX485 GNDs together on the breadboard.
> 3. **X9C VH TERMINAL:** Pin 3 (VH) MUST connect to 3.3V (NEVER to 5V).
> 4. **MAX485 RO -> ESP32 GPIO18:** Requires 10kΩ / 18kΩ resistor divider to step 5V down to 3.21V.
> 5. **HIGH VOLTAGE PROHIBITION:** 220V AC mains power is strictly prohibited on the bench setup.
