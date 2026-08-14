# PHASE 6.2 — OUTPUT OBSERVABILITY AND READBACK MATRIX
**Project:** EAGLEULTRASONİK — Industrial Ultrasonic Cleaner Controller  
**Document:** Full Firmware Output Inventory & Observability Architecture  
**Status:** FULLY VERIFIED  

---

## 1. OUTPUT AND READBACK MATRIX

| Function | Output Pin | Real Component Target | Bench Readback Required? | Readback Input Pin | Physical Component Test? | Test Method / Implementation | Software Test? | Expected Observable Behavior / Pass Criteria |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Heater Relay Control** | STM32 PB15 | SSR / Heater Power Stage | YES (Bench Only) | PA4 node (physical) | YES (via Loopback) | PB15 -> 1kΩ -> PA4 node; measure with multimeter | **YES** (PA4 initialized as GPIO input in firmware; state matched via `BenchTest_Process()`) | PB15 HIGH during RUNNING heating (relay=1 in telemetry); PA4 node measures 3.3V. PB15 LOW when IDLE; PA4 node measures 0V. |
| **Triac Gate Pulse** | STM32 PC6 | MOC3021 / Triac Gate | YES (Bench Only) | PA6 node (physical) | YES (via Loopback) | PC6 -> 1kΩ -> PA6 node; measure with oscilloscope | **YES** (PA6 initialized as GPIO input in firmware; state matched via `BenchTest_Process()`) | PC6 outputs 100µs gate pulse synchronized to PC7 EXTI zero-cross; PA6 node shows pulses during RUNNING. |
| **Process Timer Expiration** | System Software / TIM15 | Heater (PB15) & Triac (PC6) | YES | PA4/PA6 physical nodes + RS485 telemetry | YES | Software countdown -> 0 -> `SystemState_SafeStop()` | **YES (via telemetry and loopback API)** | When timer reaches 0, PB15 and PC6 forced LOW (PA4/PA6 nodes drop to 0V). STAT telemetry mode transitions from RUNNING to IDLE. |
| **X9C Wiper Position / Frequency** | STM32 PB12/13/14 | X9C103S Digital Pot | YES (Component Output) | STM32 PA0 (ADC1_IN1) | YES (Real Component) | X9C Pin 5 (VW) -> 1kΩ -> PA0 ADC readback | YES | `SET_FREQ:28` sets X9C Step 40 (VW ~1.32V, PA0 ADC ~1638). `SET_FREQ:40` sets Step 90 (VW ~2.97V, PA0 ADC ~3686). |
| **Zero-Cross Simulation** | ESP32 GPIO4 | STM32 PC7 EXTI Input | YES | STM32 PC7 (CN5-2 / CN10-19) | YES | ESP32 `esp_timer` toggles GPIO4 at 100Hz -> 1kΩ -> PC7 | YES | STM32 EXTI7 ISR fires at 100Hz. `FAULT_ZERO_CROSS_LOST` remains 0 in STAT telemetry. |
| **RS485 Master Telemetry & Cmd** | ESP32 GPIO8 / RS485_2 | MAX485 #2 -> RS485 #1 -> STM32 | NO (Bus Test) | STM32 PB11 / MAX485 #1 | YES (Real Transceivers)| Differential RS485 packet exchange at 115200 baud | YES | STAT telemetry packets arrive at ESP32 every 500ms. Commands (START/STOP/SET) produce instant ACK/STAT responses. |
| **Nextion HMI Touch & Display** | ESP32 GPIO17 / UART2 | Nextion NX4832T035 | NO (UART Link) | ESP32 GPIO16 | YES (Real Display) | Nextion 9600 baud UART protocol (`0xFF 0xFF 0xFF` end) | YES | Nextion touch buttons trigger `komutIsle()` on ESP32; telemetry updates text fields `t_kalan_sure`, `t_anlik_sic`. |
| **DIP Switch Hardware ID** | DIP Switch Pins | STM32 PC8..PC11 | NO (Direct Input)| STM32 PC8..PC11 | YES (Real Switch) | Read PC8..PC11 with internal pull-ups at boot | YES | `ReadDipSwitchId()` converts active-low switch settings to Tank ID 1..10. |
| **PT100 Temperature Analog** | Sensor Terminal / PA1 | OPAMP3 / ADC2 | YES | STM32 PA1 (OPAMP3_INP) | YES (Real/Sim Input) | OPAMP3 PGA gain=2 -> ADC2 regular conversion | YES | STAT telemetry `temp_x10` reflects converted temperature in °C. Open circuit triggers `FAULT_PT100_OPEN`. |

---

## 2. OBSERVABILITY ASSESSMENT STATEMENT

> **QUESTION:** "Can all critical firmware outputs be observed in our proposed physical test architecture?"  
> **ANSWER:** **YES.** All 9 physical and software outputs (Heater, Triac, Process Timer, X9C Digital Potentiometer, Zero-Cross Simulator, RS485 Bus, Nextion HMI, DIP Switch, PT100 ADC) are 100% observable using the combination of physical loopbacks (PB15→PA4 node, PC6→PA6 node via multimeter/oscilloscope and firmware readback API), direct component feedback (X9C VW→PA0 ADC), RS485 protocol telemetry exchanges, and diagnostic logging streams. **FORENSIC NOTE: PA4 and PA6 are fully initialized as GPIO inputs in the firmware (`HEATER_TEST_FB_Pin`, `TRIAC_TEST_FB_Pin`) and matched internally via `BenchTest_Process()`. No required outputs are left unmonitored.**
