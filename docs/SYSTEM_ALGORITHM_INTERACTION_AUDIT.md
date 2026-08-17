# EAGLEULTRASONİK — COMPLETE SYSTEM ALGORITHM, FUNCTION INTERACTION, HIERARCHY, COMMUNICATION, AND RISK AUDIT REPORT

---

## 1. Executive Summary

This document presents the authoritative master synthesis for the EAGLEULTRASONiK System Algorithm, Function Interaction, Hierarchy, Communication, and Risk Audit.

The audit evaluated all **47 System Functions** identified in [`docs/SYSTEM_FUNCTION_INVENTORY.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/SYSTEM_FUNCTION_INVENTORY.md) across STM32G474RE HAL firmware, ESP32-S3 Master controller code, Nextion HMI display UART interfaces, multi-drop RS485 communication framing, hardware wiring, and Pytest QA suites.

### Key Audit Metrics:
* **Audit Methodology:** 100% Read-Only Autonomous Multi-Agent Workstream Delegation (`system-architect`, `stm32-specialist`, `esp32-hmi-specialist`, `communication-specialist`, `hardware-engineer`, `qa-test-engineer`, `code-reviewer`).
* **Child Subagents Invoked:** **7**
* **Child Results Received:** **7**
* **System Functions Audited:** **47 / 47 (100%)**
* **Total System Risks Identified:** **15** (2 Critical, 5 High, 6 Medium, 2 Low).
* **Executable Test Pass Rate:** **109 / 109 (100%)** (1 test `test_17` deferred under `DR-001`).
* **Code Integrity:** **0 Source Code Files / Test Scripts Modified.**

### Final Classification:
```text
SYSTEM ALGORITHM AUDIT — CRITICAL FINDINGS
```

---

## 2. 47-Function System Inventory Analysis

All 47 system functions cataloged in [`docs/SYSTEM_FUNCTION_INVENTORY.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/SYSTEM_FUNCTION_INVENTORY.md) were audited across inputs, outputs, preconditions, state ownership, failure behaviors, and recovery mechanisms:

### System & State Architecture Functions (1–4)
1. **`SYS-BOOT`**: System Reset & Initialization Vector. Loads Flash Page 127 identity, configures 170MHz clock, starts IWDG.
2. **`SYS-STATE`**: Mode & Provisioning State Machine (`SYS_MODE_IDLE`, `RUNNING`, `FAULT`, `DEGAS`; `PROV_STATE_UNCOMMISSIONED`, `STAGING`, `ACTIVE`).
3. **`SYS-SAFESTOP`**: Master Emergency Hardware Disarm. Cuts PC6 Triac gate, opens PB15 relay, resets softstart delay to 9500 µs.
4. **`SYS-FAULT`**: Hardware & Comms Fault Aggregation (`fault_flags` bitmask 0x01..0x40).

### STM32 Driver & Peripheral Functions (5–12)
5. **`STM-TIM15-PWM`**: TIM15 PWM Generation. 170MHz clock, prescaler 169 (1µs resolution).
6. **`STM-TRIAC-PHASE`**: EXTI7 Zero-Cross Firing & Soft-Start Ramping (20 µs/step).
7. **`STM-ZERO-CROSS`**: PC7 Mains EXTI Edge Detector. 500ms loss timeout.
8. **`STM-PT100-ADC`**: OPAMP3 (Gain=2) + ADC2 RTD Signal Processing (\(\text{temp} = \text{adc} \times 0.0327 - 20\)).
9. **`STM-HEATER-RELAY`**: PB15 Relay Bang-Bang Temperature Control (\(\pm 1.0^\circ\text{C}\) hysteresis, 10s ON/OFF guard timer).
10. **`STM-TIMER-DOWN`**: Non-Blocking 1 Hz Process Countdown Timer.
11. **`STM-X9C103S`**: Bit-Bang Potentiometer Driver (PB12 CS, PB13 U/D, PB14 INC).
12. **`SYS-WATCHDOG-HW`**: STM32 Hardware IWDG Watchdog (1000 ms timeout).

### Frequency Sweep Subsystem Functions (13–16)
13. **`SWP-FREQ-SWEEP`**: Triangle Wave Modulation Engine (Steps 40–90, 28kHz/40kHz center).
14. **`SWP-SPAN-PER`**: Sweep Span & Period Control (400ms period, 9-point triangle).
15. **`SWP-STEP-RAMP`**: Wiper Step Increment Control (1, 2, 4, 8 steps/point).
16. **`SWP-MODE-LOCK`**: Mode Interlock (Sweep prohibited in DEGAS mode).

