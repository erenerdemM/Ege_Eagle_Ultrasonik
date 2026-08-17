# EAGLEULTRASONİK — MASTER END-TO-END SYSTEM VERIFICATION PLAN

---

## 1. Executive Summary & Verification Strategy

This document defines the authoritative Master End-to-End System Verification Plan for the EAGLEULTRASONiK project. 

The verification strategy establishes a multi-tiered, risk-managed verification pipeline designed to validate all **47 system functions** across **10 end-to-end operational flows**, **12 negative/safety test scenarios**, and **7 multi-tank isolation boundaries**.

### Verification Philosophy:
1. **Preserve Loopback Discipline:** Utilize physical hardware loopback and signal injection on currently available hardware (STM32 Nucleo, ESP32, Nextion HMI, X9C103S pot, RS485 bus, RPi test host).
2. **Strict Test Level Classification:** Distinguish between unit/mock software verification (Level 1/2), physical MCU/HMI/RS485 loop verification (Level 3), and final physical transducer/tank qualification (Level 4).
3. **Zero False Claims:** Do NOT claim unavailable physical hardware (ultrasonic transducer, power card, physical PT100 probe, AC heater load, liquid tank) was tested physically. Software/HIL oscilloscope traces must be clearly reported as Level 3 Loopback.
4. **Reuse Existing Test Suites:** Maximize regression reuse of `test_hil_uart.py` (20 HIL tests), `test_hmi_mock.py` (22 mock tests), and `test_rs485_mock.py` (26 mock tests).

---

## 2. Test Level Definitions

| Test Level | Level Name | Definition & Scope | Hardware Required | Available Status |
| :--- | :--- | :--- | :--- | :--- |
| **LEVEL 1** | Unit & Mock Verification | Software-only unit tests, mock serial parsers, NVS simulation, and protocol state assertions. | None (Python Pytest runner) | **AVAILABLE NOW** |
| **LEVEL 2** | Protocol & HMI Verification | Software mock verification of RS485 bus collision, CRC error injection, multi-drop addressing, and Nextion UI page transitions. | None (Python Pytest runner) | **AVAILABLE NOW** |
| **LEVEL 3** | Physical Loop & HIL Verification | Hardware-in-the-loop verification on real physical MCUs, RS485 transceiver, Nextion HMI display, X9C103S digital pot, and oscilloscope/meter readback. | STM32 Nucleo, ESP32, Nextion HMI, X9C103S pot, RS485 bus, RPi host | **AVAILABLE NOW** |
| **LEVEL 4** | Physical Final Hardware Qualification | Full physical qualification with high-voltage AC Triac power card, ultrasonic transducer motor, physical PT100 probe, heater element, and liquid tank system. | Ultrasonic transducer, power card, PT100 probe, heater load, liquid tank | **UNAVAILABLE** (Postponed to Phase 6 physical commissioning) |

---

## 3. Master 47-Function Verification Matrix

Below is the complete verification specification for all 47 system functions:

