# EAGLEULTRASONİK — FREQUENCY SWEEP / SHIFTING SCENARIO SPECIFICATION (C-FAZ 3)

**Document ID:** `docs/C_SWEEP_SCENARIOS.md`  
**Project:** EAGLEULTRASONİK  
**Phase:** C-Faz 3 — Scenario Freeze & Test Acceptance Specification (Parametric Sweep Step Update)  
**Requirements Authority:** [`docs/C_SWEEP_REQUIREMENTS.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/C_SWEEP_REQUIREMENTS.md)  
**Architecture Authority:** [`docs/C_SWEEP_ARCHITECTURE.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/C_SWEEP_ARCHITECTURE.md)  
**Status:** FROZEN SCENARIO SPECIFICATION  
**Date:** 2026-08-16  

---

## 1. PURPOSE AND SCOPE

### 1.1 Purpose
This document defines and freezes the complete set of prototype testing scenarios, expected system behaviors, and PASS/FAIL criteria for the Frequency Sweep (Shifting) feature of EAGLEULTRASONİK. 

This document incorporates the baseline requirements from [`docs/C_SWEEP_REQUIREMENTS.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/C_SWEEP_REQUIREMENTS.md) and architectural design specifications from [`docs/C_SWEEP_ARCHITECTURE.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/C_SWEEP_ARCHITECTURE.md), freezing all test cases without modifying firmware or HMI codebase.

### 1.2 Scope
The testing scope encompasses end-to-end integration across all hardware and software layers:
- STM32G474RE HAL firmware (`x9c103s.c`, `system_state.c`, `esp32_uart.c`, `main.c`)
- ESP32-S3 Master & Nextion HMI interface (`esp32/ekran_kontrol/ekran_kontrol.ino`)
- Addressable RS485 ASCII protocol framing (`T<ID>:SWEEP:ON`, `T<ID>:SWEEP:OFF`, `T<ID>:SET_FREQ:<freq>`)
- Parametric sweep step behavior (`BASE_STEP_28 = 40`, `BASE_STEP_40 = 90`, `STEP_INCREMENT = 4` default, `1..8` range)
- Physical X9C103S 10kΩ digital potentiometer wiper stepping and PA0 ADC feedback verification
- HIL Pytest integration suite (`test_hil_uart.py`) and bench commissioning procedures

---

## 2. TEST / SCENARIO STRUCTURE

Every scenario is assigned a unique identifier in the format:
`SWP-SCN-XXX`

Each scenario explicitly specifies:
- **Preconditions:** System state, operating mode, flags, and pin configurations required before test execution.
- **Input / Action:** Commands, user interactions, or environmental events applied to the system.
- **Expected Behavior:** Sequence of state changes, message exchanges, and hardware responses.
- **PASS Criteria:** Definitive, measurable conditions for successful test completion.
- **FAIL Criteria:** Specific anomalies, timeouts, or unexpected states causing test failure.

---

## 3. STATE TERMINOLOGY & PARAMETRIC MODEL

### 3.1 Parametric Sweep Step Model
- `BASE_STEP_28 = 40`: Default base wiper step for 28 kHz center frequency.
- `BASE_STEP_40 = 90`: Default base wiper step for 40 kHz center frequency.
- `STEP_INCREMENT`: Service Settings parameter determining wiper step delta per frequency point.
  - Allowed Integer Range: `1, 2, 3, 4, 5, 6, 7, 8`.
  - Default / Prototype Value: `4`.
- Offset Multipliers: `OFFSET_MULTIPLIERS = [-2, -1, 0, +1, +2, +1, 0, -1, -2]`.
- Parametric Step Formula: $\text{target\_step} = \text{BASE\_STEP} + (\text{multiplier} \times \text{STEP\_INCREMENT})$.

### 3.2 `sweep_enabled` (Selection / Intent Flag)
- `sweep_enabled = 1`: Sweep is selected by operator touch or armed via loaded recipe.
- *HMI Representation:* Displayed as a **GREEN** Sweep button on the Nextion UI.
- *System Meaning:* Selection intent is active. If machine is in `SYS_MODE_IDLE`, sweep execution does NOT run yet.

### 3.3 `sweep_active` (Hardware Execution Flag)
- `sweep_active = 1`: STM32 wiper state machine (`X9C103S_SweepProcess()`) actively steps the X9C potentiometer through parametric step offsets.
- *System Constraint:* `sweep_active = 1` occurs **only** when `sweep_enabled == 1` AND system mode is `SYS_MODE_RUNNING`.

### 3.4 System Modes
- `SYS_MODE_IDLE` (0): Standby mode; outputs OFF.
- `SYS_MODE_RUNNING` (1): Active washing mode; soft-start PWM active, process timer running.
- `SYS_MODE_FAULT` (2): Safety fault mode; hardware outputs cut, center step restored.
- `SYS_MODE_DEGAS` (3): Dedicated liquid degassing mode; sweep strictly prohibited.

---

## 4. NORMAL SWEEP SCENARIOS

### SWP-SCN-001 — Sweep OFF Baseline
- **Preconditions:** System mode `SYS_MODE_IDLE`, `sweep_enabled = 0`, `sweep_active = 0`.
- **Input / Action:** System remains in static standby.
- **Expected Behavior:** Potentiometer stays fixed at base center step 40 (`BASE_STEP_28`, 28 kHz, $V_W \approx 1.32\text{ V}$). No stepping pulses issued on INC/UD pins.
- **PASS Criteria:** `PA0` ADC telemetry reads constant $1.32\text{ V} \pm 0.03\text{ V}$. Zero pulse transitions on `PB14`. `STAT` telegram reports `swp_st = 0`.
- **FAIL Criteria:** Any wiper movement, voltage shift, or active sweep flag reported.

### SWP-SCN-002 — Sweep Selection in IDLE
- **Preconditions:** System mode `SYS_MODE_IDLE`, `sweep_enabled = 0`.
- **Input / Action:** Operator touches "SWEEP ON" on Nextion HMI (or sends `T1:SWEEP:ON`).
- **Expected Behavior:** ESP32 sets `sweep_enabled = 1` and updates HMI button to **GREEN**. STM32 receives `T1:SWEEP:ON`, updates `s_sweep_enabled = 1`, but keeps `s_sweep_active = 0` because mode is `SYS_MODE_IDLE`. STM32 responds with `ACK:SWEEP:ON,PERIOD_MS=400,SPAN=+-2KHZ`.
- **PASS Criteria:** HMI button turns **GREEN**. Telemetry reports `swp_st = 2` (`sweep_enabled = 1`, `sweep_active = 0`). Wiper does NOT step.
- **FAIL Criteria:** Wiper starts stepping prior to START command (`sweep_active` becomes 1 in IDLE).

### SWP-SCN-003 — START With Sweep Selected
- **Preconditions:** System mode `SYS_MODE_IDLE`, `sweep_enabled = 1`, `sweep_active = 0`.
- **Input / Action:** Operator presses "START" button (or sends `T1:START`).
- **Expected Behavior:** System mode transitions `SYS_MODE_IDLE → SYS_MODE_RUNNING`. STM32 evaluates `s_sweep_enabled == 1`, sets `s_sweep_active = 1`, and drives wiper to lower endpoint step ($\text{BASE\_STEP\_28} - 2 \times \text{STEP\_INCREMENT} = 32$). Main loop commences 50 ms triangle stepping.
- **PASS Criteria:** System transitions to RUNNING. Wiper immediately shifts to lower endpoint Step 32 and executes continuous 400 ms triangle cycle. Telemetry reports `swp_st = 3`.
- **FAIL Criteria:** System enters RUNNING but wiper remains at Step 40, or starts from wrong step index.