### DEGAS Subsystem Functions (17–20)
17. **`DEG-PULSE-DEGAS`**: Gated Complimentary TIM15 PWM Burst Control (1000ms ON / 500ms OFF).
18. **`DEG-ARMING`**: Two-Stage Selection & Arming Engine (`CMD_DEGAS_SEL` toggle).
19. **`DEG-SNAPSHOT`**: Atomic Parameter Snapshot Generation (`T1:START_DEGAS:...`).
20. **`DEG-INTERLOCK`**: Exclusions & HMI Lockout.

### Tank Identity & Provisioning Functions (21–27)
21. **`ID-UID-DISC`**: Silicon 96-bit Hardware UID Readout & `T0:DISCOVER` Slotted Response.
22. **`ID-STAGE`**: Temporary RAM Staging (`T0:STAGE_ID`, 10s timeout auto-rollback).
23. **`ID-ASSIGN`**: Service PIN `123456` Auth & Double-Word Flash Commit.
24. **`ID-RESET`**: Flash Page 127 Erase & Factory Uncommissioned Reset (`T0:RESET_ID`).
25. **`ID-PERSIST`**: Bank 2 Page 127 (`0x0807F800`) Read/Write & Magic Word `0xA5A5A5A5`.
26. **`ID-ROUTING`**: Multi-Drop `T<id>:` Address Filtering & Broadcast Matching.
27. **`ID-DIP-FREE`**: Production DIP-Switch-Free State Lifecycle Management.

### RS485 Communication Protocol Functions (28–33)
28. **`COM-RS485-DIR`**: DE/RE Transceiver Half-Duplex Direction Control.
29. **`COM-UART-DRIVER`**: USART3 Interrupt RX Buffer & DMA Engine.
30. **`COM-FRAME-PARSER`**: ASCII Line Parser & Colon Separator Tokenizer.
31. **`COM-TELEMETRY`**: 10-Field CSV Status Telegram Streaming (`STAT,...`).
32. **`COM-WATCHDOG`**: 3000 ms RX Silence Safety Watchdog (`RX_SILENCE_TIMEOUT_MS`).
33. **`COM-CRC16`**: 16-Bit CRC16-CCITT Polynomial Calculation.

### ESP32 Master & HMI Gateway Functions (34–40)
34. **`ESP-MASTER-LOOP`**: FreeRTOS `loopTask` Superloop & State Tracking.
35. **`ESP-NVS-RECIPE`**: Non-Volatile Recipe Storage (`"ultra"` & `"degas_cfg"` namespaces).
36. **`ESP-SVC-AUTH`**: Service Password Auth ("123456") & 300s Session Timeout.
37. **`ESP-CONN-MON`**: 3000 ms Multi-Node Connection Watchdog (`STM_BAGLANTI_TIMEOUT`).
38. **`ESP-ZERO-SIM`**: Hardware `esp_timer` GPIO4 100Hz Square Wave Generator.
39. **`HMI-NEXTION-UART`**: Nextion TTL UART2 Protocol Parser (`hatOku`).
40. **`HMI-PAGE-HOME`**: Nextion UI Home Page Render & Recipe Button Dispatch (`P1`..`P3`).

### Safety Guards & Clamping Functions (41–44)
41. **`SAF-EXCLUSION`**: Mutual Exclusion Interlock Enforcement.
42. **`SAF-COMM-OFFLINE`**: Disconnected Node Start Prevention (`baslatmaEngelliMi()`).
43. **`SAF-PARAM-CLAMP`**: Numerical Setpoint Boundary Clamping (Temp 0-90°C, Power 10-100%).
44. **`HMI-FAULT-POPUP`**: Hardware Fault Visual Modal Alert Rendering.

### QA Verification Test Suite Functions (45–47)
45. **`TST-HIL-SUITE`**: Pytest Hardware-in-the-Loop Test Runner (`test_hil_uart.py`, 30 tests).
46. **`TST-HMI-MOCK`**: Pytest HMI Emulation Mock Test Runner (`test_hmi_mock.py`, 49 tests).
47. **`TST-RS485-MOCK`**: Pytest RS485 Bus Simulation Mock Test Runner (`test_rs485_mock.py`, 31 tests).

---

## 3. Algorithm Interaction Matrix

The 47 system functions interact across 5 primary operational execution vectors:

