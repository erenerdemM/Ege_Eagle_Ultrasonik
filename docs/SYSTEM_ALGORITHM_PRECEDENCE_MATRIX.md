# EAGLEULTRASONİK — SYSTEM ALGORITHM PRECEDENCE MATRIX & COLLISION ANALYSIS

---

## 1. System Precedence Hierarchy Map

The EAGLEULTRASONİK dual-node controller architecture derives its execution priority from 8 distinct operational levels:

$$\text{SafeStop} > \text{Fault} > \text{Running} > \text{Mode Interlocks} > \text{Process} > \text{Operator} > \text{Service} > \text{Telemetry}$$

```
 ┌─────────────────────────────────────────────────────────────────────────────┐
 │ LEVEL 1: SAFESTOP & HW LIFECYCLE (Master Emergency Disarm)                 │
 │ [SYS-SAFESTOP, SYS-RESET, SYS-WATCHDOG-HW, SYS-BOOT]                       │
 └──────────────────────────────────────┬──────────────────────────────────────┘
                                        ▼
 ┌─────────────────────────────────────────────────────────────────────────────┐
 │ LEVEL 2: FAULT AGGREGATION (Hardware & Comms Fault Supervision)            │
 │ [SYS-FAULT, COM-WATCHDOG, STM-PT100-ADC, STM-ZERO-CROSS, ESP-CONN-MON]     │
 └──────────────────────────────────────┬──────────────────────────────────────┘
                                        ▼
 ┌─────────────────────────────────────────────────────────────────────────────┐
 │ LEVEL 3: RUNNING MODE ENGINE (Global Mode Gate & UI Popups)                 │
 │ [SYS-STATE, HMI-OP-LOCKOUT, HMI-FAULT-POPUP]                               │
 └──────────────────────────────────────┬──────────────────────────────────────┘
                                        ▼
 ┌─────────────────────────────────────────────────────────────────────────────┐
 │ LEVEL 4: MODE INTERLOCKS & SAFETY GUARDS (Mutual Exclusions)                │
 │ [SAF-EXCLUSION, SAF-COMM-OFFLINE, SAF-PARAM-CLAMP]                         │
 └──────────────────────────────────────┬──────────────────────────────────────┘
                                        ▼
 ┌─────────────────────────────────────────────────────────────────────────────┐
 │ LEVEL 5: PROCESS CONTROL DRIVERS (PWM, Triac Phase, Sweep, Degas, Relay)    │
 │ [STM-TIM15-PWM, STM-TRIAC-PHASE, SWP-FREQ-SWEEP, DEG-PULSE-DEGAS, ...]      │
 └──────────────────────────────────────┬──────────────────────────────────────┘
                                        ▼
 ┌─────────────────────────────────────────────────────────────────────────────┐
 │ LEVEL 6: OPERATOR UI COMMANDS (User Intent & Touch Panel Inputs)            │
 │ [HMI-PAGE-HOME, HMI-RECIPE-P123, HMI-QUICK-WASH, HMI-FREQ-SEL]              │
 └──────────────────────────────────────┬──────────────────────────────────────┘
                                        ▼
 ┌─────────────────────────────────────────────────────────────────────────────┐
 │ LEVEL 7: SERVICE & PROVISIONING (Flash Identity & Passwords)                │
 │ [ID-STAGE, ID-ASSIGN, ID-RESET, ID-UID-DISC, ESP-SVC-AUTH, ...]             │
 └──────────────────────────────────────┬──────────────────────────────────────┘
                                        ▼
 ┌─────────────────────────────────────────────────────────────────────────────┐
 │ LEVEL 8: TELEMETRY & DIAGNOSTICS (Passive Monitoring & Logging)             │
 │ [COM-TELEMETRY, COM-DIAG, COM-UART-DRIVER, COM-RS485-DIR, ...]              │
 └─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Level-by-Level Dominance Rules & Evidence

| Level | Dominance Name | Dominant State / Action | Suppresses | Can Be Suppressed By | Code Evidence | Fail-Safe |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Level 1** | **SafeStop & HW Lifecycle** | Forces PC6 Triac Gate & PB15 Relay OFF; resets softstart delay to 9500 µs | All active processes, PWM output, heating, commands | Hardware Reset / Power Loss | `system_state.c:99-154`, `ultrasonic_pwm.c:71` | **YES** |
| **Level 2** | **Fault Aggregation** | Sets `fault_flags` bitmask; forces `mode = SYS_MODE_FAULT` | Normal process start, recipe edits, sweep | Level 1 SafeStop | `system_state.c:116`, `pt100_adc.c:64` | **YES** |
| **Level 3** | **Running Mode Engine** | Enforces global `SYS_MODE_RUNNING` / `SYS_MODE_DEGAS` mode state | Idle commands, provisioning, parameter re-configuration | Level 1, Level 2 | `esp32_uart.c:178`, `ekran_kontrol.ino:139` | **YES** |
| **Level 4** | **Mode Interlocks** | Blocks feature collisions (`SWEEP` during `DEGAS`, provisioning during `RUNNING`) | Invalid operator requests | Level 1, 2, 3 | `esp32_uart.c:196`, `ekran_kontrol.ino:140` | **YES** |
| **Level 5** | **Process Drivers** | Drives hardware outputs (TIM15 PWM, X9C wiper, PT100 ADC, Relay) | Static output states | Level 1, 2, 3, 4 | `ultrasonic_pwm.c:127`, `x9c103s.c:182` | **YES** |
| **Level 6** | **Operator Commands** | Converts HMI touch panel events to RS485 ASCII command frames | Default UI idle states | Level 1, 2, 3, 4 | `ekran_kontrol.ino:799-854` | **NO** |
| **Level 7** | **Service & Provisioning**| Writes Tank ID to Flash Page 127; manages service authentication | Unauthenticated configuration | Level 1, 2, 3, 4 | `main.c:132-194`, `ekran_kontrol.ino:1030` | **YES** |
| **Level 8** | **Telemetry & Diag** | Formats and transmits 10-field CSV telemetry frames over RS485 | Low-priority log operations | Level 1, 2, 3, 4, 5, 6, 7 | `esp32_uart.c:608-659` | **YES** |

---

## 3. Algorithm Collision Matrix

| Competing Algorithms | Triggering Condition | State Variables Involved | Explicit / Implicit | Dominant Algorithm | Risk Analysis |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`STOP` Cmd vs. Active Fault** | User sends `STOP` while PT100/ZC fault is active | `g_system_state.mode`, `fault_flags` | **Implicit** | `STOP` (Forces `IDLE` & clears `fault_flags`) | **RISK 1**: `STOP` clears `fault_flags` unconditionally; sending `START` next cycle starts machine before fault is re-checked. |
| **`ID-STAGE` / `ID-ASSIGN` vs. `SYS_MODE_DEGAS`** | Provisioning frame sent during active DEGAS mode | `g_system_state.mode`, `prov_state` | **Implicit** | Provisioning (STM32 checks `mode == RUNNING` only) | **RISK 2**: STM32 line 178 checks `mode == 1`, missing `mode == 3` (`DEGAS`); raw UART frame alters ID during DEGAS. |
| **Timer Zero vs. PT100 Fault** | Timer reaches 0 on exact cycle sensor fault occurs | `remaining_seconds`, `fault_flags` | **Implicit** | `Timer Zero` (Evaluates later in loop) | **RISK 3**: `STOP_REASON_TIMER_ZERO` sets `mode = SYS_MODE_IDLE`, overwriting `SYS_MODE_FAULT` set by ADC earlier in cycle. |
| **`SWEEP` vs. `DEGAS`** | Operator attempts to enable Sweep during Degas cycle | `swp_st`, `degas_active` | **Explicit** | `DEGAS` (Rejects `SWEEP` with error) | **SAFE**: Exclusively rejected with `ERR:SWEEP_PROHIBITED_IN_DEGAS`. |
| **Multi-Node Broadcast `T0:DISCOVER`** | 10 uncommissioned nodes receive discovery query | `MY_TANK_ID`, `prov_state` | **Explicit** | Slotted Delay (`CRC16 % 16 * 25ms`) | **SAFE**: Slotted backoff prevents RS485 bus collision. |
| **Multi-Node Broadcast `T0:GET_DIAG`** | 10 active nodes receive diagnostic query | `MY_TANK_ID`, `tank_id` | **Explicit** | Response Suppression | **SAFE**: Slaves suppress response when `tank_id == 0`. |
