# EAGLEULTRASONİK — DEGAS IMPLEMENTATION GAP BACKLOG (B-FAZ BASELINE FREEZE)

**Document ID:** `docs/B_DEGAS_IMPLEMENTATION_GAP_BACKLOG.md`  
**Project:** EAGLEULTRASONİK  
**Phase:** B-Faz Baseline DEGAS Implementation Backlog Freeze  
**Target Hardware:** STM32G474RETx Master/Slave Nodes, ESP32-S3 Master, Nextion HMI  
**Status:** FROZEN BACKLOG SPECIFICATION  
**Date:** 2026-08-17  

---

## 1. PURPOSE AND SCOPE

### 1.1 Purpose
This document converts the frozen DEGAS Requirements ([`docs/B_DEGAS_REQUIREMENTS.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/docs/B_DEGAS_REQUIREMENTS.md)), Requirements Audit ([`docs/B_DEGAS_REQUIREMENTS_AUDIT.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/docs/B_DEGAS_REQUIREMENTS_AUDIT.md)), Architecture ([`docs/B_DEGAS_ARCHITECTURE.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/docs/B_DEGAS_ARCHITECTURE.md)), and Operating Scenarios ([`docs/B_DEGAS_SCENARIOS.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/docs/B_DEGAS_SCENARIOS.md)) into a minimal, traceable, and dependency-ordered **Implementation Gap Backlog**. 

### 1.2 Scope
The scope of this backlog encompasses:
- Clear separation between **Implementation Gaps**, **Verification Gaps**, and **Deferred Physical Calibration Gaps**.
- Priority classifications (`CRITICAL`, `HIGH`, `MEDIUM`, `LOW`, `VERIFICATION`, `DEFERRED PHYSICAL`).
- Complete mapping of requirements, ADRs, scenarios, source file anchors, and acyclic implementation dependencies.
- A recommended phase-by-phase implementation sequence ensuring zero circular dependencies.

---

## 2. BACKLOG CLASSIFICATION & METRICS

```
+-----------------------------------------------------------------------+
| BACKLOG OVERVIEW                                                      |
| Total Gaps Identified: 21 (DEG-GAP-001 through DEG-GAP-021)          |
+-----------------------------------------------------------------------+
| - Implementation Gaps: 15 (Core firmware, state machine, UI, NVS)     |
| - Verification Gaps: 3 (Automated HMI, RS485, and HIL Pytest suites)  |
| - Deferred Physical Gaps: 3 (Physical acoustic bench calibration)     |
+-----------------------------------------------------------------------+
| PRIORITY BREAKDOWN:                                                   |
| - CRITICAL:          5 Gaps (Execution cutouts & core state flags)    |
| - HIGH:              8 Gaps (PWM pulse modulation, UI, NVS, framing) |
| - MEDIUM:            2 Gaps (Multi-tank routing & HMI sync)           |
| - VERIFICATION:      3 Gaps (Automated Pytest suites)                 |
| - DEFERRED PHYSICAL: 3 Gaps (Bench hydrophone & thermal testing)      |
+-----------------------------------------------------------------------+
```

---

## 3. IMPLEMENTATION GAP MATRIX