| Function ID | Function Name | Required Level | Verification Method | Prerequisites | Input Signal / Command | Expected Behavior | Observable Output | Safety Requirement | Pass Criteria | Available Now? | Missing HW |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `SYS-BOOT` | Boot Init | Level 3 | Hardware Boot Readback | Power applied | Power ON | MCU initializes clocks, peripherals, disarms outputs | Debug UART log `BOOT_OK` | Outputs disarmed during boot | Flash binary boots cleanly | YES | None |
| `SYS-STATE` | State Machine | Level 3 | State Transition Test | Boot complete | `START`, `STOP`, `PAUSE` | State machine transitions cleanly | Telemetry `STAT` mode field | Exclusions enforced | Valid mode transitions | YES | None |
| `SYS-SAFESTOP` | SafeStop | Level 3 | SafeStop Signal Trigger | RUNNING state | SafeStop trigger / `STOP` | Forces PWM=0%, Triac=OFF, Relay=OFF | Telemetry `STAT` disarmed | Atomic execution | All outputs cut <5ms | YES | None |
| `SYS-FAULT` | Fault Handling | Level 3 | Fault Injection Test | RUNNING state | Disconnect sensor / Comm loss | Aggregates fault code into bitmask | HMI Fault Popup & `ERR=...` | Immediate SafeStop | Correct bitmask logged | YES | None |
| `SYS-WATCHDOG-HW`| IWDG Watchdog | Level 3 | Superloop Freeze Test | IDLE state | Block main loop >2000ms | STM32 hardware IWDG resets MCU | MCU reboots to SafeStop | Independent of software | MCU reboots cleanly | YES | None |
| `SYS-RESET` | Reset Recovery | Level 3 | Reboot Inspection | IWDG reset | Power cycle after reset | Reads `RCC_FLAG_IWDGRST`, sets fault | Telemetry `ERR_WATCHDOG` | SafeStop on boot | SafeStop active on boot | YES | None |
| `ID-UID-DISC` | UID Discovery | Level 3 | Slotted RS485 Query | Unprovisioned node | `T0:DISCOVER` | Responds with 96-bit UID in slot | RS485 `DISC,UID=...` frame | Slotted timing guard | Valid UID string | YES | None |
| `ID-STAGE` | Staging Flow | Level 3 | RS485 Staging Command | Unprovisioned node | `T0:STAGE_ID=<UID>,<ID>` | Stages target Tank ID in RAM | RS485 `STAGED,ID=...` ACK | Blocked if RUNNING | ACK received | YES | None |
| `ID-ASSIGN` | Assignment Flow | Level 3 | RS485 Commit Command | Staged node | `T0:ASSIGN_ID=<UID>,<ID>` | Writes Tank ID to Flash Page 127 | RS485 `ASSIGNED,ID=...` ACK | Atomic Flash write | ID persists across reboot| YES | None |
| `ID-RESET` | ID Reset | Level 3 | RS485 Reset Command | IDLE mode | `T<ID>:RESET_ID` | Erases Flash ID sector, resets to 0 | RS485 `RESET_OK` ACK | Blocked if RUNNING | Flash sector erased | YES | None |
| `ID-PERSIST` | Flash ID Persistence| Level 3 | Cold Reboot Readback | ID Assigned | Power cycle MCU | Reloads assigned Tank ID on boot | Telemetry `STAT,<ID>,...` | Flash CRC check | Correct ID on boot | YES | None |
| `ID-ROUTING` | Multi-Drop Routing | Level 3 | Addressed RS485 Frames | Multiple nodes | `T1:`, `T2:`, `T0:` | Processes frame only if ID matches | Slave response or silent | Prevents bus collision | Targeted execution | YES | None |
| `COM-UART-DRIVER`| UART Drivers | Level 3 | Serial Loopback | Boot complete | 115200 baud serial bytes | DMA / Ring buffer receives stream | Zero buffer overrun | Buffer overflow cap | 100% byte integrity | YES | None |
| `COM-RS485-DIR` | RS485 DE/RE Control| Level 3 | GPIO Oscilloscope Trace | Transmit event | Send packet over RS485 | PB1/GPIO5 DE pin set HIGH for TX | Oscilloscope DE pulse | Stop bit delay guard | No truncated stop bits | YES | None |
| `COM-FRAME-PARSER`| Line Frame Parser | Level 3 | Malformed Frame Injection| UART RX stream | `T1:SET_PWR=50\r\n` | Parses command key & value | Extracted `PWR=50` | Rejects frames >64B | Valid command parsing | YES | None |
| `COM-TELEMETRY` | Telemetry Framing | Level 3 | Telemetry Readback | Continuous | Telemetry timer 10Hz | Outputs ground-truth ASCII frame | RS485 `STAT,1,RUNNING,...` | Non-blocking sprintf | Valid ASCII frame | YES | None |
| `COM-CLAMPING` | Parameter Clamping | Level 3 | Out-of-Bounds Input | Send `SET_PWR=150` | `SET_PWR=150` | Clamps value to 100% max | Telemetry `PWR=100` | Limits hardware drive | Value clamped | YES | None |
| `COM-CRC16` | CRC16 Checksum | Level 3 | Corrupted CRC Injection | Send frame with bad CRC | Bad CRC packet | Rejects frame, increments CRC err | Telemetry `ERR_CRC` | Corrupted packet dropped| Frame dropped | YES | None |
| `COM-WATCHDOG` | 3000ms Bus Watchdog| Level 3 | RS485 Cable Sever Test | RUNNING state | Disconnect RS485 cable | Silence >3000ms triggers SafeStop | SafeStop & `ERR_COMM_TO` | Immediate disarm | Outputs cut <3005ms | YES | None |
| `COM-DIAG` | Bus Diagnostics | Level 3 | Diagnostic Query | RS485 active | Send `T1:DIAG?` | Returns error counters | RS485 `DIAG,CRC=...` | Suppressed on `T0` | Diagnostic readback | YES | None |
| `STM-TIM15-PWM` | Soft-Start PWM | Level 3 | Scope Duty Ramping | Set power 100% | `START` | TIM15 PWM ramps 0% ➔ 100% | Oscilloscope PB14 PWM | Clamped to 100% max | Smooth ramp trace | YES | Oscilloscope |
| `STM-ZERO-CROSS`| Zero-Cross EXTI | Level 3 | EXTI Pulse Injection | AC active / bench sim | 100Hz pulse on PB12 | EXTI ISR triggers delay timer | Oscilloscope sync pulse | ISR execution <5μs | Interrupt sync trace | YES | AC Line Module |
| `STM-TRIAC-PHASE`| Triac Phase Angle | Level 3 | Gate Pulse Scope Trace | Zero-cross synced | Set heater power 50% | Delays Triac gate firing by 5ms | Oscilloscope PB0 gate | Disarmed on SafeStop | Firing delay trace | YES | AC Heater Load |
| `STM-X9C103S` | Pot Freq Switch | Level 3 | Digital Pot Wiper Read | `SET_FREQ=40` | Switch to 40kHz | Stepping pulses on PA8/PA9/PA10 | Meter wiper resistance | Wiper bounded 0–99 | Resistance step trace | YES | None |
| `STM-PT100-ADC` | PT100 ADC Processing| Level 3 | ADC Voltage Injection | Analog signal | Inject 1.25V on PA1 | 16-sample moving avg ➔ °C | Telemetry `temp_x10` | Open circuit fault | Linear °C correlation | YES | PT100 Probe |
| `STM-HEATER-RELAY`| Relay Hysteresis | Level 3 | Temperature Ramp Test | Temp < Target - 1.0°C | Temperature drop | PB0 Relay GPIO set HIGH | Multimeter PB0 HIGH | Disarmed on SafeStop | ±1.0°C hysteresis band | YES | SSR / Heater |
| `STM-TIMER-DOWN`| Process Timer | Level 3 | Countdown Verification | Duration = 1 min | `START` | Decrements timer every 1000ms | Telemetry countdown | SafeStop at 00:00 | Auto-stop at zero | YES | None |
| `ESP-MASTER-LOOP`| FreeRTOS Scheduler | Level 2 | FreeRTOS Task Audit | Dual-core boot | Task execution | Dispatches HMI & RS485 tasks | Thread-safe queue | Queue overflow guard | Zero task crashes | YES | None |
| `ESP-NVS-RECIPE` | NVS Flash Storage | Level 2 | Power Cycle NVS Test | Edit Recipe P1 | Save Recipe P1 ➔ Reboot | Preferences loads saved values | HMI Recipe readback | Key length <=15 chars | NVS data restored | YES | None |
| `ESP-CONN-MON` | Connection Freshness| Level 2 | Telemetry Silence Test | Node connected | Stop slave telemetry | Marks node OFFLINE after 3000ms | HMI Offline Icon | Prevents START | Node marked OFFLINE | YES | None |
| `ESP-SVC-AUTH` | Service Auth | Level 2 | PIN Keypad Lockout | Service page touch | Enter PIN `123456` | Unlocks menu; locks after 300s | HMI Service Unlocked | Locked on wrong PIN | Access granted | YES | None |
| `ESP-ZERO-SIM` | Zero-Cross Simulator| Level 2 | `esp_timer` Readback | Bench mode active | Enable timer | 100Hz periodic hardware interrupt | Scope 100Hz pulse | Gated during AC input | 100Hz pulse trace | YES | None |
| `HMI-PAGE-HOME` | Home Screen Display| Level 3 | Nextion Protocol Test | HMI connected | Telemetry update | Renders power, temp, timer, status | Nextion Display Screen | Double-buffer sync | Real-time UI render | YES | None |
| `HMI-RECIPE-P123`| Recipe Pages Edit | Level 3 | HMI Touch Interaction | IDLE mode | Touch Recipe P1 | Loads P1 parameters to screen | HMI Parameter Text | Blocked in RUNNING | Parameters updated | YES | None |
| `HMI-QUICK-WASH`| Quick-Wash Exec | Level 3 | One-Touch Exec Test | IDLE mode | Touch Quick Wash | Dispatches `START` to active tank | Telemetry `RUNNING` | Blocked if OFFLINE | Process starts | YES | None |
| `HMI-FREQ-SEL` | Dual-Freq Toggle | Level 3 | HMI Toggle Button | IDLE mode | Touch 28k/40k Toggle | Sends `SET_FREQ` command | Telemetry `FREQ=40` | Blocked in RUNNING | Freq mode switched | YES | None |
| `HMI-SVC-PAGE` | Service Settings | Level 3 | Calibration Menu Test | Auth Service PIN | Touch Service Tab 3 | Renders Tank ID & calibration menu | Nextion Service Menu | Auto-logout 300s | Service menu active | YES | None |
| `HMI-OP-LOCKOUT`| Operator Lockout | Level 3 | Touch Attempt RUNNING | RUNNING state | Touch Recipe Edit button | Disables touch inputs on UI | HMI "LOCKED" banner | Protects active process| Touch inputs ignored | YES | None |
| `HMI-FAULT-POPUP`| Fault Alarm Display | Level 3 | Fault Injection Alert | Fault flag set | Trigger sensor fault | Displays red alert modal popup | Nextion Alert Popup | Requires manual ACK | Red popup rendered | YES | None |
| `SWP-FREQ-SWEEP`| Frequency Sweep | Level 3 (Voltage) / Level 4 (Acoustic)| Sweep Step Trace | RUNNING state | `SWEEP:ON` | Modulates X9C pot wiper steps | Scope voltage steps | Bounded 25k–43kHz | Voltage step trace (Level 3 PASS; Acoustic Level 4 DEFERRED)| YES (L3) / NO (L4) | Transducer & Power Card |
| `DEG-PULSE-DEGAS`| Degas Pulsed Mode | Level 3 (PWM Burst) / Level 4 (Liquid DO)| Gated PWM Burst Test | RUNNING state | `START_DEGAS` | Gated PWM duty bursts (1000ms ON/500ms OFF)| Scope PWM burst trace | Soft-start each burst | PWM burst trace (Level 3 PASS; Liquid Level 4 DEFERRED)| YES (L3) / NO (L4) | Liquid Tank & Sensor |
| `SAF-PARAM-CLAMP`| Out-of-Bounds Guard | Level 3 | Malformed Packet Test| Protocol parser | Inject corrupted packet | Clamps value / drops packet | Telemetry `ERR_MAL` | Hardware protection | Malformed frame dropped| YES | None |
| `SAF-COMM-OFFLINE`| Offline START Block | Level 3 | START Attempt Offline | Node OFFLINE | Touch START on HMI | Blocks START command dispatch | HMI "NODE OFFLINE" | Eliminates open-loop | START command blocked | YES | None |
| `SAF-EXCLUSION` | Mode Exclusions | Level 3 | Unsafe Mode Command | RUNNING state | Send `SWEEP:ON` in DEGAS | Rejects command with error code | RS485 `ERR_EXCLUSION` | Disarms invalid mode | Unsafe mode blocked | YES | None |
| `TST-HIL-SUITE` | HIL Pytest Suite | Level 3 | Automated Pytest Run | HIL hardware loop | Execute `pytest test_hil_uart.py` | Runs 20 real UART integration tests | Pytest output log | 100% pass rate req | 20/20 PASSED | YES | None |
| `TST-HMI-MOCK` | HMI Mock Suite | Level 1/2 | Automated Pytest Run | Mock environment | Execute `pytest test_hmi_mock.py` | Runs 22 HMI protocol mock tests | Pytest output log | 100% pass rate req | 22/22 PASSED | YES | None |
| `TST-RS485-MOCK` | RS485 Mock Suite | Level 1/2 | Automated Pytest Run | Mock environment | Execute `pytest test_rs485_mock.py` | Runs 26 RS485 bus collision tests | Pytest output log | 100% pass rate req | 26/26 PASSED | YES | None |