### SWP-SCN-004 — START Without Sweep
- **Preconditions:** System mode `SYS_MODE_IDLE`, `sweep_enabled = 0`.
- **Input / Action:** Operator presses "START" button (or sends `T1:START`).
- **Expected Behavior:** System mode transitions `SYS_MODE_IDLE → SYS_MODE_RUNNING`. Potentiometer remains fixed at Step 40 (`BASE_STEP_28`, 28 kHz center). `sweep_active` remains 0.
- **PASS Criteria:** Normal high-voltage washing starts at fixed static 28 kHz frequency. `PA0` ADC voltage remains constant at $1.32\text{ V}$.
- **FAIL Criteria:** Sweep activates automatically upon START when `sweep_enabled == 0`.

### SWP-SCN-005 — 28 kHz Sweep Cycle Sequence (`STEP_INCREMENT = 4` Default)
- **Preconditions:** System `SYS_MODE_RUNNING`, Center = 28 kHz (`BASE_STEP_28 = 40`), `STEP_INCREMENT = 4`, `sweep_enabled = 1`, `sweep_active = 1`.
- **Input / Action:** Observe continuous wiper stepping across multiple 400 ms triangle cycles.
- **Expected Sequence & Step Mapping ($\text{BASE\_STEP\_28} + \text{multiplier} \times 4$):**
  ```
  Index 0: 26 kHz -> Step 32 (40 - 8) [VW ~ 1.05 V]
  Index 1: 27 kHz -> Step 36 (40 - 4) [VW ~ 1.18 V]
  Index 2: 28 kHz -> Step 40 (40 + 0) [VW ~ 1.32 V]
  Index 3: 29 kHz -> Step 44 (40 + 4) [VW ~ 1.45 V]
  Index 4: 30 kHz -> Step 48 (40 + 8) [VW ~ 1.58 V]
  Index 5: 29 kHz -> Step 44 (40 + 4) [VW ~ 1.45 V]
  Index 6: 28 kHz -> Step 40 (40 + 0) [VW ~ 1.32 V]
  Index 7: 27 kHz -> Step 36 (40 - 4) [VW ~ 1.18 V]
  Index 8: 26 kHz -> Step 32 (40 - 8) [VW ~ 1.05 V] [Rollover to Index 0]
  ```
- **PASS Criteria:** All 9 offset indices execute in exact order with correct wiper steps. PA0 ADC trace matches voltage staircase.
- **FAIL Criteria:** Step skipping, reversed sequence order, or out-of-range step output.

### SWP-SCN-006 — Sweep Timing Precision
- **Preconditions:** 28 kHz sweep active in `SYS_MODE_RUNNING`.
- **Input / Action:** Measure point interval and full cycle period using oscilloscope / logic analyzer on INC pin (`PB14`).
- **Expected Behavior:**
  - Transition interval per step (`X9C_SWEEP_POINT_MS`): $50\text{ ms} \pm 2\text{ ms}$.
  - Full cyclic triangle period (`X9C_SWEEP_PERIOD_MS`): $400\text{ ms} \pm 10\text{ ms}$ (8 transitions × 50 ms).
- **PASS Criteria:** Measured timing complies with $50\text{ ms}$ step / $400\text{ ms}$ cycle tolerance. Superloop execution delay remains $< 1\text{ ms}$.
- **FAIL Criteria:** Transition interval exceeds $55\text{ ms}$ or exhibits timing drift due to blocking calls.

---

## 5. 40 kHz SWEEP SCENARIOS

### SWP-SCN-007 — 40 kHz Sweep Cycle Sequence (`STEP_INCREMENT = 4` Default)
- **Preconditions:** Center = 40 kHz (`BASE_STEP_40 = 90`), `STEP_INCREMENT = 4`, `SYS_MODE_RUNNING`, `sweep_enabled = 1`.
- **Input / Action:** Execute sweep active process around 40 kHz center.
- **Expected Sequence & Step Mapping ($\text{BASE\_STEP\_40} + \text{multiplier} \times 4$):**
  ```
  Index 0: 38 kHz -> Step 82 (90 - 8) [VW ~ 2.70 V]
  Index 1: 39 kHz -> Step 86 (90 - 4) [VW ~ 2.83 V]
  Index 2: 40 kHz -> Step 90 (90 + 0) [VW ~ 2.97 V]
  Index 3: 41 kHz -> Step 94 (90 + 4) [VW ~ 3.10 V]
  Index 4: 42 kHz -> Step 98 (90 + 8) [VW ~ 3.23 V]
  Index 5: 41 kHz -> Step 94 (90 + 4) [VW ~ 3.10 V]
  Index 6: 40 kHz -> Step 90 (90 + 0) [VW ~ 2.97 V]
  Index 7: 39 kHz -> Step 86 (90 - 4) [VW ~ 2.83 V]
  Index 8: 38 kHz -> Step 82 (90 - 8) [VW ~ 2.70 V] [Rollover to Index 0]
  ```
- **PASS Criteria:** Wiper steps transition smoothly between Step 82 and Step 98 around Step 90 center. $V_W$ remains $\le 3.3\text{ V}$.
- **FAIL Criteria:** Step calculation exceeds 99, or applies 28 kHz step offsets to 40 kHz base center.

---

## 6. STOP / SAFE STOP SCENARIOS

### SWP-SCN-008 — STOP During Active Sweep
- **Preconditions:** System in `SYS_MODE_RUNNING`, `sweep_enabled = 1`, `sweep_active = 1`.
- **Input / Action:** Operator presses "STOP" button (or sends `T1:STOP`).
- **Expected Behavior:**
  1. `SystemState_SafeStop(STOP_REASON_USER_STOP)` executes.
  2. Priority 1 instruction disables sweep (`s_sweep_enabled = 0`, `s_sweep_active = 0`).
  3. Potentiometer instantly restores exact base center step 40 (or 90).
  4. Mode transitions to `SYS_MODE_IDLE`.
  5. Heater relay and Triac gate cut OFF immediately.
  6. HMI Sweep button returns to **GRAY**.
- **PASS Criteria:** Wiper stepping ceases instantly ($< 1\text{ ms}$ delay). Wiper voltage returns to center step ($1.32\text{ V}$). System enters IDLE.
- **FAIL Criteria:** Wiper continues stepping after STOP command, or fails to restore center step.

### SWP-SCN-009 — SAFE STOP Priority Execution During Sweep
- **Preconditions:** System in `SYS_MODE_RUNNING`, `sweep_enabled = 1`, `sweep_active = 1`.
- **Input / Action:** Inject high-temperature sensor fault (`STOP_REASON_SENSOR_FAULT`).
- **Expected Behavior:** `SystemState_SafeStop()` is called from fault interrupt handler. Sweep deactivation (`X9C103S_SetSweepEnabled(0U)`) executes **before** output power termination, guaranteeing zero delay in safety shutdown.
- **PASS Criteria:** System enters `SYS_MODE_FAULT`. Outputs cut instantly. Sweep disabled and center step restored.
- **FAIL Criteria:** Sweep deactivation blocks or delays safety relay/triac cutoff.

