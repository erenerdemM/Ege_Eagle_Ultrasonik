# EAGLEULTRASONİK — FREQUENCY SWEEP / SHIFTING REQUIREMENTS DOCUMENT (C-FAZ 1 BASELINE FREEZE)

**Document ID:** `docs/C_SWEEP_REQUIREMENTS.md`  
**Project:** EAGLEULTRASONİK  
**Phase:** C-Faz 1 Baseline Requirements Capture & Freeze (Parametric Sweep Step Update)  
**Target Hardware:** STM32G474RETx Master/Slave Nodes, ESP32-S3 Master, Nextion HMI, X9C103S Digital Potentiometer  
**Status:** FROZEN BASELINE (PROTOTYPE VERIFIED)  
**Date:** 2026-08-16  

---

## 1. PURPOSE AND SCOPE

### 1.1 Purpose
This document formally extracts, defines, and freezes the Frequency Sweep (Shifting) requirements for the EAGLEULTRASONİK system under C-Faz 1. The objective of C-Faz 1 is to freeze the validated prototype baseline into a strict requirements specification without modifying existing firmware or HMI codebase, clarifying system behavior through repository evidence and identifying open design decisions prior to C-Faz 2 implementation.

### 1.2 Scope
The scope of this document covers the end-to-end frequency sweep architecture, including:
- STM32G474RE HAL firmware (`x9c103s.c`, `x9c103s.h`, `system_state.c`, `esp32_uart.c`, `main.c`)
- ESP32-S3 & Nextion HMI integration (`esp32/ekran_kontrol/ekran_kontrol.ino`)
- RS485 / ASCII UART command bus interface (`T<ID>:SWEEP:ON`, `T<ID>:SWEEP:OFF`, `T<ID>:SET_FREQ:<freq>`)
- X9C103S digital potentiometer driver dynamics and hardware electrical interface
- System safety interlocks, mode transitions, fault handling, and timer expirations
- Parametric sweep step modeling (`BASE_STEP_28`, `BASE_STEP_40`, `STEP_INCREMENT`) and Service Settings ownership
- Identification and formal tracking of unresolved architectural design choices for C-Faz 2

---

## 2. CURRENT PROTOTYPE BASELINE

### 2.1 Hardware Baseline
- **Microcontroller:** STM32G474RETx operating at 170 MHz (168/170 MHz PLL core clock).
- **Actuator:** Renesas X9C103S 10kΩ Digital Potentiometer (100 wiper steps, 0..99).
- **Control Interface (3-Wire SPI-like Bit-Bang):**
  - Chip Select ($\bar{\text{CS}}$): GPIO `PB12` (`X9C_CS_Pin`)
  - Up/Down Direction ($U/\bar{D}$): GPIO `PB13` (`X9C_UD_Pin`)
  - Increment Pulse ($\bar{\text{INC}}$): GPIO `PB14` (`X9C_INC_Pin`)
- **Analog Feedback / Verification:**
  - Wiper Terminal ($V_W$): Pin 5 connected to STM32 `PA0` (ADC1_IN1) through a 1 kΩ series protection resistor.
  - High Terminal ($V_H$): Pin 3 connected to Nucleo 3.3 V rail ($V_H \approx 3.31\text{ V}$).
  - Low Terminal ($V_L$): Pin 6 connected to Common Logic GND Bus ($V_L \approx 0.04\text{ V}$).
  - Supply ($V_{CC}$): Pin 8 connected to Nucleo 5 V rail ($V_{CC} = 5.0\text{ V}$).
- **Measured Hardware Baseline:**
  - Wiper voltage span: $\sim 0.05\text{ V} \dots 3.31\text{ V}$ across 0..99 wiper steps.
  - Measured total pot resistance ($R_{total}$): $9.45\text{ k}\Omega$.