---

## 4. End-to-End Verification Flows (10 Master Flows)

### FLOW-01: POWER / BOOT SEQUENCING
* **Sequence:** Apply 24V DC / 3.3V Power ➔ STM32 `main()` boot ➔ RCC 170MHz Clock ➔ Disarm PWM & Relays ➔ ESP32 FreeRTOS Kernel boot ➔ Nextion HMI initialization ➔ RS485 UART setup ➔ Read persistent Tank ID from Flash ➔ Enter safe `SYS_MODE_IDLE`.
* **Observable Validation:** STM32 debug UART `BOOT_OK`, ESP32 serial sync, Nextion Home screen renders `IDLE`.
* **Level & Available Now:** Level 3 — **YES**.

### FLOW-02: MANAGED TANK ID PROVISIONING
* **Sequence:** Unprovisioned node boots (ID=0) ➔ Master issues `T0:DISCOVER` ➔ Node calculates slotted delay from 96-bit UID and responds `DISC,UID=...` ➔ Technician logs into HMI Service Menu with Service PIN `123456` ➔ Master issues `T0:STAGE_ID=<UID>,<ID>` ➔ Node responds `STAGED` ➔ Master issues `T0:ASSIGN_ID=<UID>,<ID>` ➔ STM32 writes ID to Flash Page 127 ➔ MCU reboot ➔ Readback confirms persistent Tank ID.
* **Observable Validation:** `id_full_lifecycle_test.py` output, `test_hil_uart.py` discovery test.
* **Level & Available Now:** Level 3 — **YES**.