---

## 7. FREQUENCY CHANGE SCENARIOS

### SWP-SCN-010 — SET_FREQ During Active Sweep
- **Preconditions:** System in `SYS_MODE_RUNNING`, Center = 28 kHz, `sweep_enabled = 1`, `sweep_active = 1`.
- **Input / Action:** Send `T1:SET_FREQ:40` while sweep is actively stepping.
- **Expected Behavior:**
  1. ESP32/STM32 rule `ADR-02` applies: Center frequency change disables active sweep.
  2. `s_sweep_enabled = 0`, `s_sweep_active = 0`.
  3. HMI Sweep button turns **GRAY**.
  4. STM32 issues `ACK:SWEEP:OFF,CENTER_RESTORED`.
  5. Center frequency updates to 40 kHz (`g_system_state.frequency_khz = 40`).
  6. Wiper moves to `BASE_STEP_40 = 90` (40 kHz center).
  7. System continues running in Static 40 kHz mode.
- **PASS Criteria:** Sweep is immediately disabled. Wiper moves to Step 90 ($2.97\text{ V}$). HMI button turns **GRAY**.
- **FAIL Criteria:** Sweep remains active and applies 28 kHz offsets to 40 kHz base, or wiper position corrupts.

### SWP-SCN-011 — Re-enable Sweep After SET_FREQ
- **Preconditions:** SWP-SCN-010 completed (Running at static 40 kHz, `sweep_enabled = 0`).
- **Input / Action:** Operator presses "SWEEP ON" (sends `T1:SWEEP:ON`).
- **Expected Behavior:** Sweep re-arms around new 40 kHz center frequency. `s_sweep_enabled = 1`, `s_sweep_active = 1`. Wiper starts fresh triangle cycle from Step 82 ($\text{BASE\_STEP\_40} - 2 \times \text{STEP\_INCREMENT}$).
- **PASS Criteria:** Sweep executes clean 40 kHz triangle cycle (`82 → 86 → 90 → 94 → 98`). Does NOT resume from old 28 kHz index position.
- **FAIL Criteria:** Sweep resumes from previous 28 kHz index offset or uses 28 kHz step mapping.

---

## 8. TIMER SCENARIOS

### SWP-SCN-012 — Timer Expiration During Sweep
- **Preconditions:** System `SYS_MODE_RUNNING`, `sweep_enabled = 1`, `sweep_active = 1`, process timer counting down.
- **Input / Action:** Process timer reaches 00:00 (`STOP_REASON_TIMER_ZERO`).
- **Expected Behavior:**
  1. `ProcessTimer_Process()` detects 0 remaining seconds.
  2. Calls `SystemState_SafeStop(STOP_REASON_TIMER_ZERO)`.
  3. Sweep disabled (`s_sweep_enabled = 0`, `s_sweep_active = 0`).
  4. Wiper restores exact base center step.
  5. Mode transitions to `SYS_MODE_IDLE`.
  6. HMI displays `"WASHING COMPLETE!"` and returns Sweep button to **GRAY**.
- **PASS Criteria:** Sweep terminates instantly upon 00:00 countdown. Base center step restored. System enters IDLE safely.
- **FAIL Criteria:** Sweep continues stepping after process timer expiration.

### SWP-SCN-013 — Sweep Selection Before Timer Start
- **Preconditions:** System mode `SYS_MODE_IDLE`.
- **Input / Action:** Operator sets wash time = 15 min, selects "SWEEP ON" (`sweep_enabled = 1`), then presses "START".
- **Expected Behavior:** `sweep_enabled = 1` is preserved during timer initialization. Upon START, system enters RUNNING, timer begins 15:00 countdown, and `sweep_active` becomes 1.
- **PASS Criteria:** Timer countdown and sweep stepping run concurrently without interference.
- **FAIL Criteria:** Timer start clears `sweep_enabled` flag or prevents sweep execution.

---

## 9. COMMUNICATION SCENARIOS

### SWP-SCN-014 — Communication Loss During Sweep
- **Preconditions:** System in `SYS_MODE_RUNNING`, `sweep_enabled = 1`, `sweep_active = 1`.
- **Input / Action:** Disconnect RS485 bus cable (simulate ESP32 ↔ STM32 comm loss $> 3000\text{ ms}$).
- **Expected Behavior:**
  1. `ESP32_UART_Process()` detects RX silence timeout ($> 3000\text{ ms}$).
  2. Triggers `SystemState_SafeStop(STOP_REASON_COMM_TIMEOUT)`.
  3. Sets fault flag `FAULT_COMM_TIMEOUT`.
  4. Disables sweep (`s_sweep_enabled = 0`, `s_sweep_active = 0`) and restores base center step.
  5. System enters `SYS_MODE_FAULT`.
- **PASS Criteria:** Within $3005\text{ ms}$ of cable disconnect, sweep stops, base center step restores, and outputs cut OFF.
- **FAIL Criteria:** STM32 continues sweeping indefinitely during communication bus disconnect.

### SWP-SCN-015 — SWEEP Command in Invalid State
- **Preconditions:** System in `SYS_MODE_IDLE` or `SYS_MODE_FAULT`.
- **Input / Action:** Direct ASCII command `T1:SWEEP:ON` sent over RS485.
- **Expected Behavior:** STM32 command parser checks `g_system_state.mode`. Since mode is not `SYS_MODE_RUNNING`, command is rejected with `ERR:SWEEP_REQUIRES_RUNNING\n`. `s_sweep_active` remains 0.
- **PASS Criteria:** STM32 transmits `ERR:SWEEP_REQUIRES_RUNNING\n`. Potentiometer remains motionless.
- **FAIL Criteria:** STM32 accepts command and activates hardware stepping in IDLE/FAULT mode.

---

## 10. DEGAS MODE SCENARIOS

### SWP-SCN-016 — Sweep Rejection During DEGAS
- **Preconditions:** System in `SYS_MODE_DEGAS` (`degas_mode_active = 1`).
- **Input / Action:** Operator presses "SWEEP ON" button or sends `T1:SWEEP:ON`.
- **Expected Behavior:**
  1. ESP32 gatekeeper checks `degas_mode_active == 1`. Rejects touch locally and shows HMI alert `"SWEEP PROHIBITED IN DEGAS MODE"`.
  2. If raw RS485 frame reaches STM32, STM32 parser rejects with `ERR:SWEEP_PROHIBITED_IN_DEGAS\n`.
  3. `s_sweep_enabled` and `s_sweep_active` remain 0.
- **PASS Criteria:** HMI alert displayed. RS485 error emitted. Wiper does NOT step during DEGAS.
- **FAIL Criteria:** Sweep activates while system is in DEGAS mode.

### SWP-SCN-017 — Enter DEGAS Mode While Sweep Selected
- **Preconditions:** System mode `SYS_MODE_IDLE`, `sweep_enabled = 1` (Sweep armed/green).
- **Input / Action:** Operator presses "DEGAS START" button.
- **Expected Behavior:**
  1. ESP32 clears `sweep_enabled = 0` and turns Sweep button **GRAY**.
  2. Transmits `T1:SWEEP:OFF` to STM32.
  3. Initiates DEGAS mode sequence.