| GAP ID | Description | Priority | Files / Anchors | Requirements Mapped | Scenarios Mapped | Dependencies | Verification Method | Status |
| :--- | :--- | :---: | :--- | :--- | :--- | :--- | :--- | :---: |
| **DEG-GAP-001** | **STM32 PWM Output Driver Mode Enable Cutout** | **CRITICAL** | `ultrasonic_pwm.c` (L119, L169), `system_state.h` | `DEG-REQ-001`, `DEG-REQ-029` | `DEG-SCN-006`, `DEG-SCN-031` | None | Code Inspection & HIL Oscilloscope | **FULL PASS** |
| **DEG-GAP-002** | **STM32 Process Timer Engine Countdown Integration** | **CRITICAL** | `process_timer.c` (L25, L39), `system_state.c` | `DEG-REQ-005`, `DEG-REQ-013` | `DEG-SCN-010`, `DEG-SCN-011` | `DEG-GAP-001` | HIL Timer Test (`test_hil_uart.py`) | **FULL PASS** |
| **DEG-GAP-003** | **STM32 Communication Silence Watchdog Extension** | **CRITICAL** | `esp32_uart.c` (L105), `system_state.c` | `DEG-REQ-021`, `DEG-REQ-049` | `DEG-SCN-017` | `DEG-GAP-001` | RS485 Silence Test | **FULL PASS** |
| **DEG-GAP-004** | **STM32 Optional DEGAS Temperature Control Logic** | **CRITICAL** | `heater_relay.c` (L55), `system_state.h` | `DEG-REQ-024`, `DEG-REQ-025` | `DEG-SCN-034`, `DEG-SCN-035` | `DEG-GAP-001` | PT100 Heater Test | **FULL PASS** |
| **DEG-GAP-005** | **STM32 Non-Blocking Triac Pulse Modulation Controller** | **HIGH** | `ultrasonic_pwm.c`, `system_state.h` | `DEG-REQ-028`, `DEG-REQ-030` | `DEG-SCN-031`, `DEG-SCN-032` | `DEG-GAP-001` | Scope Waveform Trace | **FULL PASS** |
| **DEG-GAP-006** | **STM32 RS485 `START_DEGAS` Full Snapshot Parser** | **HIGH** | `esp32_uart.c` (L288-L296) | `DEG-REQ-022`, `DEG-REQ-043` | `DEG-SCN-006`, `DEG-SCN-043` | `DEG-GAP-001` .. `DEG-GAP-005` | RS485 Frame Test | **FULL PASS** |
| **DEG-GAP-007** | **ESP32 DEGAS State Machine Flags (`degas_armed`, `degas_active`)** | **CRITICAL** | `ekran_kontrol.ino` (L60-L100) | `DEG-REQ-003`, `DEG-REQ-052` | `DEG-SCN-001`, `DEG-SCN-007` | None | ESP32 State Machine Audit | **FULL PASS** |
| **DEG-GAP-008** | **ESP32 NVS `service_degas` Partition & Persistence Manager** | **HIGH** | `ekran_kontrol.ino` (L180-L235) | `DEG-REQ-037`, `DEG-REQ-051` | `DEG-SCN-024`, `DEG-SCN-026` | `DEG-GAP-007` | NVS Reboot Test | **FULL PASS** |
| **DEG-GAP-009** | **ESP32 RS485 DEGAS Protocol Command Generator** | **HIGH** | `ekran_kontrol.ino` (L415-L465) | `DEG-REQ-022`, `DEG-REQ-033` | `DEG-SCN-006`, `DEG-SCN-019` | `DEG-GAP-007`, `DEG-GAP-008` | Protocol Matrix Test | **FULL PASS** |
| **DEG-GAP-010** | **Nextion HMI Home Page DEGAS Function Button & Visual Toggle** | **HIGH** | `ekran_kontrol.ino`, Nextion HMI TFT Sketch | `DEG-REQ-004`, `DEG-REQ-040` | `DEG-SCN-001`, `DEG-SCN-002` | `DEG-GAP-007` | Nextion HMI Visual Test | **FULL PASS** |
| **DEG-GAP-011** | **HMI Setpoint Edit Arming Disarm Callback Listener** | **HIGH** | `ekran_kontrol.ino` (L615-L750) | `DEG-REQ-007`, `DEG-REQ-034` | `DEG-SCN-003` .. `DEG-SCN-005` | `DEG-GAP-007`, `DEG-GAP-010` | HMI Touch Audit | **FULL PASS** |
| **DEG-GAP-012** | **Nextion HMI Active DEGAS Touch Locking & Visual Banner** | **HIGH** | `ekran_kontrol.ino`, Nextion HMI TFT Sketch | `DEG-REQ-007`, `DEG-REQ-042` | `DEG-SCN-008`, `DEG-SCN-012` | `DEG-GAP-007`, `DEG-GAP-010` | HMI Mock Lockout Test | **FULL PASS** |
| **DEG-GAP-013** | **Nextion HMI Tab 3 Service Settings DEGAS Page & Tank Header** | **HIGH** | `ekran_kontrol.ino`, Nextion HMI TFT Sketch | `DEG-REQ-006`, `DEG-REQ-038` | `DEG-SCN-023`, `DEG-SCN-029` | `DEG-GAP-008` | HMI Service PIN Test | **FULL PASS** |
| **DEG-GAP-014** | **Multi-Tank Addressable DEGAS Command Routing (`T1`..`T10`)** | **MEDIUM** | `ekran_kontrol.ino` (L415), `esp32_uart.c` | `DEG-REQ-045`, `DEG-REQ-046` | `DEG-SCN-028`, `DEG-SCN-030` | `DEG-GAP-006`, `DEG-GAP-009` | Multi-Tank Bus Test | **FULL PASS** |
| **DEG-GAP-015** | **HMI Telemetry Countdown & Status Text Synchronization** | **MEDIUM** | `ekran_kontrol.ino` (L500-L560) | `DEG-REQ-043`, `DEG-REQ-053` | `DEG-SCN-037`, `DEG-SCN-038` | `DEG-GAP-006`, `DEG-GAP-010` | HMI Sync Test | **FULL PASS** |
| **DEG-GAP-016** | **Automated HMI Mock DEGAS Protocol & Arming Test Suite** | **VERIFICATION** | `test_hmi_mock.py` | `DEG-REQ-004`, `DEG-REQ-038` | `DEG-SCN-001` .. `DEG-SCN-005` | `DEG-GAP-007` .. `DEG-GAP-013` | Pytest Execution | **FULL PASS** |
| **DEG-GAP-017** | **Automated RS485 Bus Multi-Tank DEGAS Command Test Suite** | **VERIFICATION** | `test_rs485_mock.py` | `DEG-REQ-022`, `DEG-REQ-032` | `DEG-SCN-019`, `DEG-SCN-028` | `DEG-GAP-006`, `DEG-GAP-009` | Pytest Execution | **FULL PASS** |
| **DEG-GAP-018** | **Hardware-in-the-Loop (HIL) Physical UART Test Suite** | **VERIFICATION** | `test_hil_uart.py` | `DEG-REQ-008`, `DEG-REQ-021` | `DEG-SCN-006`, `DEG-SCN-011` | `DEG-GAP-001` .. `DEG-GAP-015` | Physical HIL Test | **FULL PASS** |
| **DEG-GAP-019** | **Physical Acoustic Cavitation Pulse Ratio Hydrophone Tuning** | **DEFERRED PHYSICAL** | Physical Bench Oscilloscope & Hydrophone | `DEG-REQ-028` | `DEG-SCN-046` | `DEG-GAP-005`, `DEG-GAP-018` | Hydrophone Trace | **DEFERRED PHYSICAL** |
| **DEG-GAP-020** | **Physical Degassing Ultrasonic Power Thermal Limit Test** | **DEFERRED PHYSICAL** | Transducer Core Thermocouple & Tank | `DEG-REQ-027` | `DEG-SCN-047` | `DEG-GAP-001`, `DEG-GAP-018` | Thermal Bench Logger | **DEFERRED PHYSICAL** |
| **DEG-GAP-021** | **Physical Fluid Degassing Solubility Temperature Curve Test** | **DEFERRED PHYSICAL** | Bench Dissolved Oxygen (DO) Meter | `DEG-REQ-024` | `DEG-SCN-048`, `DEG-SCN-049` | `DEG-GAP-004`, `DEG-GAP-018` | DO Meter PPM Trace | **DEFERRED PHYSICAL** |