### FLOW-03: NORMAL PROCESS EXECUTION
* **Sequence:** Operator selects Recipe P1 on HMI ➔ Parameters (Power 80%, Temp 60°C, Time 15m) loaded ➔ Operator presses START ➔ ESP32 verifies target node ONLINE ➔ ESP32 dispatches `T1:START` over RS485 ➔ STM32 transitions to `SYS_MODE_RUNNING` ➔ TIM15 soft-starts PWM (0% ➔ 80%) ➔ PT100 ADC reads temperature ➔ Heater relay toggles on PB0 ➔ Telemetry `STAT,1,RUNNING,...` streams 10Hz to ESP32 ➔ Nextion HMI renders active countdown & temperature.
* **Observable Validation:** TIM15 PWM oscilloscope ramp, PB0 relay GPIO High, Nextion screen sync.
* **Level & Available Now:** Level 3 — **YES**.

### FLOW-04: USER STOP & SAFESTOP DISARM
* **Sequence:** Machine in `SYS_MODE_RUNNING` at 100% power ➔ Operator touches STOP on HMI ➔ ESP32 dispatches `T1:STOP` ➔ STM32 executes `SystemState_SafeStop(USER_STOP)` ➔ TIM15 PWM duty forced to 0% ➔ PB0 Heater relay forced OFF ➔ State transitions to `SYS_MODE_IDLE` ➔ Telemetry updates `STAT,1,IDLE,...` ➔ HMI restores IDLE Home screen.
* **Observable Validation:** PWM duty cuts to 0% <5ms, PB0 GPIO drops LOW, HMI IDLE text.
* **Level & Available Now:** Level 3 — **YES**.