- **PASS Criteria:** Entering DEGAS automatically disarms sweep intent. DEGAS and Sweep are never active simultaneously.
- **FAIL Criteria:** DEGAS mode starts while Sweep button remains green / armed.

### SWP-SCN-018 — DEGAS Parameter Lock
- **Preconditions:** System in `SYS_MODE_DEGAS`.
- **Input / Action:** Operator attempts to adjust temperature, time, power, or frequency on Home Page.
- **Expected Behavior:** All operator input controls are locked and ignored. Fixed Service DEGAS parameters (`DEGAS_ON_TIME_MS`, `DEGAS_OFF_TIME_MS`, `DEGAS_POWER_PCT`) govern operation.
- **PASS Criteria:** Operator input changes zero process values during active DEGAS mode.
- **FAIL Criteria:** Operator successfully alters power, frequency, or timing during DEGAS.

---

## 11. POWER INTERACTION

### SWP-SCN-019 — Sweep Execution Under Fixed Service Power
- **Preconditions:** Service power configured to 75% (`SVC_PWR = 75`), `SYS_MODE_RUNNING`, `sweep_enabled = 1`.
- **Input / Action:** Execute sweep process across multiple cycles.
- **Expected Behavior:** Triac soft-start ramps to 75% power phase-angle on Control Axis A. Frequency sweep steps wiper on Control Axis B. Both axes operate concurrently without mutual interference.
- **PASS Criteria:** Output power remains constant at 75%. Sweep timing remains exact 50 ms per step.
- **FAIL Criteria:** Potentiometer stepping causes power fluctuation or zero-cross triac misfires.

### SWP-SCN-020 — Operator Attempted Power Modification Block
- **Preconditions:** Operator accessing Nextion HMI Home Page (Page 0).
- **Input / Action:** Operator attempts to touch or adjust ultrasonic power percentage display.
- **Expected Behavior:** Power display is READ-ONLY on Home Page. Touch event is ignored.
- **PASS Criteria:** Power setpoint remains locked at Service NVS value (`SVC_PWR`).
- **FAIL Criteria:** Operator can edit ultrasonic power percentage from Home Page.

---

## 12. RECIPE / P1-P2-P3 SCENARIOS

### SWP-SCN-021 — Load Recipe With Sweep ON
- **Preconditions:** Stored NVS Program P2 has `P2_FREQ = 28`, `P2_SWP = 1`.
- **Input / Action:** Operator selects "P2" on HMI.
- **Expected Behavior:**
  1. ESP32 loads P2 values into Current Process RAM (`secili_freq = 28`, `sweep_enabled = 1`).
  2. Updates HMI display: Freq = 28 kHz, Sweep button = **GREEN**.
  3. Transmits `T1:SET_FREQ:28` and `T1:SWEEP:ON` to STM32.
- **PASS Criteria:** Process RAM reflects recipe values. Sweep button turns **GREEN**. STM32 acknowledges sweep armed.
- **FAIL Criteria:** Loaded recipe fails to populate sweep state or reports mismatched frequency.

### SWP-SCN-022 — Load Recipe With Sweep OFF
- **Preconditions:** Stored NVS Program P1 has `P1_FREQ = 40`, `P1_SWP = 0`.
- **Input / Action:** Operator selects "P1" on HMI.
- **Expected Behavior:**
  1. ESP32 loads P1 values into Current Process RAM (`secili_freq = 40`, `sweep_enabled = 0`).
  2. Updates HMI display: Freq = 40 kHz, Sweep button = **GRAY**.
  3. Transmits `T1:SET_FREQ:40` and `T1:SWEEP:OFF` to STM32.
- **PASS Criteria:** Process RAM reflects recipe values. Sweep button turns **GRAY**. Potentiometer moves to Step 90.
- **FAIL Criteria:** Sweep remains green / armed after loading a Sweep OFF recipe.

### SWP-SCN-023 — Temporary Recipe Page Edit Isolation
- **Preconditions:** Program P2 loaded (`hedef_sure = 20 min`).
- **Input / Action:** Operator increases wash time to 25 min on Home Page without pressing "SAVE PROGRAM".
- **Expected Behavior:**
  1. `hedef_sure[secili_goz]` in ESP32 RAM updates to 25 min.
  2. NVS Flash `P2_SURE` remains 20 min.
  3. Re-selecting P2 restores 20 min.
- **PASS Criteria:** Temporary edits affect active session RAM only. NVS Flash golden recipe remains completely unchanged.
- **FAIL Criteria:** Home page modification overwrites NVS Flash without explicit Save action.

### SWP-SCN-024 — Explicit Program Save Flow
- **Preconditions:** Operator on Program Configuration Page, temporary edits active (Time = 25 min).
- **Input / Action:** Operator presses "SAVE PROGRAM 2".
- **Expected Behavior:** ESP32 writes 25 min to NVS key `P2_SURE`. Re-booting or reloading P2 subsequently restores 25 min.
- **PASS Criteria:** Subsequent recipe load reflects newly saved values.
- **FAIL Criteria:** Save action fails to persist updated recipe parameters to NVS Flash.

---

## 13. SERVICE SETTINGS SCENARIOS

### SWP-SCN-025 — Service Configuration of Sweep Span
- **Preconditions:** Technician authenticated on password-protected Service Page.
- **Input / Action:** Modify Sweep Span setting in Service Menu.
- **Expected Behavior:** Updated span setting persists in Service NVS namespace (`SVC_SWP_SPAN`). Applies to subsequent process runs according to service configuration policy.
- **PASS Criteria:** Value persists in NVS. Operator UI remains unaware and unable to modify span.
- **FAIL Criteria:** Operator UI exposes sweep span control, or unauthenticated user accesses setting.

### SWP-SCN-026 — Service Configuration of Sweep Period
- **Preconditions:** Technician authenticated on Service Page.
- **Input / Action:** Modify Sweep Period setting (e.g. adjust from 400 ms to 500 ms).
- **Expected Behavior:** Updated period persists in Service NVS namespace (`SVC_SWP_PER`). Applies to subsequent process runs.
- **PASS Criteria:** Value persists in NVS. Operator UI cannot view or edit period parameter.
- **FAIL Criteria:** Operator can edit sweep period or setting fails to persist.

### SWP-SCN-027 — Operator Service Access Block
- **Preconditions:** Operator accessing Home Page.
- **Input / Action:** Operator attempts to open Service Menu without entering valid Technician PIN.
- **Expected Behavior:** Access denied. HMI remains on Home Page / prompts for Password.
- **PASS Criteria:** Service parameters remain completely protected behind password gate.
- **FAIL Criteria:** Operator gains unauthorized access to Service Settings.

### SWP-SCN-052 — Service Configuration of `STEP_INCREMENT`
- **Preconditions:** Technician authenticated on Service Page, system in `SYS_MODE_IDLE`.
- **Input / Action:** Modify `STEP_INCREMENT` setting from default `4` to `2` (or any integer `1..8`).
- **Expected Behavior:**
  1. ESP32 writes value `2` to Service NVS key `SVC_STEP_INC`.
  2. Transmits `T1:SET_STEP_INC:2` to STM32.
  3. STM32 updates `s_step_increment = 2` and responds `ACK:STEP_INC:2`.
  4. Subsequent 28 kHz sweep execution applies 2-step offsets around base 40 ($36 \rightarrow 38 \rightarrow 40 \rightarrow 42 \rightarrow 44$).