### 2.2 Functional & Parametric Baseline
- **Dual Center Base Steps:** 
  - 28 kHz Base Step (`BASE_STEP_28`): Step 40 ($\sim 4.0\text{ k}\Omega$, $V_W \approx 1.32\text{ V}$).
  - 40 kHz Base Step (`BASE_STEP_40`): Step 90 ($\sim 9.0\text{ k}\Omega$, $V_W \approx 2.97\text{ V}$).
- **Parametric Sweep Step Increment (`STEP_INCREMENT`):**
  - Configurable Service Settings parameter.
  - Allowed Integer Range: `1, 2, 3, 4, 5, 6, 7, 8`.
  - Current / Default Prototype Value: `4`.
- **Parametric Offset Calculation:**
  - Offset Multipliers: `OFFSET_MULTIPLIERS = [-2, -1, 0, +1, +2, +1, 0, -1, -2]`.
  - Formula: $\text{target\_step} = \text{BASE\_STEP} + (\text{multiplier} \times \text{STEP\_INCREMENT})$.
- **28 kHz Prototype Sweep Profile (`STEP_INCREMENT = 4`):**
  - Offsets: $-8, -4, 0, +4, +8, +4, 0, -4, -8$ steps around `BASE_STEP_28 = 40`.
  - Wiper Steps: $32 \rightarrow 36 \rightarrow 40 \rightarrow 44 \rightarrow 48 \rightarrow 44 \rightarrow 40 \rightarrow 36 \rightarrow 32$.
  - Frequencies: $26 \rightarrow 27 \rightarrow 28 \rightarrow 29 \rightarrow 30 \rightarrow 29 \rightarrow 28 \rightarrow 27 \rightarrow 26\text{ kHz}$.
- **40 kHz Prototype Sweep Profile (`STEP_INCREMENT = 4`):**
  - Offsets: $-8, -4, 0, +4, +8, +4, 0, -4, -8$ steps around `BASE_STEP_40 = 90`.
  - Wiper Steps: $82 \rightarrow 86 \rightarrow 90 \rightarrow 94 \rightarrow 98 \rightarrow 94 \rightarrow 90 \rightarrow 86 \rightarrow 82$.
  - Frequencies: $38 \rightarrow 39 \rightarrow 40 \rightarrow 41 \rightarrow 42 \rightarrow 41 \rightarrow 40 \rightarrow 39 \rightarrow 38\text{ kHz}$.
- **Point Interval:** 50 ms per frequency transition.
- **Cycle Period:** 400 ms complete cyclic triangle period (8 transitions × 50 ms).
- **Execution Mechanism:** Non-blocking state machine (`X9C103S_SweepProcess()`) polled in main superloop.
- **Mode Requirement:** Sweep operation strictly requires active running state (`SYS_MODE_RUNNING`).

---

## 3. FREQUENCY MODES

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-001** | The system shall support two distinct frequency operational modes: Static Frequency Mode and Dynamic Frequency Sweep Mode. | `x9c103s.h:40`, `esp32_uart.c:194` | Code Inspection & HIL Pytest | **VERIFIED** |
| **SWP-REQ-002** | In Static Frequency Mode, the X9C wiper shall remain fixed at the discrete wiper step corresponding to the selected base center frequency (`BASE_STEP_28 = 40` or `BASE_STEP_40 = 90`). | `x9c103s.c:190` | ADC Telemetry & Oscilloscope | **VERIFIED** |
| **SWP-REQ-003** | In Dynamic Frequency Sweep Mode, the X9C wiper shall continuously transition through a parametric array of step offsets calculated from `BASE_STEP + (multiplier * STEP_INCREMENT)` around the active center frequency. | `x9c103s.c:258` | HIL Telemetry / PA0 ADC | **VERIFIED** |
| **SWP-REQ-004** | Fine-grained 1-step sweep capability is supported parametrically by configuring `STEP_INCREMENT = 1`. However, physical validation with a real transducer motor remains an open item for future validation. | Parametric Model Directive / `x9c103s.c:26` | Requirements Freeze Audit | **DEFINED** |