### FLOW-05: COMMUNICATION LOSS SAFETY RECOVERY
* **Sequence:** Machine in `SYS_MODE_RUNNING` ➔ RS485 bus cable severed / master silent ➔ STM32 communication watchdog timer ticks ➔ Time elapsed >3000ms ➔ STM32 triggers `SystemState_SafeStop(COMM_TIMEOUT)` ➔ Sets fault bit `ERR_COMM_TIMEOUT` ➔ Disarms PWM & Relay ➔ ESP32 marks node `OFFLINE` ➔ HMI displays alert "COMMUNICATION LOST".
* **Observable Validation:** Automated disarm at 3000ms tick, `test_hil_uart.py` comm loss test case.
* **Level & Available Now:** Level 3 — **YES**.

### FLOW-06: DUAL-FREQUENCY SELECTION & VALIDATION
* **Sequence:** Machine in `SYS_MODE_IDLE` ➔ Operator touches 28kHz/40kHz toggle button on HMI ➔ ESP32 dispatches `T1:SET_FREQ=40` ➔ STM32 validates requested frequency (28/40 allowed) ➔ `X9C103S_SetFrequency(40)` sends pulses on PA8/PA9/PA10 ➔ Digital pot wiper steps to position 90 ➔ Telemetry reports `FREQ=40` ➔ Send invalid `SET_FREQ=35` ➔ STM32 rejects command with `ERR_INVALID_FREQ`.
* **Observable Validation:** Multimeter resistance readback on X9C103S wiper, error response on 35kHz.
* **Level & Available Now:** Level 3 — **YES**.

### FLOW-07: FREQUENCY SWEEP MODULATION EXECUTION
* **Sequence:** Machine in `SYS_MODE_RUNNING` ➔ Operator touches SWEEP button ➔ ESP32 dispatches `T1:SWEEP:ON` ➔ STM32 enables sweep modulation ➔ `x9c103s.c` steps digital pot wiper in triangle wave pattern (±1.5kHz around center) every 50ms ➔ Telemetry confirms `SWEEP_ACTIVE` ➔ Issue `SET_FREQ=28` ➔ STM32 automatically terminates sweep and applies static center ➔ Touch STOP ➔ SafeStop disarms sweep and resets pot to center step 40.
* **Observable Validation:** Oscilloscope wiper voltage triangle wave trace, SafeStop reset.
* **Level & Available Now:** Level 3 (Voltage step trace) — **YES** / Level 4 (Transducer acoustic sweep) — **DEFERRED**.