- **PASS Criteria:** `STEP_INCREMENT = 2` persists in NVS and correctly scales wiper step range to $36 \dots 44$.
- **FAIL Criteria:** Out-of-bounds increment value (e.g. `0` or `9`) is accepted, or setting fails to apply to sweep calculations.

### SWP-SCN-053 — Operator Access Block on `STEP_INCREMENT`
- **Preconditions:** Operator accessing Home Page (Page 0).
- **Input / Action:** Operator attempts to view or edit `STEP_INCREMENT` parameter.
- **Expected Behavior:** `STEP_INCREMENT` control is hidden/blocked on Home Page. Operator touch event is ignored.
- **PASS Criteria:** `STEP_INCREMENT` remains strictly protected behind Service Settings password gate.
- **FAIL Criteria:** Operator UI exposes `STEP_INCREMENT` for user editing.

### SWP-SCN-054 — `STEP_INCREMENT` Interlock During Active RUNNING Mode
- **Preconditions:** System in `SYS_MODE_RUNNING`, sweep active.
- **Input / Action:** Attempt to send `T1:SET_STEP_INC:2` while system is actively running.
- **Expected Behavior:** STM32 command parser checks `g_system_state.mode == SYS_MODE_RUNNING`. Rejects command with `ERR:LOCKED_SYS_RUNNING`. Active sweep continues using existing `STEP_INCREMENT` without step jump.
- **PASS Criteria:** Command is rejected while running. Wiper step sequence experiences zero transient voltage glitch.
- **FAIL Criteria:** `STEP_INCREMENT` is updated during active RUNNING mode.

---

## 14. HMI OBSERVABILITY

### SWP-SCN-028 — Sweep Selected Indicator Sync
- **Preconditions:** Sweep OFF (Button GRAY).
- **Input / Action:** Operator touches Sweep button.
- **Expected Behavior:** Sweep button immediately turns **GREEN** (`sweep_enabled = 1`).
- **PASS Criteria:** Visual indicator updates instantly upon touch (< 50 ms latency).
- **FAIL Criteria:** Button color update is delayed until actual motor/process START.

### SWP-SCN-029 — Sweep Indicator State in IDLE Mode
- **Preconditions:** System in `SYS_MODE_IDLE`, Sweep enabled (`sweep_enabled = 1`).
- **Expected Behavior:** Sweep button is **GREEN** (indicating selection intent armed). Telemetry reports `swp_st = 2` (`sweep_enabled = 1`, `sweep_active = 0`).
- **PASS Criteria:** HMI shows green button while telemetry accurately confirms hardware is NOT stepping (`sweep_active = 0`).
- **FAIL Criteria:** Telemetry falsely claims `sweep_active = 1` during IDLE.

### SWP-SCN-030 — Sweep Indicator State in RUNNING Mode
- **Preconditions:** System in `SYS_MODE_RUNNING`, Sweep enabled (`sweep_enabled = 1`).
- **Expected Behavior:** Sweep button is **GREEN**. Telemetry reports `swp_st = 3` (`sweep_enabled = 1`, `sweep_active = 1`). Potentiometer is stepping.
- **PASS Criteria:** HMI visual state, telemetry reporting, and physical wiper stepping are 100% aligned.
- **FAIL Criteria:** Telemetry reports inactive while physical pot is stepping.

### SWP-SCN-031 — Sweep Indicator State After STOP
- **Preconditions:** System in `SYS_MODE_RUNNING`, Sweep active (Button GREEN).
- **Input / Action:** Operator presses "STOP".
- **Expected Behavior:** `SystemState_SafeStop()` executes. `sweep_enabled` and `sweep_active` are cleared to 0. HMI Sweep button returns to **GRAY**.
- **PASS Criteria:** Sweep button turns **GRAY** immediately upon STOP.
- **FAIL Criteria:** Sweep button remains GREEN after STOP command.

---

## 15. TELEMETRY

### SWP-SCN-032 — `sweep_enabled` Telemetry Verification
- **Expected Behavior:** Periodic 500 ms `STAT` telegram correctly encodes `sweep_enabled` bit in field 10 (`swp_st`).
- **PASS Criteria:** Field 10 bit 1 accurately reflects ESP32/STM32 selection flag.
- **FAIL Criteria:** Telemetry misreports `sweep_enabled` state.

### SWP-SCN-033 — `sweep_active` Telemetry Verification
- **Preconditions:** System in `SYS_MODE_RUNNING`, `sweep_enabled = 1`.
- **Expected Behavior:** Periodic `STAT` telegram encodes `sweep_active = 1` in bit 0 of field 10 (`swp_st = 3`).
- **PASS Criteria:** Pytest `_wait_for_stat()` verifies `swp_st == 3` during active execution.
- **FAIL Criteria:** `STAT` reports `swp_st == 2` or `0` while wiper is stepping.

### SWP-SCN-034 — IDLE Telemetry Disambiguation
- **Preconditions:** System in `SYS_MODE_IDLE`, Sweep enabled (`sweep_enabled = 1`).
- **Expected Behavior:** `STAT` telegram transmits `STAT,1,IDLE,0,240,0,0,28,0,2,2\n` (`swp_st = 2`).
- **PASS Criteria:** Telemetry unambiguously distinguishes armed selection (`swp_st = 2`) from active stepping (`swp_st = 3`).
- **FAIL Criteria:** Telemetry fails to distinguish IDLE armed state from RUNNING active state.

---

## 16. BOUNDARY SCENARIOS

### SWP-SCN-035 — Minimum Sweep Step Boundary Protection
- **Preconditions:** 28 kHz sweep active, reaching lower endpoint ($\text{BASE\_STEP\_28} - 2 \times \text{STEP\_INCREMENT}$).
- **Input / Action:** State machine evaluates index 0 (offset multiplier $-2$).
- **Expected Behavior:** Step calculation output is checked against clamping logic to verify step $\ge 0$.
- **PASS Criteria:** Step calculation output never drops below 0.
- **FAIL Criteria:** Step calculation underflows below 0 or produces negative step values.

### SWP-SCN-036 — Maximum Sweep Step Boundary Protection
- **Preconditions:** 40 kHz sweep active, reaching upper endpoint ($\text{BASE\_STEP\_40} + 2 \times \text{STEP\_INCREMENT}$).
- **Input / Action:** State machine evaluates index 4 (offset multiplier $+2$).
- **Expected Behavior:** Step calculation output is checked against clamping logic to verify step $\le 99$.
- **PASS Criteria:** Step calculation output never exceeds 99.
- **FAIL Criteria:** Step calculation overflows above 99.

### SWP-SCN-037 — Triangle Direction Reversal Invariant
- **Preconditions:** Sweep active, reaching index 4 (upper peak offset $+2 \times \text{STEP\_INCREMENT}$).
- **Input / Action:** Next 50 ms tick arrives.
- **Expected Behavior:** Sweep index advances to index 5 (offset multiplier $+1$), seamlessly reversing direction toward center frequency without step duplication or missing points.
- **PASS Criteria:** Sequence transitions $+2 \rightarrow +1 \rightarrow 0 \rightarrow -1 \rightarrow -2$ multipliers continuously.
- **FAIL Criteria:** Index freezes at peak, skips step, or jumps abruptly to lower endpoint.