---

## 4. CENTER FREQUENCY REQUIREMENTS

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-005** | The system shall support two discrete center frequencies: 28 kHz (default at system boot, `BASE_STEP_28 = 40`) and 40 kHz (`BASE_STEP_40 = 90`). | `x9c103s.h:18-19`, `x9c103s.c:11` | HIL Test (`test_f1`, `test_f2`) | **VERIFIED** |
| **SWP-REQ-006** | Center frequency selection shall be requested via the ASCII UART command `T<ID>:SET_FREQ:<freq>`, where `<freq>` must be 28 or 40. | `esp32_uart.c:419` | RS485 Protocol HIL Test | **VERIFIED** |
| **SWP-REQ-007** | When center frequency 28 kHz is selected while sweep is disabled, the X9C potentiometer shall immediately move to `BASE_STEP_28 = 40` (measured $V_W \approx 1.32\text{ V}$). | `x9c103s.c:194`, `HARDWARE-SOFTWARE-MAP.md` | PA0 Multimeter / ADC | **VERIFIED** |
| **SWP-REQ-008** | When center frequency 40 kHz is selected while sweep is disabled, the X9C potentiometer shall immediately move to `BASE_STEP_40 = 90` (measured $V_W \approx 2.97\text{ V}$). | `x9c103s.c:199`, `HARDWARE-SOFTWARE-MAP.md` | PA0 Multimeter / ADC | **VERIFIED** |
| **SWP-REQ-009** | If a frequency change command (`SET_FREQ`) is received while sweep is active, the behavior (re-centering sweep vs disabling sweep) requires explicit C-Faz 2 design freeze. | `x9c103s.c:215` (Currently captures center at sweep enable) | System Architecture Audit | **OPEN** |

---

## 5. SWEEP RANGE REQUIREMENTS

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-010** | For 28 kHz center frequency under default prototype increment (`STEP_INCREMENT = 4`), the sweep range shall span $\pm 2\text{ kHz}$ (spanning from 26 kHz / Step 32 to 30 kHz / Step 48). | `x9c103s.c:17`, `esp32_uart.c:205` | HIL Pytest & PA0 ADC | **VERIFIED** |
| **SWP-REQ-011** | For 40 kHz center frequency under default prototype increment (`STEP_INCREMENT = 4`), the sweep range shall span $\pm 2\text{ kHz}$ (spanning from 38 kHz / Step 82 to 42 kHz / Step 98). | `x9c103s.c:26-29`, `x9c103s.c:63-78` | Code Inspection & ADC Analysis | **VERIFIED** |
| **SWP-REQ-012** | Absolute minimum evaluated wiper step across all sweep configurations shall be Step 0. Absolute maximum evaluated wiper step shall be Step 99. | `x9c103s.c:45-78` | Wiper Step Limits Verification | **VERIFIED** |
| **SWP-REQ-013** | Dynamic or user-configurable sweep span (e.g., modifying `STEP_INCREMENT` between `1..8`) is supported as a Service Settings parameter and remains open for future operational tuning. | Service Architecture | Requirements Freeze Audit | **DEFINED** |

---