### FLOW-08: DEGAS PULSED CAVITATION CYCLE
* **Sequence:** Technician configures DEGAS parameters in Service Menu ➔ Saved to ESP32 NVS ➔ Operator touches DEGAS button on Home screen (`degas_armed = true`) ➔ Touch temp setpoint ➔ DEGAS automatically disarms ➔ Re-arm DEGAS and touch START ➔ ESP32 dispatches `T1:START_DEGAS` ➔ STM32 enters `SYS_MODE_DEGAS` ➔ Gated TIM15 PWM burst modulation begins (1000ms ON at 100% / 500ms OFF at 0%) with soft-start ramp per burst ➔ Nextion HMI displays "DEGAS ACTIVE" with touch lockout ➔ Countdown timer reaches zero ➔ SafeStop disarms outputs ➔ HMI renders "DEGAS COMPLETE".
* **Observable Validation:** Oscilloscope gated PWM burst trace, Nextion countdown & lockout.
* **Level & Available Now:** Level 3 (PWM burst trace) — **YES** / Level 4 (Liquid DO curve) — **DEFERRED**.

### FLOW-09: MULTI-TANK ADDRESSABLE ISOLATION
* **Sequence:** Connect Tank 1 and Tank 2 to RS485 bus ➔ HMI selects Tank 1 and starts process ➔ ESP32 dispatches `T1:START` ➔ Tank 1 enters `SYS_MODE_RUNNING` while Tank 2 remains in `SYS_MODE_IDLE` ➔ Edit Tank 2 parameters on HMI ➔ NVS updates Tank 2 without affecting Tank 1 ➔ Trigger fault on Tank 1 ➔ Tank 1 enters SafeStop while Tank 2 operates unperturbed ➔ HMI header correctly reflects selected tank state.
* **Observable Validation:** `test_rs485_mock.py` multi-tank suite (26 tests), physical dual Nucleo bus.
* **Level & Available Now:** Level 3 — **YES**.

### FLOW-10: DECOMMISSIONING & RECOMMISSIONING RECOVERY
* **Sequence:** Commissioned Tank 1 in IDLE ➔ Technician issues `T1:RESET_ID` ➔ STM32 erases Flash Page 127, resets ID to 0, transitions to `SYS_MODE_UNCOMMISSIONED` ➔ Master `T0:DISCOVER` query discovers node ➔ Staging & Assignment workflow executed for Tank 2 ➔ Flash committed ➔ Reboot confirms persistent Tank 2 identity.
* **Observable Validation:** `id_full_lifecycle_test.py` complete output log.
* **Level & Available Now:** Level 3 — **YES**.

---

## 5. Loop Test Design for Missing Final Hardware

Because final high-voltage power cards, ultrasonic transducers, physical PT100 probes, and liquid tanks are currently unavailable, explicit Level 3 simulation and injection procedures are established:

### 1. Ultrasonic Power Output Simulation
* **Method:** Connect TIM15 CH1/CH1N outputs (PB14/PB15) to a dual-channel digital storage oscilloscope. Measure soft-start duty cycle ramp rate (0% to 100% in 500ms), pulse frequency (20kHz–40kHz), and immediate disarm upon SafeStop (<5ms).
* **Boundary:** Explicitly reported as **Level 3 TIM15 Duty Cycle Trace**. Must NOT be called physical acoustic power validation.

### 2. PT100 Temperature Sensor Simulation
* **Method:** Inject analog DC voltage (0.5V to 2.5V corresponding to 0°C to 100°C) into STM32 ADC1 Channel 3 (PA1 OPAMP3 input) using a precision potentiometer / DAC. Verify 16-sample moving average filter, linear temperature readback, and ±1.0°C relay hysteresis toggle on PB0.
* **Boundary:** Explicitly reported as **Level 3 ADC Voltage Injection**. Must NOT be called physical PT100 RTD sensor validation.

### 3. AC Heater & Triac Phase Firing Simulation
* **Method:** Inject 100Hz square wave pulse train into EXTI pin (PB12) using a signal generator to simulate AC zero-cross events. Measure PB0 Triac gate firing pulse delay on oscilloscope relative to zero-cross edge.
* **Boundary:** Explicitly reported as **Level 3 Zero-Cross EXTI Gate Delay Trace**. Must NOT be called physical AC thermal performance validation.