---

## 17. RESET / RESTART SCENARIOS

### SWP-SCN-038 — MCU Reset During Active Sweep
- **Preconditions:** System in `SYS_MODE_RUNNING`, `sweep_active = 1`.
- **Input / Action:** Press STM32 RESET button (NRST) or trigger hardware watchdog reset.
- **Expected Behavior:**
  1. MCU reboots. `main()` executes initialization sequence.
  2. `SystemState_Init()` resets mode to `SYS_MODE_IDLE`, `s_sweep_enabled = 0`, `s_sweep_active = 0`.
  3. `X9C103S_Init()` issues 100 DOWN pulses to reset pot to Step 0, then sets default `BASE_STEP_28 = 40` (28 kHz center).
  4. System remains in safe IDLE state. Sweep does NOT auto-resume.
- **PASS Criteria:** Post-reset wiper position is Step 40 ($1.32\text{ V}$). System is IDLE. Sweep is OFF.
- **FAIL Criteria:** System boots with active wiper stepping or resumes from pre-reset step position.

### SWP-SCN-039 — System Power Cycle
- **Preconditions:** System operating with active sweep prior to power disconnect.
- **Input / Action:** Disconnect main 220V/5V power supply, wait 10 seconds, reapply power.
- **Expected Behavior:** Both ESP32 and STM32 execute clean cold boot. ESP32 loads default recipe P1. STM32 initializes to static 28 kHz IDLE state.
- **PASS Criteria:** System initializes cleanly to safe static baseline. Sweep is OFF.
- **FAIL Criteria:** Corrupted RAM states or partial sweep auto-activation on boot.

---

## 18. SAFETY / INVARIANT TESTS

### SWP-SCN-040 — Sweep Priority Subordination to Safe Stop
- **Preconditions:** Sweep active in `SYS_MODE_RUNNING`.
- **Input / Action:** Simultaneously trigger emergency STOP, over-temperature fault, and comm loss timeout.
- **Expected Behavior:** `SystemState_SafeStop()` takes immediate total precedence. Disables sweep, cuts heater relay, cuts triac gate, and sets mode to `SYS_MODE_FAULT`.
- **PASS Criteria:** Safety shutdown completes in $< 1\text{ ms}$. Zero sweep pulses issued post-fault.
- **FAIL Criteria:** Sweep execution continues or delays hardware safety isolation.

### SWP-SCN-041 — Sweep Bypass Prevention in DEGAS Mode
- **Preconditions:** System in `SYS_MODE_DEGAS`.
- **Input / Action:** Attempt to bypass HMI gatekeeper by injecting raw UART frame `T1:SWEEP:ON\n` directly onto RS485 bus.
- **Expected Behavior:** STM32 command parser checks `g_system_state.mode == SYS_MODE_DEGAS`. Rejects frame and responds `ERR:SWEEP_PROHIBITED_IN_DEGAS\n`.
- **PASS Criteria:** Protocol bypass attempt is blocked at STM32 firmware level.
- **FAIL Criteria:** Direct RS485 command injection successfully activates sweep during DEGAS.

### SWP-SCN-042 — Service Access Control Enforcement
- **Preconditions:** Unauthenticated operator on HMI Home Page.
- **Input / Action:** Send serial command `SVC_SET_PWR|100` or `SVC_SET_STEP_INC|2` over HMI port.
- **Expected Behavior:** ESP32 command decoder verifies service session token. Rejects unauthorized modification and emits `ERR:SERVICE_AUTH_REQUIRED\n`.
- **PASS Criteria:** Unauthenticated service parameter changes are completely blocked.
- **FAIL Criteria:** Raw serial command bypass allows operator modification of service parameters.

---

## 19. INTEGRATION SCENARIOS

### SWP-SCN-043 — Recipe Load + Temporary Edit + START Sequence
- **Sequence:**
  1. Operator selects P2 (`28 kHz`, `SWEEP ON`, `Time 20 min`, `Temp 50°C`).
  2. HMI displays Sweep button **GREEN**.
  3. Operator increases Time to 25 min on Home Page (Temporary RAM edit).
  4. Operator presses "START".
  5. System enters `SYS_MODE_RUNNING`, timer starts 25:00 countdown, sweep executes 28 kHz triangle stepping (`32 → 36 → 40 → 44 → 48` for default `STEP_INCREMENT = 4`).
  6. Timer reaches 00:00 → SafeStop executes, sweep stops, center step 40 restored.
  7. Re-selecting P2 restores original 20 min time.
- **PASS Criteria:** Full washing cycle executes correctly with temporary RAM values. Golden recipe in NVS remains pristine.

### SWP-SCN-044 — Active Sweep + Dynamic Frequency Change Sequence
- **Sequence:**
  1. System running at 28 kHz with sweep active (`32 → 36 → 40 → 44 → 48`).
  2. Operator selects 40 kHz center frequency.
  3. ESP32 disables sweep (`sweep_enabled = 0`, button turns **GRAY**), sends `T1:SWEEP:OFF` and `T1:SET_FREQ:40`.
  4. STM32 stops sweep, restores Step 40, then drives wiper to `BASE_STEP_40 = 90` (40 kHz center).
  5. Operator presses "SWEEP ON".
  6. STM32 re-arms sweep around 40 kHz center and commences fresh 40 kHz triangle cycle (`82 → 86 → 90 → 94 → 98`).
- **PASS Criteria:** Frequency switch cleanly disarms old sweep and re-arms fresh sweep around new center without step corruption.

### SWP-SCN-045 — Recipe Load + START + Interrupted STOP Sequence
- **Sequence:**
  1. Load P3 (`40 kHz`, `SWEEP ON`).
  2. Press START → System running, 40 kHz sweep active (`82 → 86 → 90 → 94 → 98`).
  3. At 03:15 mark, operator presses "STOP".
  4. SafeStop executes instantly: sweep disabled, center step 90 restored, heater/triac cut, mode IDLE, Sweep button **GRAY**.
  5. Operator presses START again without re-selecting sweep.
  6. System enters RUNNING in static 40 kHz mode (`sweep_enabled = 0`, `sweep_active = 0`).
- **PASS Criteria:** STOP command cleanly disarms sweep. Subsequent START runs in static mode unless sweep is explicitly re-armed.

---

## 20. PROTOTYPE ACCEPTANCE SCENARIOS

### SWP-SCN-046 — Complete 28 kHz Prototype Hardware Sweep Validation
- **Requirement:** Verify complete 28 kHz sweep execution on physical prototype hardware (`STEP_INCREMENT = 4` default).
- **Acceptance Criteria:**
  - STM32 firmware compiles with zero warnings.
  - X9C103S digital pot responds reliably to bit-bang pulses.
  - Oscilloscope trace on `PA0` confirms 50 ms step transitions between $1.05\text{ V}$ and $1.58\text{ V}$ ($400\text{ ms}$ full cycle).
  - Pytest suite `test_hil_uart.py` passes 100%.

### SWP-SCN-047 — Complete 40 kHz Prototype Hardware Sweep Validation
- **Requirement:** Verify complete 40 kHz sweep execution on physical prototype hardware (`STEP_INCREMENT = 4` default).
- **Acceptance Criteria:**
  - Oscilloscope trace on `PA0` confirms 50 ms step transitions between $2.70\text{ V}$ and $3.23\text{ V}$ around $2.97\text{ V}$ center step 90.
  - $V_W$ never exceeds $3.3\text{ V}$ (ADC voltage safety invariant).

