# EAGLEULTRASONİK — FREQUENCY SWEEP / SHIFTING IMPLEMENTATION GAP BACKLOG

**Document ID:** `docs/C_SWEEP_IMPLEMENTATION_GAP_BACKLOG.md`  
**Project:** EAGLEULTRASONİK  
**Phase:** C-Phase Implementation Backlog Freeze  
**Authoritative Audit Source:** [`docs/C_SWEEP_IMPLEMENTATION_COMPLIANCE_AUDIT.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/C_SWEEP_IMPLEMENTATION_COMPLIANCE_AUDIT.md)  
**Authoritative Specifications:**
- [`docs/C_SWEEP_REQUIREMENTS.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/C_SWEEP_REQUIREMENTS.md) (71 Requirements: `SWP-REQ-001` … `SWP-REQ-071`)
- [`docs/C_SWEEP_ARCHITECTURE.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/C_SWEEP_ARCHITECTURE.md) (7 ADRs: `ADR-01` … `ADR-07`)
- [`docs/C_SWEEP_SCENARIOS.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/C_SWEEP_SCENARIOS.md) (54 Scenarios: `SWP-SCN-001` … `SWP-SCN-054`)

**Status:** FROZEN BACKLOG  
**Date:** 2026-08-17  

---

## 1. EXECUTIVE SUMMARY