```
[HMI Touch Panel] ──► (ESP-MASTER-LOOP) ──► (COM-RS485-DIR) ──► (COM-UART-DRIVER) ──► (SYS-STATE)
                                                                                            │
 ┌──────────────────────────────────────────────────────────────────────────────────────────┘
 ▼
(STM-TIM15-PWM) ──► (STM-TRIAC-PHASE) ──► Transducer Output
(STM-HEATER-RELAY) ──► Heater Load
(STM-PT100-ADC) ──► Safety Feedback ──► (SYS-FAULT) ──► [SafeStop Activation]
```

---

## 4. Algorithm Collision Analysis

Key collision scenarios identified during the forensic audit:

1. **`STOP` Command vs. Active Fault (Collision RSK-001)**: Sending `STOP` while PT100/ZC fault is active clears `fault_flags` unconditionally, enabling a subsequent `START` command to start the machine before the fault is re-checked.
2. **`ID-STAGE` / `ID-ASSIGN` vs. `SYS_MODE_DEGAS` (Collision RSK-006)**: Provisioning frames sent during DEGAS mode bypass STM32 line 178 check (`mode == RUNNING` only).
3. **Timer Zero vs. PT100 Fault (Collision RSK-003)**: Timer `STOP_REASON_TIMER_ZERO` overwrites `SYS_MODE_FAULT` to `SYS_MODE_IDLE` if both occur in the same superloop cycle.

---

## 5. Precedence & Dominance Hierarchy

The authoritative execution hierarchy derived from code evidence is:

$$\text{SafeStop} > \text{Fault} > \text{Running} > \text{Mode Interlocks} > \text{Process} > \text{Operator} > \text{Service} > \text{Telemetry}$$

*Full details documented in [`docs/SYSTEM_ALGORITHM_PRECEDENCE_MATRIX.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/SYSTEM_ALGORITHM_PRECEDENCE_MATRIX.md).*

---

## 6. Operator vs Internal Request Behavior

* **Operator START + Active Fault**: Rejected; UI displays fault popup; STM32 stays in `SYS_MODE_FAULT`.
* **Operator START + Comm Timeout**: Rejected by ESP32 `baslatmaEngelliMi()`; displays "Kart Yok!".
* **Operator Setpoint Change during RUNNING**: Accepted by STM32, but **bypasses ESP32 touch lockout** (RSK-003).
* **Operator SWEEP during DEGAS**: Rejected with `ERR:SWEEP_PROHIBITED_IN_DEGAS`.

---

## 7. State Machine Analysis

Transition matrix across primary modes:

| From / To Mode | `SYS_MODE_IDLE` | `SYS_MODE_RUNNING` | `SYS_MODE_DEGAS` | `SYS_MODE_FAULT` |
| :--- | :--- | :--- | :--- | :--- |
| **`SYS_MODE_IDLE`** | — | Valid (`T<id>:START`) | Valid (`T<id>:START_DEGAS`) | Sensor / ZC / Watchdog Fault |
| **`SYS_MODE_RUNNING`** | Valid (`STOP` / Timer 0) | — | Invalid (Must STOP first) | Hardware / Comm Fault Edge |
| **`SYS_MODE_DEGAS`** | Valid (`STOP` / Timer 0) | Invalid (Must STOP first) | — | Hardware / Comm Fault Edge |
| **`SYS_MODE_FAULT`** | Valid (`STOP` Fault Ack) | Invalid | Invalid | — |

---

## 8. Communication Architecture Analysis

* Multi-drop RS485 half-duplex bus (115200 8N1).
* Addressed frames `T<id>:` (1-10) and broadcast `T0:`.
* Telemetry status stream `STAT,...` (10 fields).
* **Critical Finding**: ESP32 Master possesses a response blind spot (discards slave ACK/NACK/ERR responses).

*Full details documented in [`docs/SYSTEM_HARDWARE_COMMUNICATION_AUDIT.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/SYSTEM_HARDWARE_COMMUNICATION_AUDIT.md).*

---

## 9. Hardware Communication

* STM32 PB10/PB11 UART3 + PB1 DE/RE.
* ESP32 GPIO8/GPIO18 UART1 + GPIO5 DE/RE.
* X9C103S bit-bang PB12/PB13/PB14.
* Hardware bench uses 1 kΩ series protection resistors for 3.3V TTL desktop HIL validation.

---

## 10. Configuration Ownership Matrix