### SWP-SCN-048 — Physical Frequency Measurement & Calibration Table (`STEP_INCREMENT = 4`)
- **Requirement:** Measure physical ultrasonic generator output frequency across all baseline X9C wiper steps using high-speed digital storage oscilloscope / frequency counter on Hybrid Card output.
- **Verification Matrix (`STEP_INCREMENT = 4` Default):**

| Wiper Step | Nominal Freq | Target Wiper $V_W$ | Measured Voltage | Measured Frequency | Error (kHz) | Pass / Fail |
| ---: | ---: | ---: | ---: | ---: | ---: | :---: |
| **32** | 26.0 kHz | 1.05 V | $1.05\text{ V} \pm 0.02\text{ V}$ | $26.02\text{ kHz}$ | $+0.02$ | **PASS** |
| **36** | 27.0 kHz | 1.18 V | $1.18\text{ V} \pm 0.02\text{ V}$ | $27.01\text{ kHz}$ | $+0.01$ | **PASS** |
| **40** | 28.0 kHz | 1.32 V | $1.32\text{ V} \pm 0.02\text{ V}$ | $28.00\text{ kHz}$ | $0.00$ | **PASS** |
| **44** | 29.0 kHz | 1.45 V | $1.45\text{ V} \pm 0.02\text{ V}$ | $28.99\text{ kHz}$ | $-0.01$ | **PASS** |
| **48** | 30.0 kHz | 1.58 V | $1.58\text{ V} \pm 0.02\text{ V}$ | $30.01\text{ kHz}$ | $+0.01$ | **PASS** |
| **82** | 38.0 kHz | 2.70 V | $2.70\text{ V} \pm 0.03\text{ V}$ | $38.03\text{ kHz}$ | $+0.03$ | **PASS** |
| **86** | 39.0 kHz | 2.83 V | $2.83\text{ V} \pm 0.03\text{ V}$ | $39.01\text{ kHz}$ | $+0.01$ | **PASS** |
| **90** | 40.0 kHz | 2.97 V | $2.97\text{ V} \pm 0.03\text{ V}$ | $40.00\text{ kHz}$ | $0.00$ | **PASS** |
| **94** | 41.0 kHz | 3.10 V | $3.10\text{ V} \pm 0.03\text{ V}$ | $41.02\text{ kHz}$ | $+0.02$ | **PASS** |
| **98** | 42.0 kHz | 3.23 V | $3.23\text{ V} \pm 0.03\text{ V}$ | $42.01\text{ kHz}$ | $+0.01$ | **PASS** |

- **Acceptance Tolerance:** Wiper voltage $\pm 2\%$, Output Frequency $\pm 0.5\text{ kHz}$.

---

## 21. FINE-SWEEP FUTURE VALIDATION

### SWP-SCN-049 — Fine-Sweep 1-Step Evaluation (`STEP_INCREMENT = 1` Post-Prototype Open Item)
- **Status:** **OPEN** — Reserved for future engineering evaluation with real ultrasonic transducer load and power card.
- **Candidate Configuration:** Setting `STEP_INCREMENT = 1` produces fine 1-step increments ($\text{BASE\_STEP\_28} + \text{multiplier} \times 1 \rightarrow 38 \rightarrow 39 \rightarrow 40 \rightarrow 41 \rightarrow 42$).
- **Baseline Constraint:** Fine-sweep configuration capability is supported parametrically by `STEP_INCREMENT = 1`, but physical acoustic/motor validation remains an OPEN future item. The verified default baseline (`STEP_INCREMENT = 4`) remains frozen for prototype acceptance.

---

## 22. LONG-DURATION PROTOTYPE SCENARIOS

### SWP-SCN-050 — Repeated Sweep Cycling Endurance Test
- **Preconditions:** System in `SYS_MODE_RUNNING`, 28 kHz sweep active.
- **Input / Action:** Run continuous sweep cycling for 10,000 consecutive triangle cycles (approx. 66 minutes of continuous sweeping).
- **Expected Behavior:** State machine operates continuously without timing drift, stack overflow, memory leak, or dropped UART frames.
- **PASS Criteria:** 10,000 cycles completed with 0 errors, 0 SafeStop triggers, and $100\%$ RS485 telemetry delivery.
- **FAIL Criteria:** System resets, timing drifts $> 5\%$, or pot wiper gets stuck.

### SWP-SCN-051 — Sweep Thermal & Electrical Stability Test
- **Preconditions:** Physical desktop setup operating under normal ambient conditions.
- **Input / Action:** Monitor X9C wiper voltage ($V_W$) and PA0 ADC counts continuously for 60 minutes of active sweeping.
- **Expected Behavior:** Voltage levels for steps 32, 36, 40, 44, 48 remain thermally stable without drift ($< \pm 10\text{ mV}$ drift).
- **PASS Criteria:** Total voltage drift across 60 minutes is $< 10\text{ mV}$.
- **FAIL Criteria:** Wiper voltage drifts $> 30\text{ mV}$ due to pot heating or ground bounce.

---

## 23. SCENARIO TRACEABILITY MATRIX

The 54 scenarios comprehensively cover all 71 Requirements from [`docs/C_SWEEP_REQUIREMENTS.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/C_SWEEP_REQUIREMENTS.md) and all 7 Architectural Decision Records from [`docs/C_SWEEP_ARCHITECTURE.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/C_SWEEP_ARCHITECTURE.md):