## 6. STEP / FREQUENCY MAPPING

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-014** | Frequency-to-wiper-step conversion shall use the piecewise linear interpolation algorithm `X9C103S_SweepStepForFrequency()`. | `x9c103s.c:36-79` | Code Inspection & Math Verification | **VERIFIED** |
| **SWP-REQ-015** | For frequencies $\le 28\text{ kHz}$, step mapping shall strictly conform to: $\text{step} = 40 + \left\lfloor \frac{(\text{freq\_khz} - 28) \times 50}{12} \right\rfloor$. | `x9c103s.c:47-48` | Code Inspection & Math Audit | **VERIFIED** |
| **SWP-REQ-016** | For 28 kHz sweep, wiper steps shall be calculated parametrically as $\text{BASE\_STEP\_28} + (\text{multiplier} \times \text{STEP\_INCREMENT})$. For default `STEP_INCREMENT = 4`, discrete steps evaluate to 32, 36, 40, 44, 48. | `x9c103s.c:47-75` | PA0 ADC Voltage Step Logging | **VERIFIED** |
| **SWP-REQ-017** | For 40 kHz sweep, wiper steps shall be calculated parametrically as $\text{BASE\_STEP\_40} + (\text{multiplier} \times \text{STEP\_INCREMENT})$. For default `STEP_INCREMENT = 4`, discrete steps evaluate to 82, 86, 90, 94, 98. | `x9c103s.c:63-78` | Code Calculation Audit | **VERIFIED** |
| **SWP-REQ-018** | Wiper steps shall be clamped between 0 and 99. Calculated steps outside 0..99 shall be saturated at 0 or 99. | `x9c103s.c:50-59`, `x9c103s.c:67-75` | Boundary Code Inspection | **VERIFIED** |

---

## 7. SWEEP TIMING / RATE

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-019** | The sweep transition interval (`X9C_SWEEP_POINT_MS`) shall be exactly 50 ms per frequency offset point. | `x9c103s.c:23` | System Timer HIL Log | **VERIFIED** |
| **SWP-REQ-020** | The full cyclic sweep period (`X9C_SWEEP_PERIOD_MS`) shall be 400 ms, corresponding to 8 point transitions per full cycle. | `x9c103s.c:22` | HIL Pytest & Scope Timing | **VERIFIED** |
| **SWP-REQ-021** | Sweep timing shall be non-blocking, managed via periodic timestamp comparison (`HAL_GetTick() - s_sweep_last_tick >= 50ms`) within the main loop. | `x9c103s.c:269` | Main Loop Latency Audit | **VERIFIED** |
| **SWP-REQ-022** | User-configurable sweep rate / period adjustments via RS485 command (e.g., `SWEEP:PERIOD=200`) are not present in baseline and remain open. | Baseline Analysis | Protocol Architecture Audit | **OPEN** |

---

## 8. SWEEP DIRECTION AND CYCLE BEHAVIOR

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-023** | The sweep sequence shall follow a symmetric continuous triangle cycle pattern using multipliers: $[-2, -1, 0, +1, +2, +1, 0, -1, -2]$. | `x9c103s.c:26-29` | Offset Array Inspection | **VERIFIED** |
| **SWP-REQ-024** | Upon enabling sweep (`SWEEP:ON`), the sweep engine shall immediately force the wiper to the lower frequency endpoint (index 0, offset $-2 \times \text{STEP\_INCREMENT}$) before initiating timing cycles. | `x9c103s.c:231-240` | Immediate Step Log Verification | **VERIFIED** |
| **SWP-REQ-025** | The sweep engine index shall automatically roll over from index 8 back to index 0 upon completing each triangle cycle. | `x9c103s.c:287-289` | Continuous Operation HIL Test | **VERIFIED** |
| **SWP-REQ-026** | Support for alternative sweep waveforms (sawtooth up, sawtooth down, random frequency hopping) remains unallocated and open. | Baseline Analysis | Design Freeze Audit | **OPEN** |

---