| Parameter Domain | Source of Truth | Persistent Owner | Runtime Owner | Protection Mechanism |
| :--- | :--- | :--- | :--- | :--- |
| **Normal Recipes (P1..P3)** | ESP32 Flash | NVS (`"ultra"`) | ESP32 RAM / STM32 RAM | Read on boot, sent to STM32 on `START` |
| **Degas Settings** | ESP32 Flash | NVS (`"degas_cfg"`) | ESP32 RAM / STM32 RAM | Guarded by service PIN "123456" |
| **Tank Identity (ID)** | STM32 Flash | Bank 2 Page 127 | `g_system_state.tank_id` | Read on boot, `0xA5A5A5A5` magic, Mode!=RUNNING interlock |
| **Global Power Level** | ESP32 Flash | NVS (`"ultra"`) | `guc_seviyesi` / STM32 delay | Clamped 10–100% |
| **Frequency Selection** | HMI Button | NVS (`"ultra"`) | `X9C103S` Wiper Step | 28kHz (Step 40) / 40kHz (Step 90) |

---

## 11. Timing & Concurrency

* STM32 superloop + EXTI7 ZC ISR + TIM15 OC ISR + USART3 RX ISR.
* EXTI7 ISR priority 1, TIM15 OC priority 1.
* Micro-critical sections in `x9c103s.c` keep IRQ blackout <10 µs per wiper step.
* Identified risk: Struct `g_system_state` multi-word data race between superloop and EXTI ISR (RSK-011).

---

## 12. Safety & Failure Dominance

* Level 1 SafeStop dominates all hardware outputs (`HeaterRelay_ForceOff()`, `TriacForceOff()`).
* Level 2 Fault Aggregation latches `fault_flags` bitmask and enters `SYS_MODE_FAULT`.
* Comm silence >3000ms trips `STOP_REASON_COMM_TIMEOUT`.
* Zero-cross silence >500ms trips `FAULT_ZERO_CROSS_LOST`.

---

## 13. Risk Register Summary

15 total risks registered across all subsystems:
* **Critical (2)**: RSK-001 (Fault bypass on STOP), RSK-002 (Spinlock deadlock in RS485 transmit).
* **High (5)**: RSK-003 (Touch lockout omissions), RSK-004 (Master response blind spot), RSK-005 (Buffer over-read), RSK-006 (DEGAS provisioning bypass), RSK-007 (Unchecked HAL UART status).
* **Medium (6)**: RSK-008, RSK-009, RSK-010, RSK-011, RSK-012, RSK-013.
* **Low (2)**: RSK-014, RSK-015.

*Full details documented in [`docs/SYSTEM_RISK_REGISTER.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/SYSTEM_RISK_REGISTER.md).*

---

## 14. QA Review Summary

* 110 total test cases audited across 3 suites (`test_hil_uart.py`, `test_hmi_mock.py`, `test_rs485_mock.py`).
* 109 / 109 executable tests PASSED (100% executable pass rate).
* 1 test (`test_17`) cleanly deferred under `DR-001`.

*Full details documented in [`docs/SYSTEM_QA_FINAL_AUDIT.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/SYSTEM_QA_FINAL_AUDIT.md).*

---

## 15. Code Review Summary

* Safety-critical C/C++ logic audited for MISRA C:2012 compliance and FreeRTOS safety.
* Unchecked HAL return values (Rule 17.7), buffer boundaries, and ISR spinlocks flagged.

---

## 16. Specialist Disagreements

* **Zero Disagreements:** All 7 specialist workstreams reached 100% consensus on finding facts, risk severities, and precedence mapping.

---

## 17. Critical Findings Summary

1. **RSK-001**: Sending `STOP` during active hardware fault clears `fault_flags` and enables immediate `START` before re-check.
2. **RSK-002**: `RS485_Transmit_Blocking()` spinlock can deadlock if called with masked interrupts.
3. **RSK-003**: Setpoint touch edits on HMI bypass lockout during normal washing cycles.
4. **RSK-004**: ESP32 master discards slave ACK/NACK/ERR responses.

---

## 18. Human Decisions Required

* **DEC-001**: Approve updating `STOP` handler to clear `fault_flags` only when hardware fault condition is absent.
* **DEC-002**: Approve adding `makine_calisiyor` lockout check to ESP32 setpoint touch handlers.
* **DEC-003**: Approve expanding ESP32 telemetry parser to process slave ACK/NACK/ERR frames.
* **DEC-004**: Approve physical bench revalidation of `test_17` (DR-001) upon PT100 probe attachment.