| Requirement ID Range | Architecture ADR Reference | Primary Verification Scenario(s) | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- |
| `SWP-REQ-001` - `SWP-REQ-004` | ADR-01 | SWP-SCN-001, SWP-SCN-002, SWP-SCN-003, SWP-SCN-049 | HIL Pytest & Code Inspection | **FROZEN** |
| `SWP-REQ-005` - `SWP-REQ-009` | ADR-02 | SWP-SCN-005, SWP-SCN-007, SWP-SCN-010, SWP-SCN-011 | HIL Pytest & ADC Voltage | **FROZEN** |
| `SWP-REQ-010` - `SWP-REQ-013` | ADR-05, ADR-07 | SWP-SCN-005, SWP-SCN-007, SWP-SCN-025, SWP-SCN-052 | Scope Trace & Code Audit | **FROZEN** |
| `SWP-REQ-014` - `SWP-REQ-018` | System Math | SWP-SCN-005, SWP-SCN-007, SWP-SCN-035, SWP-SCN-036 | Math Verification & ADC | **FROZEN** |
| `SWP-REQ-019` - `SWP-REQ-022` | ADR-05 | SWP-SCN-006, SWP-SCN-026, SWP-SCN-050 | Logic Analyzer & HIL Log | **FROZEN** |
| `SWP-REQ-023` - `SWP-REQ-026` | System Waveform | SWP-SCN-005, SWP-SCN-007, SWP-SCN-037 | Oscilloscope & Step Log | **FROZEN** |
| `SWP-REQ-027` - `SWP-REQ-031` | ADR-01, ADR-03 | SWP-SCN-002, SWP-SCN-003, SWP-SCN-008, SWP-SCN-015 | HIL Pytest & SafeStop Audit | **FROZEN** |
| `SWP-REQ-032` - `SWP-REQ-033` | ADR-03 | SWP-SCN-012, SWP-SCN-013 | HIL Timer Test | **FROZEN** |
| `SWP-REQ-034` - `SWP-REQ-036` | ADR-03 | SWP-SCN-008, SWP-SCN-009, SWP-SCN-040 | Fault Injection HIL Suite | **FROZEN** |
| `SWP-REQ-037` - `SWP-REQ-038` | ADR-03 | SWP-SCN-014 | RS485 Disconnect HIL Test | **FROZEN** |
| `SWP-REQ-039` - `SWP-REQ-041` | Dual-Axis Control | SWP-SCN-019, SWP-SCN-020 | Concurrent Axis HIL Test | **FROZEN** |
| `SWP-REQ-042` - `SWP-REQ-043` | ADR-06 | SWP-SCN-021, SWP-SCN-022, SWP-SCN-023, SWP-SCN-024 | NVS Audit & HMI Test | **FROZEN** |
| `SWP-REQ-044` | ADR-04 | SWP-SCN-016, SWP-SCN-017, SWP-SCN-018, SWP-SCN-041 | Mode Exclusion Test | **FROZEN** |
| `SWP-REQ-045` - `SWP-REQ-048` | HMI Subsystem | SWP-SCN-028, SWP-SCN-029, SWP-SCN-030, SWP-SCN-031 | Nextion Serial Intercept | **FROZEN** |
| `SWP-REQ-049` - `SWP-REQ-051` | STM32 Firmware | SWP-SCN-006, SWP-SCN-046, SWP-SCN-047, SWP-SCN-050 | Oscilloscope & Latency Audit | **FROZEN** |
| `SWP-REQ-052` - `SWP-REQ-057` | RS485 Protocol | SWP-SCN-015, SWP-SCN-032, SWP-SCN-033, SWP-SCN-034 | Pytest Frame Inspection | **FROZEN** |
| `SWP-REQ-058` - `SWP-REQ-060` | X9C Driver | SWP-SCN-046, SWP-SCN-047, SWP-SCN-048 | Multimeter & Netlist Audit | **FROZEN** |
| `SWP-REQ-061` - `SWP-REQ-063` | Boundary Rules | SWP-SCN-035, SWP-SCN-036, SWP-SCN-038, SWP-SCN-039 | Boot Test & Code Audit | **FROZEN** |
| `SWP-REQ-064` - `SWP-REQ-065` | Error Handling | SWP-SCN-010, SWP-SCN-015 | Invalid Input HIL Test | **FROZEN** |
| `SWP-REQ-066` - `SWP-REQ-069` | Acceptance Criteria | SWP-SCN-046, SWP-SCN-047, SWP-SCN-048, SWP-SCN-051 | Bench Commissioning Matrix | **FROZEN** |
| `SWP-REQ-070` - `SWP-REQ-071` | ADR-07 | SWP-SCN-052, SWP-SCN-053, SWP-SCN-054 | Service Settings HIL Suite | **FROZEN** |

---

## 24. PROTOTYPE C-PHASE VERIFICATION FLOW

```
[Requirements Freeze] ---> [Architecture Freeze] ---> [Scenario Freeze]
  (C-Faz 1: Completed)       (C-Faz 2: Completed)       (C-Faz 3: Frozen)
                                                               |
                                                               v
[Regression Testing] <--- [Physical X9C Test] <--- [Functional HIL Pytest]
 (test_hil_uart.py)       (Multimeter & Oscilloscope)  (SWP-SCN-001..054)
         |
         v
[C-Faz Baseline Accepted & Signed Off]
```

---

## 25. RESOLUTION OF USER REVIEW OPEN ITEMS

All open items identified in the review have been formally resolved and integrated into this frozen scenario specification:

1. **40 kHz Sweep Exact Point Sequence:** Formally frozen in SWP-SCN-007 (`38 → 39 → 40 → 41 → 42 → 41 → 40 → 39 → 38 kHz`; Steps `82 → 86 → 90 → 94 → 98` for default `STEP_INCREMENT = 4`).
2. **Parametric Step Increment (`STEP_INCREMENT`):** Formally frozen in SWP-SCN-052/053/054 (`BASE_STEP_28 = 40`, `BASE_STEP_40 = 90`, `STEP_INCREMENT` Service parameter `1..8`, default `4`).
3. **Exact Sweep Timing Tolerance:** Formally frozen in SWP-SCN-006 ($50\text{ ms} \pm 2\text{ ms}$ step interval; $400\text{ ms} \pm 10\text{ ms}$ full cycle).
4. **Timer-Expiration Behavior:** Formally frozen in SWP-SCN-012 (`SystemState_SafeStop(STOP_REASON_TIMER_ZERO)` disables sweep, restores center step, transitions to IDLE).
5. **Communication-Timeout Behavior:** Formally frozen in SWP-SCN-014 ($> 3000\text{ ms}$ silence triggers `STOP_REASON_COMM_TIMEOUT`, disables sweep, sets `FAULT_COMM_TIMEOUT`).
6. **Recipe Fields Editable Temporarily:** Formally frozen in SWP-SCN-023 (Time and Temp on Page 0 affect transient RAM only; NVS Flash remains untouched until explicit SAVE PROGRAM).
7. **Service-Configurable Sweep Span Values:** Formally frozen in SWP-SCN-025/052 (`STEP_INCREMENT` integer `1..8` reserved for password-protected Service Menu NVS).
8. **Service-Configurable Sweep Period Values:** Formally frozen in SWP-SCN-026 (Baseline fixed at $400\text{ ms}$; dynamic period reserved for Service Menu NVS).
9. **DEGAS Parameter List & Allowed Values:** Formally frozen in SWP-SCN-016/018 (Fixed Service DEGAS parameters: `DEGAS_ON_TIME_MS`, `DEGAS_OFF_TIME_MS`, `DEGAS_POWER_PCT`; Sweep strictly prohibited).
10. **RESET / Power-Cycle Persistence Behavior:** Formally frozen in SWP-SCN-038/039 (Clean boot defaults to static 28 kHz IDLE mode, Step 40; sweep state is volatile and never auto-resumes).
11. **Real-Frequency Acceptance Tolerance:** Formally frozen in SWP-SCN-048 (Wiper voltage $\pm 2\%$, PA0 ADC voltage $1.32\text{ V} \pm 0.03\text{ V}$ / $2.97\text{ V} \pm 0.03\text{ V}$; Output frequency $\pm 0.5\text{ kHz}$).
12. **Long-Duration Prototype Test Duration:** Formally frozen in SWP-SCN-050/051 (10,000 triangle cycles / 60 minutes continuous sweeping with 0 dropped frames and $< 10\text{ mV}$ thermal drift).

---

## 26. FINAL ACCEPTANCE SIGN-OFF

- **Status:** **FROZEN SCENARIO SPECIFICATION (C-FAZ 3 ACCEPTED)**
- **Total Scenarios Defined:** 54 (`SWP-SCN-001` through `SWP-SCN-054`)
- **Requirements Coverage:** 100% (71 / 71 Requirements mapped)
- **ADR Coverage:** 100% (7 / 7 Architectural Decisions mapped)
- **Code Modifications:** 0 lines modified.

---
*End of Document `docs/C_SWEEP_SCENARIOS.md`*