### 4. Transducer Frequency Sweep Simulation
* **Method:** Connect X9C103S digital pot wiper (PA4) to a digital multimeter / oscilloscope. Measure wiper voltage step ladder ($1.05\text{ V} \dots 1.58\text{ V}$ for 28kHz range) and 50ms step timing during sweep modulation.
* **Boundary:** Explicitly reported as **Level 3 X9C Pot Wiper Voltage Ladder**. Must NOT be called acoustic cavitation sweep validation.

---

## 6. Safety & Negative Test Scenarios

1. **NEG-01 (Invalid Command):** Send malformed string `T1:INVALID_CMD=123`. Verified: Dropped, logs `ERR_MALFORMED`.
2. **NEG-02 (Out-of-Bounds Parameter):** Send `T1:SET_PWR=250`. Verified: Clamped to 100% max.
3. **NEG-03 (Wrong Tank ID):** Send `T5:START` when local ID is 1. Verified: Ignored silently to prevent bus collision.
4. **NEG-04 (Duplicate ID Conflict):** Two nodes configured with ID 1. Verified: Discovery slotted timing prevents collision.
5. **NEG-05 (Missing Target Node):** Issue command to disconnected Tank 3. Verified: ESP32 marks node OFFLINE after 3000ms.
6. **NEG-06 (RS485 Comm Loss):** Sever RS485 bus during RUNNING. Verified: STM32 comm watchdog disarms outputs at 3000ms.
7. **NEG-07 (PT100 Open Circuit):** Disconnect ADC input (>3.0V). Verified: SafeStop triggered, `ERR_PT100_OPEN` set.
8. **NEG-08 (Zero-Cross Loss):** Silence zero-cross EXTI pulses during RUNNING. Verified: SafeStop triggered, `ERR_ZERO_CROSS` set.
9. **NEG-09 (User STOP During RUNNING):** Touch STOP on HMI during 100% power. Verified: SafeStop cuts PWM to 0% <5ms.
10. **NEG-10 (MCU Reboot During RUNNING):** Assert hardware reset during active operation. Verified: MCU boots cleanly to SafeStop IDLE.
11. **NEG-11 (Concurrent Sweep & DEGAS):** Send `SWEEP:ON` while in `SYS_MODE_DEGAS`. Verified: Rejected with `ERR_SWEEP_PROHIBITED`.
12. **NEG-12 (Unauthorized Service Access):** Attempt service menu without PIN. Verified: Access denied, menu locked.

---

## 7. Multi-Tank Isolation Test Strategy

Using the maximum supported 10-node logical tank range ($T1 \dots T10$):

* **Tank ID Isolation:** Verified by sending commands to $T1$ through $T10$ on shared RS485 bus. Only addressed node responds.
* **NVS Isolation:** ESP32 `Preferences.h` uses separate NVS key prefixes per tank (`t1_pwr`, `t2_pwr`). Updates to $T2$ leave $T1$ NVS untouched.
* **Process State Isolation:** $T1$ in RUNNING state while $T2$ is in IDLE mode. Fault on $T1$ does not interrupt $T2$.
* **Telemetry Isolation:** Master parses telemetry stream by Tank ID header (`STAT,1,...` vs `STAT,2,...`) and updates corresponding HMI data buffer.
* **HMI Selected-Tank Consistency:** Nextion header selector (`secili_goz`) switches active screen parameters cleanly between tanks.

---

## 8. Regression Strategy

Existing test suites MUST be reused to prevent verification duplication:

1. **`test_hil_uart.py` (20 Pytest Cases):** Covers physical UART telemetry, provisioning workflows (`DISCOVER`, `STAGE_ID`, `ASSIGN_ID`, `RESET_ID`), frequency switching, soft-start PWM, and comm loss watchdog.
2. **`test_hmi_mock.py` (22 Pytest Cases):** Covers Nextion UI serial protocol parsing, dual-buffer state sync, recipe editing, service PIN authentication, and connection watchdog.
3. **`test_rs485_mock.py` (26 Pytest Cases):** Covers multi-drop RS485 bus addressing ($T1 \dots T10$), broadcast ($T0$), CRC error injection, and frame corruption rejection.

New test scripts will ONLY be created if a new integration edge is introduced in future phases.

---

## 9. Pass / Fail / Blocked / Deferred Definitions

* **`PASS`:** Test executed cleanly on target environment and all assertions/expected behaviors were empirically observed.
* **`FAIL`:** Test executed on target environment but one or more assertions failed or incorrect behavior was observed.
* **`BLOCKED`:** Test could not be executed due to a prerequisite infrastructure or build failure.
* **`DEFERRED`:** Final physical hardware qualification intentionally postponed because physical transducer, PT100 probe, or liquid tank is unavailable.
* **`NOT APPLICABLE`:** Function does not require execution at that specific test level.