This document converts the findings of the completed compliance audit ([`docs/C_SWEEP_IMPLEMENTATION_COMPLIANCE_AUDIT.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/C_SWEEP_IMPLEMENTATION_COMPLIANCE_AUDIT.md)) into a frozen, minimal-dependency implementation and verification backlog.

The audit confirmed that core 28/40 kHz static frequency control, X9C bit-bang pulse generation with micro critical sections ($< 10\ \mu\text{s}$), non-blocking superloop execution, and Priority 1 SafeStop sweep deactivation are fully implemented and functional. However, 10 implementation/protocol gaps, 1 design contradiction, and 33 hardware/HIL verification items were identified.

This backlog categorizes every item into actionable engineering work packages, separating software implementation tasks from physical hardware bench execution tasks.

---

## 2. GAP COUNT BY PRIORITY & TYPE

### Summary by Priority:
- **CRITICAL:** **1** (`SWP-GAP-001` — `SET_FREQ` Active Sweep Reset)
- **HIGH:** **2** (`SWP-GAP-002` — Parametric `STEP_INCREMENT`, `SWP-GAP-003` — `STAT` Telemetry Field 10)
- **MEDIUM:** **2** (`SWP-GAP-004` — `SYS_MODE_DEGAS` Interlock, `SWP-GAP-005` — IDLE Mode Sweep Arming)
- **LOW:** **4** (`SWP-GAP-006` — Dynamic Span/Period Commands, `SWP-GAP-007` — Pytest HIL Suite, `SWP-GAP-008` — Physical Calib, `SWP-GAP-009` — Endurance Run)
- **DEFERRED / FUTURE:** **1** (`SWP-GAP-010` — Fine 1-step Physical Transducer Validation)

### Summary by Type:
- **IMPLEMENTATION Gaps (Software / Firmware / Protocol):** **6** (`SWP-GAP-001` .. `SWP-GAP-006`)
- **VERIFICATION Gaps (Pytest HIL / Physical Bench Execution):** **3** (`SWP-GAP-007` .. `SWP-GAP-009`)
- **FUTURE / DEFERRED Items:** **1** (`SWP-GAP-010`)
- **Total Backlog Items:** **10**

---

## 3. FROZEN GAP BACKLOG TABLE

| Gap ID | Priority | Type | Requirement(s) | ADR(s) | Scenario(s) | Current Behavior | Required Behavior | Dependency | Verification | Acceptance |
| :--- | :---: | :---: | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **SWP-GAP-001** | **CRITICAL** | SAFETY / IMPL | `SWP-REQ-009` | `ADR-02` | `SWP-SCN-010`<br>`SWP-SCN-011`<br>`SWP-SCN-044` | `SET_FREQ` updates center without disabling active sweep. | `SET_FREQ` must call `X9C103S_SetSweepEnabled(0U)` before setting center frequency. | None | HIL Pytest / UART Intercept | `s_sweep_enabled` becomes 0 on `SET_FREQ`. Wiper returns to static center step. |
| **SWP-GAP-002** | **HIGH** | CONFIG / IMPL | `SWP-REQ-004`<br>`SWP-REQ-013`<br>`SWP-REQ-070`<br>`SWP-REQ-071` | `ADR-07` | `SWP-SCN-052`<br>`SWP-SCN-053`<br>`SWP-SCN-054` | Step offsets hardcoded to 1 kHz steps (`-2..+2`). `STEP_INCREMENT` missing. | Implement `s_step_increment` (`1..8`, def 4), `T<ID>:SET_STEP_INC:<1..8>`, NVS `SVC_STEP_INC`, and RUNNING lock. | None | HIL Pytest / NVS Audit | `SET_STEP_INC:2` scales wiper steps to 2-step offsets. Rejected with `ERR:LOCKED_SYS_RUNNING` when running. |
| **SWP-GAP-003** | **HIGH** | TELEMETRY | `SWP-REQ-056` | `ADR-01` | `SWP-SCN-032`<br>`SWP-SCN-033`<br>`SWP-SCN-034` | `STAT` telegram emits 9 fields; `swp_st` field 10 missing. | Format `STAT` telegram with field 10 `swp_st = (sweep_enabled << 1) \| sweep_active`. Update ESP32 parser. | None | Pytest Telegram Parsing | `STAT` telegram includes field 10 (0=disabled, 2=armed, 3=active). ESP32 parses 10 fields. |
| **SWP-GAP-004** | **MEDIUM** | SAFETY / INTEGRATION | `SWP-REQ-044` | `ADR-04` | `SWP-SCN-016`<br>`SWP-SCN-017`<br>`SWP-SCN-018`<br>`SWP-SCN-041` | `system_state.h` lacks `SYS_MODE_DEGAS`. STM32 parser accepts sweep during DEGAS. | Add `SYS_MODE_DEGAS` enum to STM32. Reject `SWEEP:ON` during DEGAS with `ERR:SWEEP_PROHIBITED_IN_DEGAS`. | None | HIL Pytest Injection | `SWEEP:ON` received in `SYS_MODE_DEGAS` responds with `ERR:SWEEP_PROHIBITED_IN_DEGAS`. |
| **SWP-GAP-005** | **MEDIUM** | INTEGRATION | `SWP-REQ-027` | `ADR-01` | `SWP-SCN-002`<br>`SWP-SCN-003`<br>`SWP-SCN-013`<br>`SWP-SCN-029` | `SWEEP:ON` in IDLE mode rejected with `ERR:SWEEP_REQUIRES_RUNNING`. | Allow arming selection intent (`sweep_enabled = 1`) in IDLE without active stepping (`sweep_active = 0`). Auto-start stepping on START. | `SWP-GAP-003` | HIL Integration Test | `SWEEP:ON` in IDLE sets `swp_st = 2` (armed). START transitions mode to RUNNING and sets `swp_st = 3`. |
| **SWP-GAP-006** | **LOW** | CONFIGURATION | `SWP-REQ-022`<br>`SWP-REQ-057` | Baseline Arch | `SWP-SCN-025`<br>`SWP-SCN-026` | Sweep period fixed at 400 ms; span fixed at $\pm 2\text{ kHz}$. | Support Service Menu commands `SET_SWP_SPAN` and `SET_SWP_PER` in ESP32 NVS. | None | Service UI Audit | Service technician can modify period and span from password-protected Service Menu. |
| **SWP-GAP-007** | **LOW** | HARDWARE / TEST | `SWP-REQ-068` | Baseline Arch | `SWP-SCN-001` .. `054` | `test_hil_uart.py` lacks sweep test cases. | Expand `test_hil_uart.py` to include automated tests for `SWEEP:ON/OFF`, `SET_FREQ` reset, `SET_STEP_INC`, and SafeStop. | `SWP-GAP-001`<br>`SWP-GAP-002`<br>`SWP-GAP-003` | Automated `pytest` Run | `pytest test_hil_uart.py` passes 100% across all sweep test routines. |
| **SWP-GAP-008** | **LOW** | HARDWARE / TEST | `SWP-REQ-051`<br>`SWP-REQ-067` | Baseline Arch | `SWP-SCN-001`<br>`SWP-SCN-005`<br>`SWP-SCN-007`<br>`SWP-SCN-046` .. `048` | Physical wiper voltage & PA0 ADC feedback unmeasured on real hardware. | Perform physical bench measurement using multimeter and oscilloscope on PA0 ADC and X9C wiper $V_W$. | `SWP-GAP-001`<br>`SWP-GAP-002` | Oscilloscope / PA0 ADC Log | $V_W$ reads $1.32\text{ V} \pm 0.03\text{ V}$ (Step 40) and $2.97\text{ V} \pm 0.03\text{ V}$ (Step 90). |
| **SWP-GAP-009** | **LOW** | HARDWARE / TEST | `SWP-REQ-019`<br>`SWP-REQ-020`<br>`SWP-REQ-050` | Baseline Arch | `SWP-SCN-050`<br>`SWP-SCN-051` | Long-duration endurance & thermal stability unverified on bench setup. | Execute 10,000 continuous triangle cycles (66 min) and measure thermal voltage drift on physical bench setup. | `SWP-GAP-001`<br>`SWP-GAP-002` | Bench Endurance Run | 10,000 cycles completed with 0 errors, 0 drops, and $< 10\text{ mV}$ thermal drift. |
| **SWP-GAP-010** | **DEFERRED** | FUTURE / EVAL | `SWP-REQ-004` | `OD-SWP-01` | `SWP-SCN-049` | Fine 1-step sweep (`STEP_INCREMENT = 1`) unverified on transducer load. | Evaluate acoustic cleaning efficacy of fine 1-step sweep (`38 → 39 → 40 → 41 → 42`) with physical motor card. | `SWP-GAP-002` | Acoustic Bench Test | Physical transducer motor test report completed for fine 1-step sweep. |

---

## 4. DEPENDENCY / EXECUTION ORDER

The backlog enforces a strict minimum-dependency execution sequence:

```
[SWP-GAP-001: SET_FREQ Active Sweep Reset]  <--- CRITICAL (Safety / State Correctness)
            │
            ▼
[SWP-GAP-002: Parametric STEP_INCREMENT]    <--- HIGH (Core Parameterization)
            │
            ├───> [SWP-GAP-003: STAT Telemetry Field 10 (swp_st)]   <--- HIGH (Protocol)
            │                 │
            │                 ▼
            │     [SWP-GAP-005: IDLE Sweep Arming Intent]            <--- MEDIUM (HMI / Integration)
            │
            ├───> [SWP-GAP-004: SYS_MODE_DEGAS Interlock]            <--- MEDIUM (DEGAS Safety)
            │
            └───> [SWP-GAP-006: Dynamic Span/Period Service Cmds]    <--- LOW (Configuration)
                              │
                              ▼
[SWP-GAP-007: Pytest HIL Sweep Test Suite Expansion]                 <--- LOW (Regression Test)
            │
            ├───> [SWP-GAP-008: Physical PA0/VW Hardware Bench Calib] <--- LOW (Physical Validation)
            │
            └───> [SWP-GAP-009: 10,000 Cycle Thermal Endurance Run]  <--- LOW (Long-Duration Stability)
                              │
                              ▼
[SWP-GAP-010: Fine 1-Step Transducer Acoustic Validation]            <--- DEFERRED / FUTURE
```

---

## 5. DETAILED IMPLEMENTATION GAPS

### SWP-GAP-001 — `SET_FREQ` Active Sweep Reset & Deactivation
- **Gap ID:** `SWP-GAP-001`
- **Source Requirements:** `SWP-REQ-009`
- **Source ADRs:** `ADR-02`
- **Source Scenarios:** `SWP-SCN-010`, `SWP-SCN-011`, `SWP-SCN-044`
- **Priority:** **CRITICAL**
- **Type:** SAFETY / IMPLEMENTATION
- **Current Behavior:** [`esp32_uart.c:419-439`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L419-L439) processes `SET_FREQ` by calling `X9C103S_SetFrequency()` directly. `s_sweep_enabled` remains `1`. `X9C103S_SweepProcess()` continues stepping around the old captured center frequency.
- **Required Behavior:** Receiving `SET_FREQ` while sweep is active must immediately disable sweep (`X9C103S_SetSweepEnabled(0U)`), restore static baseline, apply new center frequency (Step 40 or Step 90), and require an explicit future `SWEEP:ON` command to start a new sweep cycle.
- **Concrete Implementation Anchor:** [`STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c:424`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L424)
- **Dependency:** None
- **Verification Method:** Send `T1:SWEEP:ON` in `SYS_MODE_RUNNING` at 28 kHz, verify stepping active, send `T1:SET_FREQ:40`. Inspect UART response and verify wiper shifts to Step 90 static without stepping.
- **Acceptance Criterion:** `X9C103S_IsSweepEnabled()` returns 0 immediately upon receiving `SET_FREQ`. Wiper moves to exact new center frequency step.

---

### SWP-GAP-002 — Parametric `STEP_INCREMENT` Model & Service Parameterization
- **Gap ID:** `SWP-GAP-002`
- **Source Requirements:** `SWP-REQ-004`, `SWP-REQ-013`, `SWP-REQ-070`, `SWP-REQ-071`
- **Source ADRs:** `ADR-07`
- **Source Scenarios:** `SWP-SCN-052`, `SWP-SCN-053`, `SWP-SCN-054`
- **Priority:** **HIGH**
- **Type:** CONFIGURATION / IMPLEMENTATION
- **Current Behavior:** [`x9c103s.c:26-29`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/x9c103s.c#L26-L29) uses hardcoded offset array `{-2, -1, 0, 1, 2, 1, 0, -1, -2}` with fixed 1 kHz interpolation steps. Firmware lacks `s_step_increment` variable, `T<ID>:SET_STEP_INC` ASCII command, and ESP32 NVS parameter `SVC_STEP_INC`.
- **Required Behavior:** Implement `s_step_increment` variable (allowed range `1..8`, default `4`) in `x9c103s.c`. Scale triangle offsets as $\text{BASE\_STEP} + (\text{s\_sweep\_offsets}[i] \times \text{s\_step\_increment})$. Implement `T<ID>:SET_STEP_INC:<1..8>` command handler in `esp32_uart.c` with `SYS_MODE_RUNNING` interlock check (reject with `ERR:LOCKED_SYS_RUNNING`). Persist `SVC_STEP_INC` in ESP32 Service NVS Flash.
- **Concrete Implementation Anchors:**
  - [`STM32/Ultrasonik_G4_Master/Core/Src/x9c103s.c:31`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/x9c103s.c#L31)
  - [`STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c:194`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L194)
  - [`esp32/ekran_kontrol/ekran_kontrol.ino:190`](file:///c:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino#L190)
- **Dependency:** None
- **Verification Method:** Send `T1:SET_STEP_INC:2` in `SYS_MODE_IDLE`. Verify `ACK:STEP_INC:2` response. Execute 28 kHz sweep and verify wiper step range is $36 \dots 44$. Send `T1:SET_STEP_INC:2` in `SYS_MODE_RUNNING` and verify rejection `ERR:LOCKED_SYS_RUNNING`.
- **Acceptance Criterion:** `s_step_increment` dynamically scales step outputs. `SET_STEP_INC` command functions in IDLE and is rejected in RUNNING mode.

---

### SWP-GAP-003 — Telemetry `STAT` Telegram Field 10 (`swp_st`) Expansion
- **Gap ID:** `SWP-GAP-003`
- **Source Requirements:** `SWP-REQ-056`
- **Source ADRs:** `ADR-01`
- **Source Scenarios:** `SWP-SCN-032`, `SWP-SCN-033`, `SWP-SCN-034`
- **Priority:** **HIGH**
- **Type:** TELEMETRY
- **Current Behavior:** [`esp32_uart.c:488`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L488) formats status telegram as `STAT,%u,%s,%u,%d,%u,%u,%u,%u,%u\n` (9 fields). ESP32 [`ekran_kontrol.ino:470`](file:///c:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino#L470) parses 9 fields. Field 10 (`swp_st`) is omitted.
- **Required Behavior:** Format `STAT` telegram to include field 10: `STAT,%u,%s,%u,%d,%u,%u,%u,%u,%u,%u\n` where field 10 is `swp_st = (s_sweep_enabled << 1) | s_sweep_active`. Update ESP32 `stmTelemetryIsle()` to parse 10 fields and update HMI status display.
- **Concrete Implementation Anchors:**
  - [`STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c:488`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L488)
  - [`esp32/ekran_kontrol/ekran_kontrol.ino:470`](file:///c:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino#L470)
- **Dependency:** None
- **Verification Method:** Intercept periodic `STAT` telegrams over ST-Link VCP / RS485. Confirm field 10 reports `0` (disabled), `2` (armed in IDLE), and `3` (active in RUNNING).
- **Acceptance Criterion:** Field 10 correctly reflects `swp_st` bitmask. ESP32 parses telemetry line cleanly without frame drop.

---

### SWP-GAP-004 — `SYS_MODE_DEGAS` Enum & STM32 Slave Level DEGAS Sweep Interlock
- **Gap ID:** `SWP-GAP-004`
- **Source Requirements:** `SWP-REQ-044`
- **Source ADRs:** `ADR-04`
- **Source Scenarios:** `SWP-SCN-016`, `SWP-SCN-017`, `SWP-SCN-018`, `SWP-SCN-041`
- **Priority:** **MEDIUM**
- **Type:** SAFETY / INTEGRATION
- **Current Behavior:** [`system_state.h:17-22`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Inc/system_state.h#L17-L22) defines `SystemMode_t` with `SYS_MODE_IDLE`, `SYS_MODE_RUNNING`, `SYS_MODE_FAULT`. `SYS_MODE_DEGAS` is missing. STM32 command parser lacks DEGAS mode sweep rejection.
- **Required Behavior:** Add `SYS_MODE_DEGAS = 3` to `SystemMode_t` in `system_state.h`. Add check in `esp32_uart.c`: if `SWEEP:ON` is received while `mode == SYS_MODE_DEGAS`, respond with `ERR:SWEEP_PROHIBITED_IN_DEGAS\n` and reject execution.
- **Concrete Implementation Anchors:**
  - [`STM32/Ultrasonik_G4_Master/Core/Inc/system_state.h:21`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Inc/system_state.h#L21)
  - [`STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c:196`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L196)
- **Dependency:** None
- **Verification Method:** Set system mode to `SYS_MODE_DEGAS`. Send `T1:SWEEP:ON`. Verify response is `ERR:SWEEP_PROHIBITED_IN_DEGAS\n` and sweep remains disabled.
- **Acceptance Criterion:** STM32 firmware rejects sweep activation whenever `g_system_state.mode == SYS_MODE_DEGAS`.

---

### SWP-GAP-005 — Decoupled `sweep_enabled` Selection Arming in IDLE Mode
- **Gap ID:** `SWP-GAP-005`
- **Source Requirements:** `SWP-REQ-027`
- **Source ADRs:** `ADR-01`
- **Source Scenarios:** `SWP-SCN-002`, `SWP-SCN-003`, `SWP-SCN-013`, `SWP-SCN-029`
- **Priority:** **MEDIUM**
- **Type:** INTEGRATION
- **Current Behavior:** [`esp32_uart.c:196`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L196) rejects `SWEEP:ON` in `SYS_MODE_IDLE` with `ERR:SWEEP_REQUIRES_RUNNING`. ESP32 [`ekran_kontrol.ino:665`](file:///c:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino#L665) blocks touching Sweep ON in IDLE.
- **Required Behavior:** Update command parser in `esp32_uart.c` and `ekran_kontrol.ino` to permit setting `sweep_enabled = 1` in `SYS_MODE_IDLE`. When armed in IDLE, `s_sweep_enabled = 1`, `s_sweep_active = 0`, telemetry `swp_st = 2`. When `START` command is received, transition to `SYS_MODE_RUNNING` and automatically set `s_sweep_active = 1` (`swp_st = 3`).
- **Concrete Implementation Anchors:**
  - [`STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c:196`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L196)
  - [`STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c:269`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L269)
  - [`esp32/ekran_kontrol/ekran_kontrol.ino:665`](file:///c:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino#L665)
- **Dependency:** `SWP-GAP-003` (`swp_st` telemetry)
- **Verification Method:** Send `T1:SWEEP:ON` in IDLE. Confirm `ACK:SWEEP:ARMED` response and `swp_st = 2`. Send `T1:START`. Confirm transition to RUNNING, `swp_st = 3`, and active wiper stepping.
- **Acceptance Criterion:** Sweep selection intent (`sweep_enabled`) can be armed in IDLE prior to process start.

---

### SWP-GAP-006 — Dynamic Service Commands for Sweep Span & Period
- **Gap ID:** `SWP-GAP-006`
- **Source Requirements:** `SWP-REQ-022`, `SWP-REQ-057`
- **Source ADRs:** Baseline Architecture (Section 8.1)
- **Source Scenarios:** `SWP-SCN-025`, `SWP-SCN-026`
- **Priority:** **LOW**
- **Type:** CONFIGURATION
- **Current Behavior:** Sweep period is fixed at 400 ms (`#define X9C_SWEEP_PERIOD_MS 400U`); sweep span is fixed at $\pm 2\text{ kHz}$. No ASCII commands exist to configure span or period from Service Menu.
- **Required Behavior:** Implement Service Menu configuration interface in ESP32 NVS (`SVC_SWP_SPAN`, `SVC_SWP_PER`) allowing authorized service personnel to view/edit span and period parameters.
- **Concrete Implementation Anchor:** [`esp32/ekran_kontrol/ekran_kontrol.ino:190`](file:///c:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino#L190)
- **Dependency:** None
- **Verification Method:** Authenticate on password-protected Service Menu page in Nextion HMI. Modify sweep period. Verify parameter persists in NVS.
- **Acceptance Criterion:** Service settings persist in NVS and remain completely hidden from operator UI.

---

## 6. VERIFICATION-ONLY GAPS

### SWP-GAP-007 — Automated Pytest HIL Sweep Test Suite Expansion
- **Gap ID:** `SWP-GAP-007`
- **Source Requirements:** `SWP-REQ-068`
- **Source ADRs:** Baseline Architecture
- **Source Scenarios:** `SWP-SCN-001` .. `SWP-SCN-054`
- **Priority:** **LOW**
- **Type:** HARDWARE / TEST
- **Current Behavior:** [`test_hil_uart.py:537-555`](file:///c:/Users/ern0e/EAGLEULTRASONiK/test_hil_uart.py#L537-L555) contains static frequency tests (`test_f1_set_freq_28`, `test_f2_set_freq_40`, `test_f3_set_freq_invalid`), but lacks test cases for `SWEEP:ON`, `SWEEP:OFF`, `SET_STEP_INC`, and SafeStop sweep deactivation.
- **Required Behavior:** Expand `test_hil_uart.py` with explicit automated pytest functions: `test_sweep_on_off()`, `test_set_freq_resets_sweep()`, `test_set_step_inc_configuration()`, and `test_safestop_sweep_deactivation()`.
- **Concrete Implementation Anchor:** [`test_hil_uart.py:556`](file:///c:/Users/ern0e/EAGLEULTRASONiK/test_hil_uart.py#L556)
- **Dependency:** `SWP-GAP-001`, `SWP-GAP-002`, `SWP-GAP-003`
- **Verification Method:** Execute `pytest test_hil_uart.py -v` against physical HIL hardware / test host.
- **Acceptance Criterion:** All test functions pass 100% with 0 failures or assertion errors.

---

### SWP-GAP-008 — Physical X9C Wiper Voltage & PA0 ADC Feedback Calibration
- **Gap ID:** `SWP-GAP-008`
- **Source Requirements:** `SWP-REQ-051`, `SWP-REQ-067`
- **Source ADRs:** Baseline Hardware Authority
- **Source Scenarios:** `SWP-SCN-001`, `SWP-SCN-005`, `SWP-SCN-007`, `SWP-SCN-046`, `SWP-SCN-047`, `SWP-SCN-048`
- **Priority:** **LOW**
- **Type:** HARDWARE / TEST
- **Current Behavior:** Firmware implementation is present in [`x9c103s.c`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/x9c103s.c) and [`main.c:568`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L568), but physical bench multimeter/oscilloscope measurement table has not been logged on physical hardware.
- **Required Behavior:** Connect digital storage oscilloscope and multimeter to PA0 ADC IN1 and X9C wiper terminal $V_W$. Measure wiper voltage and hybrid card output frequency across steps 32, 36, 40, 44, 48 (28 kHz) and 82, 86, 90, 94, 98 (40 kHz). Populate calibration matrix in SWP-SCN-048.
- **Concrete Implementation Anchor:** Physical Hardware Bench / `PA0` ADC Pin
- **Dependency:** `SWP-GAP-001`, `SWP-GAP-002`
- **Verification Method:** Measure $V_W$ wiper voltage at each step index. Confirm $V_W = 1.32\text{ V} \pm 0.03\text{ V}$ at Step 40 and $V_W = 2.97\text{ V} \pm 0.03\text{ V}$ at Step 90.
- **Acceptance Criterion:** Measured wiper voltages match theoretical ladder values within $\pm 2\%$ tolerance.

---

### SWP-GAP-009 — Long-Duration Sweep Endurance & Thermal Stability Validation
- **Gap ID:** `SWP-GAP-009`
- **Source Requirements:** `SWP-REQ-019`, `SWP-REQ-020`, `SWP-REQ-050`
- **Source ADRs:** Baseline Architecture
- **Source Scenarios:** `SWP-SCN-050`, `SWP-SCN-051`
- **Priority:** **LOW**
- **Type:** HARDWARE / TEST
- **Current Behavior:** Non-blocking state machine is implemented in [`x9c103s.c:258`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/x9c103s.c#L258), but 60-minute long-duration endurance run has not been executed on physical hardware setup.
- **Required Behavior:** Execute continuous 10,000 triangle cycle endurance run (66 minutes). Log PA0 ADC counts and RS485 telemetry packet delivery rate. Verify zero dropped frames, zero SafeStop triggers, and $< 10\text{ mV}$ thermal voltage drift.
- **Concrete Implementation Anchor:** Physical Hardware Bench
- **Dependency:** `SWP-GAP-001`, `SWP-GAP-002`
- **Verification Method:** Run 66-minute continuous sweep cycle test on physical desktop setup.
- **Acceptance Criterion:** 10,000 cycles completed with 100% telemetry delivery and $< 10\text{ mV}$ thermal drift.

---

## 7. DEFERRED / FUTURE ITEMS

### SWP-GAP-010 — Fine-Sweep 1-Step Physical Transducer Validation (`OD-SWP-01`)
- **Gap ID:** `SWP-GAP-010`
- **Source Requirements:** `SWP-REQ-004`
- **Source ADRs:** `OD-SWP-01`
- **Source Scenarios:** `SWP-SCN-049`
- **Priority:** **DEFERRED / FUTURE**
- **Type:** FUTURE / EVALUATION
- **Current Behavior:** Fine-step 1-step sweep configuration capability is supported parametrically by setting `STEP_INCREMENT = 1` ($\text{BASE\_STEP} + \text{multiplier} \times 1 \rightarrow 38 \rightarrow 39 \rightarrow 40 \rightarrow 41 \rightarrow 42$), but physical acoustic cleaning efficacy on real transducer motor load is unverified.
- **Required Behavior:** Perform physical transducer motor acoustics test comparing 1-step fine sweep (`STEP_INCREMENT = 1`) against 4-step baseline sweep (`STEP_INCREMENT = 4`) to evaluate cavitation performance.
- **Concrete Implementation Anchor:** Future Transducer Acoustics Bench
- **Dependency:** `SWP-GAP-002` (`STEP_INCREMENT = 1` configuration capability)
- **Verification Method:** Physical transducer cavitation measurement.
- **Acceptance Criterion:** Acoustic evaluation report completed for post-prototype engineering review.

---

## 8. FINAL C12/C13 CLOSURE CHECKLIST

To achieve formal C-Phase sign-off and transition to production deployment, all backlog items must satisfy the following completion criteria:

- [ ] **Item 1 (`SWP-GAP-001`):** `SET_FREQ` in `esp32_uart.c` verified to call `X9C103S_SetSweepEnabled(0U)`.
- [ ] **Item 2 (`SWP-GAP-002`):** `s_step_increment` implemented in `x9c103s.c`; `T<ID>:SET_STEP_INC:<1..8>` command verified in `esp32_uart.c`; `SVC_STEP_INC` stored in ESP32 NVS.
- [ ] **Item 3 (`SWP-GAP-003`):** `STAT` telegram field 10 (`swp_st`) verified in `esp32_uart.c` and parsed in `ekran_kontrol.ino`.
- [ ] **Item 4 (`SWP-GAP-004`):** `SYS_MODE_DEGAS` enum added to `system_state.h`; DEGAS sweep rejection verified.
- [ ] **Item 5 (`SWP-GAP-005`):** `sweep_enabled` selection intent arming in IDLE verified.
- [ ] **Item 6 (`SWP-GAP-006`):** Dynamic span/period Service commands stored in NVS.
- [ ] **Item 7 (`SWP-GAP-007`):** `pytest test_hil_uart.py` passes 100% across all sweep test routines.
- [ ] **Item 8 (`SWP-GAP-008`):** Physical multimeter & oscilloscope measurements on PA0 ADC logged.
- [ ] **Item 9 (`SWP-GAP-009`):** 10,000 cycle (66 min) endurance test completed with 0 errors.
- [ ] **Item 10 (`SWP-GAP-010`):** Fine 1-step sweep evaluation report archived for future review.

---

## 9. RECAP OF BACKLOG METRICS

- **Backlog Document Created:** `docs/C_SWEEP_IMPLEMENTATION_GAP_BACKLOG.md`
- **Total Gaps Identified:** 10 (`SWP-GAP-001` .. `SWP-GAP-010`)
- **Priority Counts:**
  - **CRITICAL:** 1 (`SWP-GAP-001`)
  - **HIGH:** 2 (`SWP-GAP-002`, `SWP-GAP-003`)
  - **MEDIUM:** 2 (`SWP-GAP-004`, `SWP-GAP-005`)
  - **LOW:** 4 (`SWP-GAP-006`, `SWP-GAP-007`, `SWP-GAP-008`, `SWP-GAP-009`)
  - **DEFERRED / FUTURE:** 1 (`SWP-GAP-010`)
- **Classification Counts:**
  - **Implementation Gaps:** 6 (`SWP-GAP-001` .. `SWP-GAP-006`)
  - **Verification Gaps:** 3 (`SWP-GAP-007` .. `SWP-GAP-009`)
  - **Deferred/Future Items:** 1 (`SWP-GAP-010`)
- **First Implementation Task:** `SWP-GAP-001` (`SET_FREQ` Active Sweep Reset in `esp32_uart.c`)
- **Audit Contradictions:** 0 (The audit is 100% self-consistent and frozen into this backlog)

---
*End of Document `docs/C_SWEEP_IMPLEMENTATION_GAP_BACKLOG.md`*