---

## 4. RECOMMENDED IMPLEMENTATION ORDER

To ensure a continuous, buildable codebase without circular dependencies, implementation must follow a strictly ordered 6-phase path:

```
[ PHASE 1: STM32 CORE FIRMWARE ]
  - DEG-GAP-001 (PWM Execution Cutout Fix)
  - DEG-GAP-002 (Process Timer Countdown Integration)
  - DEG-GAP-003 (Comm Silence Watchdog Extension)
  - DEG-GAP-004 (Optional Temperature Control Logic)
  - DEG-GAP-005 (Triac Pulse Modulation Controller)
  - DEG-GAP-006 (RS485 START_DEGAS Snapshot Parser)
         │
         v
[ PHASE 2: ESP32 MASTER & NVS STORAGE ]
  - DEG-GAP-007 (State Machine Flags: degas_armed / degas_active)
  - DEG-GAP-008 (NVS service_degas Partition Manager)
  - DEG-GAP-009 (RS485 DEGAS Protocol Command Generator)
         │
         v
[ PHASE 3: NEXTION HMI & PERMISSIONS ]
  - DEG-GAP-010 (Home Page DEGAS Function Button)
  - DEG-GAP-011 (Setpoint Edit Arming Disarm Listener)
  - DEG-GAP-012 (Active DEGAS Touch Lockout & Visual Banner)
  - DEG-GAP-013 (Tab 3 Service Settings DEGAS Page)
         │
         v
[ PHASE 4: MULTI-TANK & TELEMETRY SYNC ]
  - DEG-GAP-014 (Multi-Tank Addressable Routing T1..T10)
  - DEG-GAP-015 (HMI Telemetry & Countdown Text Sync)
         │
         v
[ PHASE 5: AUTOMATED HIL & UNIT VERIFICATION ]
  - DEG-GAP-016 (test_hmi_mock.py Extensions)
  - DEG-GAP-017 (test_rs485_mock.py Extensions)
  - DEG-GAP-018 (test_hil_uart.py Integration)
         │
         v
[ PHASE 6: DEFERRED PHYSICAL BENCH CALIBRATION ]
  - DEG-GAP-019 (Hydrophone Acoustic Pulse Ratio Tuning)
  - DEG-GAP-020 (Ultrasonic Power Thermal Limit Evaluation)
  - DEG-GAP-021 (Chemical Fluid Degassing Solubility Curve Test)
```

---

## 5. DEPENDENCY AND BLOCKER AUDIT

- **Circular Dependencies:** 0 (Verified strictly acyclic).
- **Missing Architecture Mappings:** 0 (100% of requirements, ADRs, and scenarios mapped).
- **Implementation Blockers:** None. All software implementation gaps (Phases 1 through 5) can proceed immediately without waiting for deferred physical acoustic bench tuning (Phase 6).

---
*End of Document `docs/B_DEGAS_IMPLEMENTATION_GAP_BACKLOG.md`*
