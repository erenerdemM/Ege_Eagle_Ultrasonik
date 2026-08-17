# EAGLEULTRASONİK — SYSTEM HARDWARE COMMUNICATION AUDIT

---

## 1. Executive Summary

This report documents the forensic hardware communication audit for the EAGLEULTRASONiK controller platform. The audit covers inter-IC communication channels (ESP32 Master ↔ STM32 Slave RS485 bus, ESP32 ↔ Nextion HMI UART, STM32 ↔ X9C103S bit-bang interface) and physical-vs-simulated hardware boundaries.

---

## 2. Hardware Communication Topology & Interface Map

```
 ┌────────────────┐              RS485 Bus (Half-Duplex)             ┌────────────────┐
 │  ESP32-S3      │  GPIO8 (TX) / GPIO18 (RX) / GPIO5 (DE/RE)        │ STM32G474RE    │
 │  Master        │◄────────────────────────────────────────────────►│ Slave Node (1) │
 │  Gateway       │               115200 Baud, 8N1                   │                │
 └───────┬────────┘                                                  └───────┬────────┘
         │ UART2 (9600 8N1)                                                  │ Bit-Bang GPIO
         │ GPIO16 (RX) / GPIO17 (TX)                                         │ PB12 (CS), PB13 (UD), PB14 (INC)
         ▼                                                                   ▼
 ┌────────────────┐                                                  ┌────────────────┐
 │  Nextion HMI   │                                                  │ X9C103S        │
 │  Touch Panel   │                                                  │ Digital Pot    │
 └────────────────┘                                                  └────────────────┘
```

---

## 3. Detailed Communication Channel Audit

### A. ESP32 ↔ STM32 (Multi-Drop RS485 ASCII UART Bus)
* **Baud & Frame Format**: 115200 Baud, 8 Data Bits, No Parity, 1 Stop Bit (8N1).
* **Physical Hardware Interface**: Half-Duplex RS485 transceiver driven via `GPIO8` (TX), `GPIO18` (RX), `GPIO5` (DE/RE) on ESP32, and `PB10` (TX), `PB11` (RX), `PB1` (DE/RE) on STM32.
* **Downlink Framing (ESP32 ➔ STM32)**: Line-terminated ASCII: `T<id>:<COMMAND>\n` (e.g. `T1:START`, `T1:SET_TIME:15`, `T0:DISCOVER`).
* **Uplink Telemetry Framing (STM32 ➔ ESP32)**: `STAT,<TankID>,<mode>,<remaining_sec>,<temp_x10>,<relay>,<power_pct>,<frequency_khz>,<fault_flags>,<prov_state>,<swp_st>\n`.
* **Direction Control Timing**:
  - ESP32 (`rs485Transmit()`): Pre-drive setup 10 µs $\to$ `Serial1.print()` $\to$ `Serial1.flush()` $\to$ Post-drive hold 5 µs $\to$ DE LOW.
  - STM32 (`RS485_Transmit_Blocking()`): Asserts DE HIGH $\to$ `HAL_UART_Transmit()` $\to$ Polls `UART_FLAG_TC` $\to$ DE LOW.
* **Communication Safety Watchdog**:
  - STM32: 3000 ms RX silence watchdog (`RX_SILENCE_TIMEOUT_MS`). Trips `SystemState_SafeStop(STOP_REASON_COMM_TIMEOUT)` if no command addressed to node arrives within 3s during `RUNNING` or `DEGAS`.
  - ESP32: 1000 ms periodic heartbeat (`T<id>:HEARTBEAT`). Node offline threshold set to 3000 ms (`STM_BAGLANTI_TIMEOUT`).

### B. ESP32 ↔ Nextion HMI (Touch Display UART Interface)
* **Baud & Frame Format**: 9600 Baud, 8N1 via `Serial2` (`GPIO16` RX / `GPIO17` TX).
* **Protocol Ingestion**: `hatOku(Stream &kaynak, String &tampon, String &satir)` reads character-by-character, filtering `\r` and `0xFF`.
* **Identified Vulnerabilities**:
  1. Nextion displays send `0xFF 0xFF 0xFF` tails. Omitting `\n` in custom Nextion touch events causes `hatOku()` to store characters indefinitely without dispatching.
  2. Blocking `delay()` calls during NVS save operations block the main loop, risking RX FIFO overflow.

### C. STM32 ↔ X9C103S (Digital Potentiometer Stepping Interface)
* **Control Pins**: PB12 ($\bar{\text{CS}}$), PB13 ($U/\bar{D}$), PB14 ($\bar{\text{INC}}$).
* **Electrical Protection**: Wiper terminal $V_H$ tied to 3.3V rail; wiper output $V_W$ routed to PA0 (`ADC1_IN1`) through a 1 kΩ series protection resistor.
* **Micro-Critical Timing Sections**: Setup steps execute unblocked; IRQs disabled ONLY around individual 6 µs `INC` pulses ($t_{\text{INC}}$ low 3 µs, high 3 µs), keeping IRQ blackout <10 µs per step so Zero-Cross EXTI timing is never degraded.

---

## 4. Master vs. Slave Authority & Response Blind Spots

> [!WARNING]
> **Critical Architectural Finding: ESP32 Response Blind Spot**  
> While STM32 generates explicit ASCII ACK/NACK/ERR responses for incoming commands (`ACK:SWEEP:ON`, `ERR:LOCKED_SYS_RUNNING`, `ERR:SWEEP_PROHIBITED_IN_DEGAS`, `NACK,STAGE_ID,...`), the ESP32 background loop (`loop()`) **ONLY parses incoming lines starting with `STAT,`**. All ACK/NACK/ERR responses sent by STM32 slaves are discarded by `stmTelemetryIsle()`. The ESP32 Master receives no feedback when a command is rejected or fails at the slave level!

---

## 5. Physical Bench Hardware vs. Unavailable Production Hardware

| Component | Physical Bench State (PRESENT) | Production Hardware (UNAVAILABLE) | Boundary / Simulation Mechanism |
| :--- | :--- | :--- | :--- |
| **Microcontrollers** | Nucleo-G474RE, ESP32-S3, Nextion HMI | N/A (Present) | Direct Hardware Execution & USB-Serial HIL |
| **Ultrasonic Transducer / Power Card** | **ABSENT** | Piezo stack & AC power MOSFET card | TIM15 / TIM1 OPM PWM generation; frequency sweep verified via X9C pot + PA0 ADC readback |
| **PT100 Sensor Probe** | **ABSENT** | Industrial PT100 RTD sensor element | Simulated DC potential on PA1; OPAMP3 (gain=2) internal output sampled by ADC2 |
| **Heater & Triac Stage** | **ABSENT** | 220V AC immersion heater & SSR load | Bench loopbacks (PB15 $\to$ PA4, PC6 $\to$ PA6); zero-cross simulated via ESP32 GPIO4 100Hz square wave $\to$ PC7 |
| **RS485 Transceivers** | Direct 3.3V TTL UART bench wires | MAX485 differential transceiver ICs | Direct 3.3V TTL point-to-point connection with 1 kΩ series protection resistors |