## 9. START / STOP BEHAVIOR

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-027** | Sweep activation (`SWEEP:ON`) while system is in `SYS_MODE_IDLE` or `SYS_MODE_FAULT` shall be rejected with `ERR:SWEEP_REQUIRES_RUNNING`. | `esp32_uart.c:196-201` | HIL Integration Pytest | **VERIFIED** |
| **SWP-REQ-028** | Sweep activation (`SWEEP:ON`) while system is in `SYS_MODE_RUNNING` shall be accepted, setting sweep enabled and responding with `ACK:SWEEP:ON,PERIOD_MS=400,SPAN=+-2KHZ`. | `esp32_uart.c:203-208` | RS485 ASCII Bus Test | **VERIFIED** |
| **SWP-REQ-029** | Receiving explicit `SWEEP:OFF` command shall disable sweep, restore the exact center frequency step, and respond with `ACK:SWEEP:OFF,CENTER_RESTORED`. | `esp32_uart.c:211-218` | RS485 ASCII Bus Test | **VERIFIED** |
| **SWP-REQ-030** | Receiving system `STOP` command (user stop) shall trigger `SystemState_SafeStop(STOP_REASON_USER_STOP)`, which immediately turns off sweep and restores center frequency. | `system_state.c:95`, `esp32_uart.c:276` | System SafeStop Audit | **VERIFIED** |
| **SWP-REQ-031** | If system receives `START` command while sweep was previously enabled (or pre-flagged on HMI), whether sweep auto-activates or requires explicit `SWEEP:ON` must be frozen for C-Faz 2. | `esp32_uart.c:269` | Protocol Matrix Audit | **OPEN** |

---

## 10. TIMER BEHAVIOR

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-032** | When the washing process timer reaches 00:00 (`STOP_REASON_TIMER_ZERO`), `SystemState_SafeStop()` shall execute immediately. | `process_timer.c`, `system_state.c:106` | Timer Countdown HIL Test | **VERIFIED** |
| **SWP-REQ-033** | Upon process timer expiration, sweep shall be automatically disabled (`s_sweep_enabled = 0`) and the active center frequency wiper position restored. | `system_state.c:95`, `x9c103s.c:248` | HIL Telemetry Verification | **VERIFIED** |

---

## 11. FAULT / SAFE STOP BEHAVIOR

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-034** | Any system fault transition to `SYS_MODE_FAULT` (general fault, sensor fault, watchdog reset) shall invoke `SystemState_SafeStop()`. | `system_state.c:92-135` | Fault Injection HIL Suite | **VERIFIED** |
| **SWP-REQ-035** | `SystemState_SafeStop()` shall enforce priority 1 deactivation of frequency sweep (`X9C103S_SetSweepEnabled(0U)`) prior to cutting heater and triac outputs. | `system_state.c:95` | SafeStop Code Order Audit | **VERIFIED** |
| **SWP-REQ-036** | Upon fault execution, the X9C pot wiper shall instantly restore the selected center frequency step, ensuring the transducer generator returns to a stable static baseline. | `x9c103s.c:249` | Oscilloscope / ADC Trace | **VERIFIED** |

---

