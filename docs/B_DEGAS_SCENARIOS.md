# EAGLEULTRASONİK — DEGAS OPERATING SCENARIOS SPECIFICATION (B-FAZ BASELINE FREEZE)

**Document ID:** `docs/B_DEGAS_SCENARIOS.md`  
**Project:** EAGLEULTRASONİK  
**Phase:** B-Faz Baseline DEGAS Scenarios Freeze  
**Target Hardware:** STM32G474RETx Master/Slave Nodes, ESP32-S3 Master, Nextion HMI  
**Status:** FROZEN SCENARIOS SPECIFICATION  
**Date:** 2026-08-17  

---

## 1. PURPOSE AND SCOPE

### 1.1 Purpose
This document formally defines, structures, and freezes the complete set of prototype-level **DEGAS (Liquid Degassing)** operating scenarios for the EAGLEULTRASONİK system. Built upon the frozen DEGAS Requirements ([`docs/B_DEGAS_REQUIREMENTS.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/docs/B_DEGAS_REQUIREMENTS.md)), Requirements Audit ([`docs/B_DEGAS_REQUIREMENTS_AUDIT.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/docs/B_DEGAS_REQUIREMENTS_AUDIT.md)), and Architecture ([`docs/B_DEGAS_ARCHITECTURE.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/docs/B_DEGAS_ARCHITECTURE.md)), these scenarios cover the complete lifecycle of degassing operations across all operational tiers.

### 1.2 Scope
The scope of this scenario specification covers:
- HMI Home Page arming, deselection, parameter disarming, and setpoint touch locking workflows.
- End-to-end DEGAS initiation, execution, countdown timing, and safe auto-stop completion to `SYS_MODE_IDLE`.
- Safety interlocks: user STOP, system faults (PT100, zero-cross loss, watchdog reset), communication timeouts (3000 ms silence), and MCU power-cycle recovery.
- Frequency Sweep mutual exclusion (`SWEEP:ON` rejection and sweep disarming).
- Service Settings 3-Page tabbed menu, NVS persistence (`service_degas`), and multi-tank addressable parameter targeting (`T1` .. `T10`).
- Optional DEGAS temperature control (`DEGAS_TEMPERATURE_CONTROL = ON/OFF`) and heater safety logic.
- Clear separation between **SOFTWARE/HIL VERIFIABLE** scenarios and **PHYSICAL CHARACTERIZATION — DEFERRED** parameters.

---

## 2. STATE MACHINE AND TRANSITION MATRIX

### 2.1 State Model Topology

```text
                  +-----------------------+
                  |     SYS_MODE_IDLE     |  (degas_armed = false, degas_active = false)
                  +-----------------------+
                              |
                              | Touch DEGAS Button on HMI Home Page
                              v
                  +-----------------------+
                  |      DEGAS ARMED      |  (degas_armed = true, degas_active = false)
                  +-----------------------+
                              |
                              |-- Touch Setpoint / Recipe / DEGAS again (Disarm -> IDLE)
                              |
                              | Touch START Button on HMI Home Page
                              v
                  +-----------------------+
                  |    SYS_MODE_DEGAS     |  (degas_armed = false, degas_active = true)
                  +-----------------------+
                              |
                              |-- User STOP / Timer Zero / Hardware Fault / Comm Loss
                              v
                  +-----------------------+
                  |     SYS_MODE_IDLE     |  or  SYS_MODE_FAULT
                  +-----------------------+
```

### 2.2 Legal and Illegal State Transitions

| Initial State | Event / Trigger | Target State | Legal / Illegal | Architectural Rule / Interlock |
| :--- | :--- | :--- | :---: | :--- |
| `SYS_MODE_IDLE` | Touch DEGAS Button | `DEGAS ARMED` | **LEGAL** | Sets `degas_armed = true` in ESP32 RAM; toggles button visual highlight. |
| `DEGAS ARMED` | Touch DEGAS Button | `SYS_MODE_IDLE` | **LEGAL** | Sets `degas_armed = false`; restores normal button visual state. |
| `DEGAS ARMED` | Edit Time or Temp Setpoint | `SYS_MODE_IDLE` | **LEGAL** | Setpoint modification disarms DEGAS selection automatically. |
| `DEGAS ARMED` | Select Recipe P1/P2/P3 | `SYS_MODE_IDLE` | **LEGAL** | Recipe selection disarms DEGAS and loads recipe parameters. |
| `DEGAS ARMED` | Touch START Button | `SYS_MODE_DEGAS` | **LEGAL** | Transmits `START_DEGAS` payload to STM32; sets `degas_active = true`. |
| `SYS_MODE_IDLE` | Touch START (Unarmed) | `SYS_MODE_RUNNING` | **LEGAL** | Initiates normal ultrasonic washing process (`START`). |
| `SYS_MODE_RUNNING` | Touch DEGAS Button | `SYS_MODE_RUNNING` | **ILLEGAL** | Rejected locally by ESP32 (`isAnyTankRunning() == true`). |
| `SYS_MODE_DEGAS` | Touch DEGAS Button | `SYS_MODE_DEGAS` | **ILLEGAL** | Rejected locally; HMI touch handlers locked during `degas_active`. |
| `SYS_MODE_DEGAS` | Touch Setpoint Widget | `SYS_MODE_DEGAS` | **ILLEGAL** | Touch handler disabled; displays "DEGAS IN PROGRESS — LOCKED". |
| `SYS_MODE_DEGAS` | RS485 `SWEEP:ON` | `SYS_MODE_DEGAS` | **ILLEGAL** | STM32 rejects command with `ERR:SWEEP_PROHIBITED_IN_DEGAS`. |
| `SYS_MODE_DEGAS` | Touch STOP Button | `SYS_MODE_IDLE` | **LEGAL** | Executes `SystemState_SafeStop(STOP_REASON_USER_STOP)`. |
| `SYS_MODE_DEGAS` | Timer reaches 00:00 | `SYS_MODE_IDLE` | **LEGAL** | Executes `SystemState_SafeStop(STOP_REASON_TIMER_ZERO)`. |
| `SYS_MODE_DEGAS` | Comm Silence > 3000 ms | `SYS_MODE_FAULT` | **LEGAL** | Executes `SystemState_SafeStop(STOP_REASON_COMM_TIMEOUT)`. |

---

## 3. ARCHITECTURAL ANCHORS AND PARAMETER OWNERSHIP

### 3.1 Parameter Ownership & Protocol Model (ADR-DEG-08)

To eliminate ambiguous dual ownership or synchronization race conditions:
1. **Single Source-of-Truth:** Master ESP32-S3 NVS Flash under namespace `service_degas` is the single authoritative store for DEGAS configuration settings.
2. **Atomic Execution Snapshot Protocol:** Upon DEGAS initiation, the ESP32 transmits a single, atomic ASCII command carrying the full DEGAS parameter snapshot to the target STM32 node:
   ```
   T<ID>:START_DEGAS:<dur>:<pwr>:<freq>:<on>:<off>:<t_ctrl>:<t_target>\n
   ```
3. **Volatile Execution State:** STM32 receives the execution snapshot into volatile RAM for the active duration of `SYS_MODE_DEGAS`. STM32 does NOT write DEGAS parameters to Flash Page 127.

### 3.2 Timer Engine Semantics (ADR-DEG-05)

- **Single Shared Timer Engine:** A single physical timer engine (`ProcessTimer_Process()` in `process_timer.c`) is used for both normal washing and degassing.
- **Logical State Isolation:** The timer state variable `g_system_state.remaining_seconds` in STM32 RAM decrements once per 1000 ms tick for whichever mode is active (`SYS_MODE_RUNNING` or `SYS_MODE_DEGAS`). Upon reaching 00:00, the timer engine triggers SafeStop and returns mode to `SYS_MODE_IDLE`.

### 3.3 Prototype Initial Default Values (`PROTOTYPE INITIAL DEFAULT`)

The following values represent **unvalidated initial prototype defaults** for software and HIL testing. They are fully adjustable via the Service Settings menu:

| Parameter Key | Parameter Description | Prototype Initial Default | Service Range Bounds | NVS Storage Key |
| :--- | :--- | :---: | :---: | :--- |
| `DEGAS_DURATION` | DEGAS Process Duration | **15 min** | `1 .. 99 min` | `degas_duration` |
| `DEGAS_POWER` | Ultrasonic Power Percentage | **100 %** | `10 .. 100 %` | `degas_power` |
| `DEGAS_FREQUENCY` | Base Center Frequency | **28 kHz** | `28` or `40` kHz | `degas_freq` |
| `DEGAS_PULSE_ON` | Ultrasonic Firing Duration | **1000 ms** | `100 .. 10000 ms` | `degas_pulse_on` |
| `DEGAS_PULSE_OFF` | Ultrasonic Silent Duration | **500 ms** | `0 .. 10000 ms` (`0`=Continuous) | `degas_pulse_off` |
| `DEGAS_TEMP_CTRL` | Temperature Control Toggle | **OFF (0)** | `0` (OFF) or `1` (ON) | `degas_temp_ctrl` |
| `DEGAS_TARGET_TEMP`| Target Tank Temperature | **50.0 °C** | `20.0 .. 90.0 °C` | `degas_target_temp` |

### 3.4 Physical Characterization Boundary

All scenarios in this document explicitly differentiate between:
- **`SOFTWARE/HIL VERIFIABLE`:** Logic transitions, protocol matrix parsing, HMI arming/deselection, timer decrements, parameter locking, SafeStop execution, and NVS persistence.
- **`PHYSICAL CHARACTERIZATION — DEFERRED`:** Real-world fluid acoustic hydrophone pulse ratio optimization, degassing cavitation efficiency, transducer thermal heating limits, and chemical fluid solubility curves.

---

## 4. DETAILED OPERATING SCENARIOS TABLE

```
Status Values Legend:
- FROZEN: Software/HIL logic requirement fully frozen for implementation.
- OPEN / CHARACTERIZATION: Parametric tuning open for software/bench testing.
- DEFERRED PHYSICAL: Physical fluid/acoustic validation deferred to real hardware bench characterization.
```

| ID | Scenario | Preconditions | Action | Expected State | Expected Output | Safety Result | Verification Method | Status |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :---: |
| **DEG-SCN-001** | **DEGAS Button Selection / Arming** | System in `SYS_MODE_IDLE`, `degas_armed == false`. | Operator touches DEGAS button on HMI Home Page. | `degas_armed = true`, `SYS_MODE_IDLE`. | DEGAS button visual state toggled to highlighted/armed. | No outputs active. | HMI Mock Test | **FROZEN** |
| **DEG-SCN-002** | **DEGAS Button Deselection** | System in `SYS_MODE_IDLE`, `degas_armed == true`. | Operator touches DEGAS button again on Home Page. | `degas_armed = false`, `SYS_MODE_IDLE`. | DEGAS button visual state restored to normal. | No outputs active. | HMI Mock Test | **FROZEN** |
| **DEG-SCN-003** | **Normal Process Time Change Before START** | System in `SYS_MODE_IDLE`, `degas_armed == true`. | Operator edits normal process time setpoint on Home Page. | `degas_armed = false`, `SYS_MODE_IDLE`. | Time setpoint updated; DEGAS button automatically disarmed. | No outputs active. | HMI Touch Audit | **FROZEN** |
| **DEG-SCN-004** | **Normal Target Temp Change Before START** | System in `SYS_MODE_IDLE`, `degas_armed == true`. | Operator edits target temp setpoint on Home Page. | `degas_armed = false`, `SYS_MODE_IDLE`. | Temp setpoint updated; DEGAS button automatically disarmed. | No outputs active. | HMI Touch Audit | **FROZEN** |
| **DEG-SCN-005** | **Recipe Selection Before START** | System in `SYS_MODE_IDLE`, `degas_armed == true`. | Operator touches P1, P2, or P3 recipe button. | `degas_armed = false`, `SYS_MODE_IDLE`. | Recipe parameters loaded; DEGAS button automatically disarmed. | No outputs active. | HMI Recipe Audit | **FROZEN** |
| **DEG-SCN-006** | **START Execution With DEGAS Armed** | System in `SYS_MODE_IDLE`, `degas_armed == true`. | Operator presses START button on HMI Home Page. | `degas_active = true`, `SYS_MODE_DEGAS`. | Master sends `T<ID>:START_DEGAS:<params>`; HMI renders `DEGAS ACTIVE`. | Triac PWM soft-starts; Sweep forced OFF. | HIL Pytest (`test_hil_uart.py`) | **FROZEN** |
| **DEG-SCN-007** | **START Execution Without DEGAS Armed** | System in `SYS_MODE_IDLE`, `degas_armed == false`. | Operator presses START button on HMI Home Page. | `degas_active = false`, `SYS_MODE_RUNNING`. | Master sends `T<ID>:START`; HMI renders `RUNNING`. | Normal cleaning process initiated. | HIL Pytest | **FROZEN** |
| **DEG-SCN-008** | **Active DEGAS Parameter Locking** | System in `SYS_MODE_DEGAS`, `degas_active == true`. | Operator touches process time, temp, or recipe setpoint. | `SYS_MODE_DEGAS` unchanged. | Touch ignored; HMI displays "DEGAS IN PROGRESS — LOCKED". | DEGAS operation unperturbed. | HMI Mock Test | **FROZEN** |
| **DEG-SCN-009** | **User STOP During Active DEGAS** | System in `SYS_MODE_DEGAS`, `degas_active == true`. | Operator presses STOP button on HMI Home Page. | `degas_active = false`, `SYS_MODE_IDLE`. | Master sends `T<ID>:STOP`; HMI restores IDLE Home screen. | `SystemState_SafeStop(USER_STOP)` cuts Triac & Heater. | SafeStop HIL Test | **FROZEN** |
| **DEG-SCN-010** | **DEGAS Countdown Decrement** | System in `SYS_MODE_DEGAS`, timer loaded (15 min). | Superloop ticks every 1000 ms. | `SYS_MODE_DEGAS`. | `remaining_seconds` decrements by 1 per second; STAT telegram updates. | Triac pulse modulation active. | Timer HIL Test | **FROZEN** |
| **DEG-SCN-011** | **DEGAS Timer Zero Auto-Stop Completion** | System in `SYS_MODE_DEGAS`, `remaining_seconds == 1`. | Countdown timer reaches 00:00. | `degas_active = false`, `SYS_MODE_IDLE`. | Node calls `SafeStop(TIMER_ZERO)`; HMI displays "DEGAS COMPLETE". | Outputs cut; system returns to safe IDLE mode. | Timer Zero HIL Test | **FROZEN** |
| **DEG-SCN-012** | **No Automatic Cleaning Restart Post-DEGAS** | System transitions from `SYS_MODE_DEGAS` to `SYS_MODE_IDLE` at timer zero. | No user touch action. | `SYS_MODE_IDLE` remains active indefinitely. | HMI Home screen ready for operator; outputs off. | System stays safely in IDLE without auto-restart. | Safety Invariant Audit | **FROZEN** |
| **DEG-SCN-013** | **Zero-Cross Loss Fault During DEGAS** | System in `SYS_MODE_DEGAS`, PWM firing active. | Disconnect zero-cross signal (EXTI silent > 500 ms). | `SYS_MODE_FAULT`. | Node calls `SafeStop(FAULT)`; sets `FAULT_ZERO_CROSS_LOST`. | Triac & Heater forced OFF instantly. | Fault Injection Test | **FROZEN** |
| **DEG-SCN-014** | **PT100 Sensor Open Fault During DEGAS** | System in `SYS_MODE_DEGAS`. | Disconnect PT100 sensor (ADC full scale). | `SYS_MODE_FAULT`. | Node calls `SafeStop(SENSOR_FAULT)`; sets `FAULT_PT100_OPEN`. | Outputs cut instantly; fault flag logged. | Sensor Fault Test | **FROZEN** |
| **DEG-SCN-015** | **Hardware Watchdog Reset During DEGAS** | System in `SYS_MODE_DEGAS`. | Block superloop to trip IWDG hardware watchdog. | `SYS_MODE_FAULT` upon MCU boot. | Node reboots; `SystemState_Init()` logs `FAULT_WATCHDOG_RESET`. | Hardware reset cuts outputs; boots to FAULT. | Watchdog Timeout Test | **FROZEN** |
| **DEG-SCN-016** | **No Automatic Restart Post-Fault** | System in `SYS_MODE_FAULT` following DEGAS fault. | Operator clears fault or reboots MCU. | `SYS_MODE_IDLE`. | System requires explicit user START to run any process. | Automatic process restart prohibited. | Interlock Test | **FROZEN** |
| **DEG-SCN-017** | **Communication Silence Timeout During DEGAS** | System in `SYS_MODE_DEGAS`, active firing. | Sever RS485 bus cable (RX silence > 3000 ms). | `SYS_MODE_FAULT`. | Node calls `SafeStop(COMM_TIMEOUT)`; sets `FAULT_COMM_TIMEOUT`. | Triac PWM & Heater forced OFF. | Comm Silence Test | **FROZEN** |
| **DEG-SCN-018** | **MCU Reboot During Active DEGAS** | System in `SYS_MODE_DEGAS`. | Power cycle MCU or issue SWD reset (`reset run`). | `SYS_MODE_IDLE`. | Node boots cleanly; RAM state cleared; mode set to `SYS_MODE_IDLE`. | Volatile active DEGAS state cleared on reboot. | Reboot HIL Test | **FROZEN** |
| **DEG-SCN-019** | **Frequency Sweep Toggle Attempt During DEGAS** | System in `SYS_MODE_DEGAS`. | Operator or master issues `T<ID>:SWEEP:ON`. | `SYS_MODE_DEGAS` (Sweep remains OFF). | Node responds `ERR:SWEEP_PROHIBITED_IN_DEGAS\n`. | Sweep disarmed (`sweep_enabled = 0`). | Mutual Exclusion Test | **FROZEN** |
| **DEG-SCN-020** | **Center Frequency Selection Change In DEGAS** | System in `SYS_MODE_DEGAS` (28 kHz). | Master issues `T<ID>:SET_FREQ:40`. | `SYS_MODE_DEGAS` (40 kHz setpoint). | Node responds `ACK:SET_FREQ:40`; X9C pot updates wiper step to 90. | Static 40 kHz center applied without sweep. | Frequency HIL Test | **FROZEN** |
| **DEG-SCN-021** | **Normal Ultrasonic Power Edit Attempt** | System in `SYS_MODE_DEGAS`, power = 100%. | Operator attempts to edit normal power on Home Page. | `SYS_MODE_DEGAS` (Power remains 100%). | Touch blocked; normal power setting isolated from DEGAS. | Active DEGAS power unperturbed. | Parameter Isolation Test | **FROZEN** |
| **DEG-SCN-022** | **Normal Recipe Temp Edit Attempt** | System in `SYS_MODE_DEGAS`, Temp Ctrl = OFF. | Operator attempts to edit normal recipe temp. | `SYS_MODE_DEGAS` (Heater remains OFF). | Touch blocked; recipe temp isolated from DEGAS. | Heater Relay remains strictly OFF. | Thermal Isolation Test | **FROZEN** |
| **DEG-SCN-023** | **Service PIN Authentication Protection** | Operator attempts to open Service Menu Tab 3. | Touch DEGAS Service Settings tab without PIN. | Service menu locked. | Nextion prompts "ENTER SERVICE PIN"; requires `123456`. | Operator access denied. | HMI Security Test | **FROZEN** |
| **DEG-SCN-024** | **Service Settings Page 3 DEGAS Configuration** | Technician authenticated (PIN `123456`), Tank ID = 1. | Technician updates DEGAS Duration to 20 min in Service Page 3. | Service settings updated in ESP32 RAM. | ESP32 writes `degas_duration = 20` to NVS `service_degas`. | Configuration persisted for Tank 1. | NVS Key Test | **FROZEN** |
| **DEG-SCN-025** | **Service Settings Lockout During Active DEGAS**| System in `SYS_MODE_DEGAS` on Tank 1. | Technician attempts to edit Service DEGAS settings for Tank 1. | Edit rejected. | HMI displays "TANK 1 DEGAS ACTIVE — EDITING LOCKED". | Active DEGAS process parameters protected. | Service Lockout Test | **FROZEN** |
| **DEG-SCN-026** | **NVS DEGAS Settings Persistence Across Reboot** | Technician configured Duration = 20 min, Power = 80%. | Power cycle ESP32 Master controller. | ESP32 boots into `SYS_MODE_IDLE`. | `nvsYukle()` loads `degas_duration = 20`, `degas_power = 80`. | Saved Service configuration restored cleanly. | NVS Reboot Test | **FROZEN** |
| **DEG-SCN-027** | **Uninitialized / Corrupted NVS Fallback** | Erase ESP32 NVS Flash partition `service_degas`. | Power cycle ESP32 Master controller. | ESP32 boots into `SYS_MODE_IDLE`. | `nvsYukle()` detects missing keys; loads prototype defaults (15 min, 100%). | Default prototype values written back to NVS. | NVS Corruption Test | **FROZEN** |
| **DEG-SCN-028** | **Multi-Tank Targeted DEGAS Control** | Multi-drop bus with Tank 1 and Tank 2 connected. | Operator selects Tank 1 and initiates DEGAS. | Tank 1: `SYS_MODE_DEGAS`<br>Tank 2: `SYS_MODE_IDLE`. | Master sends `T1:START_DEGAS:<params>`; Tank 2 ignores frame. | Tank 1 runs DEGAS; Tank 2 remains in safe IDLE. | Multi-Tank HIL Test | **FROZEN** |
| **DEG-SCN-029** | **Multi-Tank Service Page Header Display** | Service Menu Tab 3 opened on HMI. | Select Tank ID = 2 on Service Header selector (`secili_goz`). | Service Page 3 displays Tank 2 DEGAS configuration. | Nextion header updates to `[ Target Tank ID: T2 ]`. | Edits target Tank 2 NVS parameters exclusively. | HMI Header Audit | **FROZEN** |
| **DEG-SCN-030** | **Simultaneous Multi-Tank State Isolation** | Multi-drop bus with Tank 1 and Tank 2 connected. | Tank 1 running `SYS_MODE_DEGAS`; Tank 2 running normal `SYS_MODE_RUNNING`. | Tank 1: `SYS_MODE_DEGAS`<br>Tank 2: `SYS_MODE_RUNNING`. | Master handles telemetry STAT for Tank 1 (DEGAS) and Tank 2 (RUNNING). | Tank state machines operate completely independently. | Multi-Tank HIL Test | **FROZEN** |
| **DEG-SCN-031** | **Continuous Firing Profile Execution** | `DEGAS_PULSE_OFF_MS = 0`, `SYS_MODE_DEGAS` active. | Triac PWM driver processes superloop iterations. | `SYS_MODE_DEGAS`. | Triac PWM remains continuously active for entire duration. | Continuous ultrasonic cavitation active. | Scope Waveform Trace | **FROZEN** |
| **DEG-SCN-032** | **Pulsed Firing Profile Execution** | `PULSE_ON = 1000 ms`, `PULSE_OFF = 500 ms`, active DEGAS. | Superloop polls `HAL_GetTick()` timing. | `SYS_MODE_DEGAS`. | Triac fires 1000 ms (`PULSE_ON`), cuts 500 ms (`PULSE_OFF`), repeating. | Soft-start delay reapplied on each pulse ON. | Scope Waveform Trace | **FROZEN** |
| **DEG-SCN-033** | **DEGAS Power Soft-Start Ramping** | System enters `SYS_MODE_DEGAS`, target power = 80%. | Triac phase delay initialized to `TRIAC_MAX_DELAY_US`. | `SYS_MODE_DEGAS`. | Phase delay ramps down smoothly to 80% target phase angle. | Prevents inrush current transients on startup. | Oscilloscope Trace | **FROZEN** |
| **DEG-SCN-034** | **Optional Temperature Control OFF (Default)** | `DEGAS_TEMPERATURE_CONTROL = OFF (0)`, DEGAS active. | PT100 senses temperature below normal setpoint. | `SYS_MODE_DEGAS`. | `HeaterRelay_Process()` calls `HeaterRelay_ForceOff()`. | Heater Relay strictly forced OFF. | Heater HIL Test | **FROZEN** |
| **DEG-SCN-035** | **Optional Temperature Control ON Execution** | `DEGAS_TEMPERATURE_CONTROL = ON (1)`, Target = 50 °C. | PT100 senses temperature = 42 °C during DEGAS. | `SYS_MODE_DEGAS`. | `HeaterRelay_Process()` energizes Heater Relay using 50 °C setpoint. | Thermal regulation active with guard timers. | Thermal HIL Test | **FROZEN** |
| **DEG-SCN-036** | **PT100 Over-Temp Protection With Temp Ctrl OFF**| `DEGAS_TEMPERATURE_CONTROL = OFF`, fluid temp = 85 °C. | PT100 ADC reads over-temperature threshold. | `SYS_MODE_FAULT`. | Node calls `SafeStop(FAULT)`; sets `FAULT_GENERAL`. | Outputs cut instantly regardless of Temp Ctrl toggle. | Over-Temp HIL Test | **FROZEN** |
| **DEG-SCN-037** | **STAT Telemetry Reporting During DEGAS** | System in `SYS_MODE_DEGAS`, rem_sec = 600. | STM32 transmits periodic 500 ms telemetry frame. | `SYS_MODE_DEGAS`. | Payload: `STAT,1,DEGAS,600,450,0,100,28,0,2,0\n`. | Mode reported as `DEGAS`; sweep bit = 0. | Telemetry Trace | **FROZEN** |
| **DEG-SCN-038** | **HMI Countdown Synchronization** | Periodic STAT packets received by ESP32 Master. | ESP32 parses `STAT` frame and updates HMI. | `SYS_MODE_DEGAS`. | Nextion updates countdown display text (`10:00`). | HMI reflects live slave countdown seconds. | HMI Sync Test | **FROZEN** |
| **DEG-SCN-039** | **RS485 Communication Recovery** | RS485 cable re-connected following SafeStop comm loss. | Master resumes periodic polling. | `SYS_MODE_FAULT` -> `SYS_MODE_IDLE`. | Node responds to `T<ID>:STOP` fault clear command; returns to IDLE. | Communication restored cleanly. | Comm Recovery Test | **FROZEN** |
| **DEG-SCN-040** | **DEGAS Duration Range Boundary Validation** | Technician enters Duration = 150 min in Service Page. | HMI input validation routine evaluates range bounds. | Value rejected. | HMI displays "OUT OF RANGE (1..99 MIN)"; retains previous value. | Invalid parameter rejected before NVS write. | Config Bounds Test | **FROZEN** |
| **DEG-SCN-041** | **DEGAS Power Range Boundary Validation** | Technician enters Power = 5% in Service Page. | HMI input validation evaluates min power limit. | Value clamped to 10%. | HMI clamps value to 10% min; writes 10 to NVS. | Prevents triac misfiring at ultra-low power. | Config Bounds Test | **FROZEN** |
| **DEG-SCN-042** | **DEGAS Target Temp Boundary Validation** | Technician enters Target Temp = 110 °C in Service Page. | HMI input validation evaluates max temp limit. | Value clamped to 90 °C. | HMI clamps value to 90 °C max; writes 90 to NVS. | Prevents boiling over-temperature setpoints. | Config Bounds Test | **FROZEN** |
| **DEG-SCN-043** | **RS485 Command Framing Malformed Payload** | Master sends malformed `T1:START_DEGAS:ABC:XYZ\n`. | STM32 command parser processes payload. | `SYS_MODE_IDLE`. | Node discards malformed frame; sends `NACK:START_DEGAS,ERR_PARAM\n`. | State machine unperturbed. | Malformed Frame Test | **FROZEN** |
| **DEG-SCN-044** | **RS485 Command Unicast Address Mismatch** | Node address = Tank 1. Master sends `T2:START_DEGAS:...`. | STM32 UART RX interrupt filters frame. | `SYS_MODE_IDLE`. | Node address check (`T2 != T1`) fails; frame silently dropped. | Non-target node unperturbed. | Address Filter Test | **FROZEN** |
| **DEG-SCN-045** | **Emergency Stop Cut During Active DEGAS** | System in `SYS_MODE_DEGAS`, high-voltage PWM active. | Emergency Stop hardware line cut / power lost. | `SYS_MODE_IDLE` on reset. | Hardware cuts Triac gate immediately; softstart ramp reset. | Hardware-level instant disarm. | Emergency Cut Test | **FROZEN** |
| **DEG-SCN-046** | **DEGAS Cavitation Pulse Ratio Optimization** | `PULSE_ON` and `PULSE_OFF` ratio bench tuning. | Hydrophone measures acoustic bubble collapse in fluid tank. | `SYS_MODE_DEGAS`. | Acoustic hydrophone trace logs gas dissolution rate. | Characterizes optimal acoustic pulse ratios. | Bench Hydrophone Test | **DEFERRED PHYSICAL** |
| **DEG-SCN-047** | **Degassing Power Thermal Heating Limit** | DEGAS active at 100% power for 15 minutes. | Thermocouple monitors transducer core temperature rise. | `SYS_MODE_DEGAS`. | Temperature rise logged vs duration. | Verifies transducer thermal safety margins. | Thermal Bench Test | **DEFERRED PHYSICAL** |
| **DEG-SCN-048** | **Fluid Degassing Chemical Solubility Curve** | Degassing executed across various fluid temperatures. | Dissolved oxygen (DO) meter measures gas PPM reduction. | `SYS_MODE_DEGAS`. | DO PPM reduction curve plotted vs time and temp. | Optimizes DEGAS target temperature thresholds. | Chemical DO Bench Test | **DEFERRED PHYSICAL** |
| **DEG-SCN-049** | **Tank Volume Fluid Duration Characterization** | DEGAS duration tested in 10L vs 50L fluid tanks. | DO meter tracks 95% gas clearance time. | `SYS_MODE_DEGAS`. | Clearance time recorded per tank volume liter. | Establishes recommended duration per tank size. | Bench Volume Test | **DEFERRED PHYSICAL** |

---

## 5. VERIFICATION STRATEGY AND TRACEABILITY MATRIX

### 5.1 Verification Tiers

```
+-----------------------------------------------------------------------+
| TIER 1: AUTOMATED UNIT & MOCK TESTS                                  |
| - Files: test_hmi_mock.py, test_rs485_mock.py                         |
| - Coverage: HMI arming/deselection, PIN auth, NVS persistence,        |
|   command payload framing, malformed packet rejection.                |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
| TIER 2: HARDWARE-IN-THE-LOOP (HIL) INTEGRATION SUITES                 |
| - Files: test_hil_uart.py                                             |
| - Coverage: STM32 state machine transitions, Process timer reload,    |
|   comm silence watchdog, SafeStop execution, STAT telemetry trace.    |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
| TIER 3: PHYSICAL BENCH ACOUSTIC CHARACTERIZATION                      |
| - Scope: Bench DO meter, hydrophone oscilloscope, thermal sensors.    |
| - Coverage: Acoustic pulse ratio optimization, degassing power limit, |
|   fluid temperature solubility curves, duration per tank volume.       |
+-----------------------------------------------------------------------+
```

---

## 6. SCENARIO SUMMARY METRICS

- **Total DEGAS Scenarios Defined:** 49 (`DEG-SCN-001` through `DEG-SCN-049`)
- **FROZEN Software / HIL Scenarios:** 45 (100% mapped to software logic and test suites)
- **DEFERRED PHYSICAL Characterization Scenarios:** 4 (Explicitly tracked for bench acoustic/fluid tuning)
- **Requirements Coverage:** 100% of all frozen DEGAS requirements ([`docs/B_DEGAS_REQUIREMENTS.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/docs/B_DEGAS_REQUIREMENTS.md)) and architecture decisions ([`docs/B_DEGAS_ARCHITECTURE.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/docs/B_DEGAS_ARCHITECTURE.md)) are traceably covered.

---
*End of Document `docs/B_DEGAS_SCENARIOS.md`*