> **CRITICAL RULE:** A skipped or unexecuted test must NEVER be converted into `PASS`.

---

## 10. Master Execution Sequence (17 Steps)

Future end-to-end verification execution MUST follow this strict 17-step order:

1. **Environment Sanity Check:** Verify 24V/3.3V power rails, USB-UART interfaces, and Raspberry Pi host connectivity.
2. **Build Integrity Check:** Run clean STM32 GCC build (`tools/build_stm32.sh`).
3. **Unit & Mock Test Suite Run:** Execute `pytest test_hmi_mock.py test_rs485_mock.py`.
4. **Physical Flash Programming:** Program STM32 Nucleo via OpenOCD / ST-LINK.
5. **Physical UART / RS485 Loopback Check:** Verify DE/RE pin switching and baudrate integrity.
6. **MCU / ESP32 Communication Boot Test:** Verify `BOOT_OK` telemetry frames.
7. **Identity & Provisioning Workflow Run:** Execute `python id_full_lifecycle_test.py`.
8. **Normal Process Execution Run:** Execute `pytest test_hil_uart.py -k test_normal_process`.
9. **Fault & Safety Interlock Run:** Execute `pytest test_hil_uart.py -k test_safety_interlocks`.
10. **Dual-Frequency Selection Run:** Execute `pytest test_hil_uart.py -k test_frequency_control`.
11. **Frequency Sweep Modulation Run:** Execute `pytest test_hil_uart.py -k test_sweep_modulation`.
12. **Degas Pulsed Cavitation Run:** Execute `pytest test_hil_uart.py -k test_degas_cycle`.
13. **Multi-Tank Isolation Run:** Execute `pytest test_rs485_mock.py -k test_multi_tank`.
14. **Decommissioning & Reset Run:** Execute `pytest test_hil_uart.py -k test_id_reset`.
15. **Full Regression Suite Run:** Execute `pytest test_hil_uart.py test_hmi_mock.py test_rs485_mock.py`.
16. **Baseline Restoration:** Restore default persistent configuration parameters.
17. **Test Log Archival:** Preserve clean pytest log output in `docs/` or `logs/`.

---

## 11. Test Entry & Exit Criteria

### Entry Criteria:
* STM32 firmware binary compiled cleanly without warnings.
* ESP32 firmware programmed cleanly on master node.
* Nextion HMI `.tft` file flashed to display.
* Hardware wiring verified against `hardware_wiring_FINAL_AUTHORITY.md`.
* Test harness cabling and USB-UART serial ports verified.

### Exit Criteria:
* 100% of planned executable tests (Level 1, 2, 3) exhibit `PASS` status.
* Zero unexplained test failures or system crashes.
* All `DEFERRED` items explicitly logged with missing hardware justification.
* Persistent Tank ID and recipe parameters restored to baseline defaults.
* Complete test execution log output archived.

---

## 12. Deferred Final Hardware Tests

The following 2 physical qualification steps are explicitly documented as **DEFERRED TO PHASE 6 COMMISSIONING** due to missing physical hardware:

1. **Acoustic Transducer Frequency Sweep Power Distribution (`SWP-FREQ-SWEEP` Level 4):**  
   Requires high-power ultrasonic driver card, 28kHz/40kHz acoustic transducer motor, hydrophone, and acoustic tank. (Level 3 X9C pot voltage ladder is verified).
2. **Liquid Tank Cavitation Degas Bubble Removal (`DEG-PULSE-DEGAS` Level 4):**  
   Requires physical liquid tank, ultrasonic transducer, and dissolved oxygen (DO) PPM sensor meter. (Level 3 gated PWM burst modulation is verified).

---

## 13. Ambiguities & Human Engineering Decisions

The following 3 items require human engineering decision alignment prior to Phase 6 physical hardware commissioning:

1. **Physical Transducer Impedance Matching:** Alignment on whether X9C103S wiper step voltage calibration curves match the physical transducer's resonance bandwidth under full liquid load.
2. **Liquid Degas Cavitation Burst Timing:** Alignment on whether prototype 1000ms ON / 500ms OFF burst timing requires fluid-specific tuning based on tank liquid viscosity.
3. **Module Requirement Document Standardization:** Alignment on whether to generate standalone requirement `.md` files for 6 basic driver modules (`x9c103s.c`, `process_timer.c`) or retain them under master system manifestos.