## 12. COMMUNICATION LOSS BEHAVIOR

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-037** | If no valid RS485 command is received for $> 3000\text{ ms}` while in `SYS_MODE_RUNNING`, RX timeout watchdog shall trigger `SystemState_SafeStop(STOP_REASON_COMM_TIMEOUT)`. | `esp32_uart.c:108-112` | RS485 Disconnect HIL Test | **VERIFIED** |
| **SWP-REQ-038** | Communication loss safe-stop shall disable frequency sweep and restore center frequency position. | `system_state.c:95`, `system_state.c:116` | Bus Disconnect HIL Test | **VERIFIED** |

---

## 13. POWER INTERACTION

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-039** | Ultrasonic power setpoint adjustments (`T<ID>:SET_POWER:<pct>`) shall operate completely independently of active frequency sweep. | `esp32_uart.c:253`, `POWER-VS-FREQUENCY-CONTROL.md` | Independent Axis HIL Test | **VERIFIED** |
| **SWP-REQ-040** | Changing output power percentage while sweep is active shall be permitted without interrupting or resetting the sweep state machine. | `esp32_uart.c:253` | HIL Concurrent Test | **VERIFIED** |
| **SWP-REQ-041** | Triac phase-angle soft-start and power regulation shall operate on Control Axis A, strictly decoupled from X9C pot stepping on Control Axis B. | `PHASE-4.5-REV2-DESIGN-FREEZE-CHALLENGE.md` | Architectural Audit | **VERIFIED** |

---

## 14. RECIPE INTERACTION

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-042** | ESP32 NVS recipe storage currently persists time, temperature, and power parameters (`p_sure`, `p_sicaklik`, `guc_seviyesi`). | `esp32/ekran_kontrol/ekran_kontrol.ino:561` | NVS Code Audit | **VERIFIED** |
| **SWP-REQ-043** | Whether NVS recipe schema should be extended to store default sweep state (Sweep ON/OFF) per recipe remains an open design decision for C-Faz 2. | Recipe Architecture | System Design Audit | **OPEN** |

---

## 15. DEGAS INTERACTION

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-044** | Degas pulse timing and interaction with X9C frequency sweep is not present in the current baseline and remains an open future requirement. | System Architecture | Requirements Audit | **OPEN** |

---

## 16. ESP32 / HMI REQUIREMENTS

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-045** | The ESP32 master shall parse HMI commands `CMD_SWEEP_ON` / `CMD_SWEEP\|ON` and transmit `T<secili_goz>:SWEEP:ON\n` over RS485. | `ekran_kontrol.ino:424, 664` | Serial HMI Intercept Test | **VERIFIED** |
| **SWP-REQ-046** | The ESP32 master shall parse HMI commands `CMD_SWEEP_OFF` / `CMD_SWEEP\|OFF` and transmit `T<secili_goz>:SWEEP:OFF\n` over RS485. | `ekran_kontrol.ino:424, 672` | Serial HMI Intercept Test | **VERIFIED** |
| **SWP-REQ-047** | The ESP32 shall check local running state (`makine_calisiyor[secili_goz]`) before issuing `SWEEP:ON`. If false, command is rejected locally. | `ekran_kontrol.ino:665-667` | HMI Mock Test | **VERIFIED** |
| **SWP-REQ-048** | Integration of dedicated visual sweep status indicator / pulsing icon on Nextion HMI screen requires UI design freeze in C-Faz 2. | Nextion TFT Layout | HMI UI Audit | **OPEN** |

---

## 17. STM32 REQUIREMENTS

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-049** | STM32 main superloop shall execute `X9C103S_SweepProcess()` on every iteration without blocking zero-cross, ADC, or UART handling. | `main.c:460` | Superloop Latency Audit | **VERIFIED** |
| **SWP-REQ-050** | `X9C103S_SetStep()` shall use micro critical sections (`__disable_irq()` $< 10\ \mu\text{s}$ per step pulse) to eliminate global interrupt blackout and prevent ZC phase drift or UART overrun errors. | `x9c103s.c:169-180`, `verified-findings.json` | Oscilloscope & HIL Test | **VERIFIED** |
| **SWP-REQ-051** | PA0 ADC feedback shall continuously track $V_W$ wiper voltage to monitor potentiometer physical integrity. | `main.c:568`, `pt100_adc.c` | ADC Telemetry Verification | **VERIFIED** |

---

## 18. RS485 / COMMAND REQUIREMENTS

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-052** | Protocol matrix shall support line-terminated ASCII commands: `T<ID>:SWEEP:ON` and `T<ID>:SWEEP:OFF`. | `esp32_uart.c:194, 211` | RS485 Protocol Test | **VERIFIED** |
| **SWP-REQ-053** | `T<ID>:SWEEP:ON` response shall be: `ACK:SWEEP:ON,PERIOD_MS=400,SPAN=+-2KHZ\n`. | `esp32_uart.c:205` | Bus Response Logging | **VERIFIED** |
| **SWP-REQ-054** | `T<ID>:SWEEP:OFF` response shall be: `ACK:SWEEP:OFF,CENTER_RESTORED\n`. | `esp32_uart.c:215` | Bus Response Logging | **VERIFIED** |
| **SWP-REQ-055** | `SWEEP:ON` while not in `SYS_MODE_RUNNING` shall yield: `ERR:SWEEP_REQUIRES_RUNNING\n`. | `esp32_uart.c:198` | Bus Response Logging | **VERIFIED** |
| **SWP-REQ-056** | Broadening STAT telemetry packet to explicitly include `sweep_active` bit (currently `STAT` reports `frequency_khz` center) is open for C-Faz 2 protocol freeze. | `esp32_uart.c:488` | Protocol Telegram Audit | **OPEN** |
| **SWP-REQ-057** | Additional RS485 diagnostic / tuning commands (e.g. `T<ID>:SWEEP:SET_SPAN`) are open and unallocated. | Baseline Analysis | Protocol Architecture Audit | **OPEN** |

---

## 19. X9C103S REQUIREMENTS

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-058** | Pin assignments shall be strictly preserved: CS → PB12, U/D → PB13, INC → PB14. | `hardware_wiring_FINAL_AUTHORITY.md:78-85` | Hardware Netlist Audit | **VERIFIED** |
| **SWP-REQ-059** | Pulse timing delays (`X9C_DelayUs(3U)`) shall fulfill datasheet minimums ($t_{INC} \ge 1\ \mu\text{s}$, $t_{CI} \ge 100\text{ ns}$, $t_{ID} \ge 2.9\ \mu\text{s}$, $t_{CPH} \ge 10\ \mu\text{s}$). | `x9c103s.c:86-93`, Datasheet FN8158 | Timing Analysis | **VERIFIED** |
| **SWP-REQ-060** | Wiper position initialization (`X9C103S_Init()`) shall issue 100 DOWN pulses to force pot wiper to step 0, followed by setting step 40 (28 kHz). | `x9c103s.c:95-130` | Power-On ADC Readback | **VERIFIED** |

---

## 20. BOUNDARY CONDITIONS

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-061** | Wiper step requests outside 0..99 shall be clamped to boundary values (0 or 99). | `x9c103s.c:136-139` | Code Inspection | **VERIFIED** |
| **SWP-REQ-062** | Frequency sweep state (`s_sweep_enabled`, `s_sweep_index`) is stored strictly in RAM and shall NOT survive system power reset or MCU restart. | `x9c103s.c:31-34` | Boot Sequence Audit | **VERIFIED** |
| **SWP-REQ-063** | Upon power-on or MCU reset, the system shall default to static 28 kHz mode (sweep OFF, Step 40). | `main.c:449`, `x9c103s.c:129` | Reset Telemetry Test | **VERIFIED** |

---

## 21. INVALID COMMAND / ERROR BEHAVIOR & SERVICE PARAMETERIZATION

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-064** | Setting an unsupported center frequency (e.g. `SET_FREQ:35`) shall transmit `ERR:INVALID_FREQ\n` and retain the active center frequency without step change. | `esp32_uart.c:435`, `test_hil_uart.py:553` | HIL Test (`test_f3`) | **VERIFIED** |
| **SWP-REQ-065** | Sending malformed or corrupt sweep commands (e.g. `SWEEP:INVALID`) shall be silently discarded without altering state or blocking UART processing. | `esp32_uart.c:463` | Malformed Packet HIL Test | **VERIFIED** |
| **SWP-REQ-070** | `STEP_INCREMENT` shall be a password-protected Service Settings parameter with an allowed integer range of `1..8` and default prototype value `4`. Operators shall have NO access to edit `STEP_INCREMENT`. | Parametric Model Directive / Service Architecture | Service Menu HIL Audit | **DEFINED** |
| **SWP-REQ-071** | `STEP_INCREMENT` modifications shall be strictly forbidden while the system is in active `SYS_MODE_RUNNING` state to prevent transient wiper step jumps. | System Safety Interlock | Interlock Test Suite | **DEFINED** |

---

## 22. PROTOTYPE-LEVEL ACCEPTANCE CRITERIA

| Requirement ID | Requirement Description | Rationale / Source | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| **SWP-REQ-066** | Complete baseline source code compiles cleanly with zero errors/warnings. | Phase Baseline | Firmware Build | **VERIFIED** |
| **SWP-REQ-067** | Real hardware PA0 ADC voltage correctly maps to step 40 (28 kHz, ~1.32 V) and step 90 (40 kHz, ~2.97 V). | Physical Bench Test | Multimeter & PA0 ADC | **VERIFIED** |
| **SWP-REQ-068** | Pytest HIL integration test suite (`test_hil_uart.py`) passes 100% of tests including dual frequency and protocol checks. | `PROJECT_STATE.md:38` | Pytest Execution | **VERIFIED** |
| **SWP-REQ-069** | RS485 communication timeouts (3000 ms) reliably trigger SafeStop and disable active sweep. | HIL Test Suite | Pytest Execution | **VERIFIED** |

---

## 23. OPEN DECISIONS / UNRESOLVED REQUIREMENTS

The following items represent architectural design choices that are not implemented or explicitly constrained in the current prototype baseline. They are formally marked as **OPEN** and must be resolved prior to C-Faz 2 firmware development:

| Item | Description | Current Status / Baseline Behavior | Required Action before C-Faz 2 |
| :--- | :--- | :--- | :--- |
| **OD-SWP-01** | **Fine-Sweep 1-Step Physical Validation** | `STEP_INCREMENT` supports configuration value `1` (`32 → 33 → ... → 48`). Physical acoustic validation with real transducer motor is an open future item. | Perform transducer acoustics test to evaluate fine-step benefits. |
| **OD-SWP-02** | **Center Frequency Change During Active Sweep** | If `SET_FREQ:40` is received while sweeping at 28 kHz, center frequency variable updates but sweep center captured at enable remains. | Define policy: Re-center active sweep instantly, or reject `SET_FREQ` while sweep is ON. |
| **OD-SWP-03** | **START Command Sweep State Re-Activation** | If sweep was enabled prior to STOP, issuing START does not automatically re-enable sweep. | Define policy: Require explicit `SWEEP:ON` after START, or remember sweep enable flag across START/STOP cycles. |
| **OD-SWP-04** | **Dynamic Sweep Span / Rate Control Commands** | Span is fixed at $\pm 2\text{ kHz}$ and period at 400 ms. No ASCII commands exist to adjust span/rate. | Decide if dynamic parameters (`SWEEP:SPAN=`, `SWEEP:RATE=`) are required for production HMI. |
| **OD-SWP-05** | **HMI Sweep Status Observability** | Nextion HMI screen currently lacks dynamic visual indicator for active sweep state. | Specify HMI widget and telemetry field for sweep status display. |
| **OD-SWP-06** | **Telemetry `STAT` Packet Expansion** | `STAT` telegram contains `frequency_khz` (center freq), but no dedicated `sweep_active` bit flag. | Decide whether to append `sweep_active` flag to STAT telegram format. |
| **OD-SWP-07** | **Recipe Schema Sweep Parameter Storage** | NVS recipe storage saves time, temperature, and power, but not frequency center or sweep enable state. | Determine if recipe struct should include target frequency (28/40 kHz) and sweep mode. |
| **OD-SWP-08** | **Degas & Sweep Coexistence** | Future Degas mode pulsation logic is unmodeled in relation to frequency sweep. | Define precedence rules between Degas pulsing and Frequency Sweep shifting. |

---

## 24. SUMMARY MATRIX OF REQUIREMENTS

- **Total Requirements Captured:** 71 (SWP-REQ-001 through SWP-REQ-071)
- **VERIFIED Requirements:** 58 (Verified against physical hardware baseline, HAL firmware, and HIL test suite)
- **DEFINED Requirements:** 3 (Formally frozen scope baseline & Service `STEP_INCREMENT` constraints)
- **OPEN Requirements:** 10 (Architectural decisions reserved for C-Faz 2 alignment)

---
*End of Document `docs/C_SWEEP_REQUIREMENTS.md`*
