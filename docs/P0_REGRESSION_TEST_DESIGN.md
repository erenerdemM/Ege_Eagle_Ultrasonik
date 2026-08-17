# EAGLEULTRASONİK — P0 REGRESSION TEST DESIGN SPECIFICATION

---

## 1. Executive Summary

This document specifies the pre-implementation regression test designs for Priority 0 (P0) safety vulnerabilities **RSK-001**, **RSK-002**, and **RSK-003**.

All test designs are mapped directly to the authoritative test suites:
- [`test_hil_uart.py`](file:///c:/Users/ern0e/EAGLEULTRASONiK/test_hil_uart.py) (Physical HIL / Ground-truth Verification)
- [`test_hmi_mock.py`](file:///c:/Users/ern0e/EAGLEULTRASONiK/test_hmi_mock.py) (ESP32 / Nextion HMI Protocol & Logic Mock)
- [`test_rs485_mock.py`](file:///c:/Users/ern0e/EAGLEULTRASONiK/test_rs485_mock.py) (RS485 Multi-drop Bus & STM32 Slave Mock)

No test files or firmware source files have been modified.

---

## 2. RSK-001 Regression Test Specifications (STOP Fault Retention)

### 2.1 Existing Baseline & False Pass Analysis
- **Existing Test:** [`test_hil_uart.py:test_04_stop_clears_fault`](file:///c:/Users/ern0e/EAGLEULTRASONiK/test_hil_uart.py#L408)
- **False Pass Reason:** Current test asserts `STOP` resets `fault_flags` to `0` under clean hardware conditions. It fails to test sending `STOP` while an active hardware sensor fault (e.g. PT100 open/short or ADC out-of-bounds) is continuously injected.

### 2.2 Required New Test Specifications

#### A. Physical HIL Suite ([`test_hil_uart.py`](file:///c:/Users/ern0e/EAGLEULTRASONiK/test_hil_uart.py))
* **Test Name:** `test_rsk001_active_hardware_fault_stop_rejection`
* **Test Procedure:**
  1. Inject active hardware sensor fault (e.g., PT100 probe open circuit). Confirm STM32 enters `SYS_MODE_FAULT` with `fault_flags != 0`.
  2. Send `T1:STOP` via `ESP32_PORT`.
  3. Read `STAT` telemetry frame on `STM32_PORT`. Assert `mode == FAULT` and `fault_flags != 0`.
  4. Immediately send `T1:START`.
  5. Assert response is `NACK:FAULT_ACTIVE` (or `ERR:FAULT_ACTIVE`), and confirm `mode` remains `FAULT`.
  6. Issue `T1:CLEAR_FAULT`. Assert `NACK:FAULT_PERSISTENT` while physical sensor fault remains.
  7. Restore physical sensor to valid range (-10°C to 110°C), send `T1:CLEAR_FAULT`. Assert `ACK:FAULT_CLEARED` and `mode == IDLE`.

#### B. HMI Mock Suite ([`test_hmi_mock.py`](file:///c:/Users/ern0e/EAGLEULTRASONiK/test_hmi_mock.py))
* **Test Name:** `test_rsk001_hmi_stop_under_active_fault`
* **Test Procedure:**
  1. Feed telemetry frame `STAT,1,FAULT,0,0,0,50,28,1` (`fault_flags = 1`).
  2. Execute `CMD_STOP`.
  3. Feed secondary telemetry with `fault_flags = 1` still active.
  4. Execute `CMD_START|10|50`.
  5. Assert `makine_calisiyor[1] == False`, `durum_metni` displays `"HATA!"`, and no `START` frame is emitted to `stm32_tx_log`.

#### C. RS485 Mock Suite ([`test_rs485_mock.py`](file:///c:/Users/ern0e/EAGLEULTRASONiK/test_rs485_mock.py))
* **Test Name:** `test_rsk001_rs485_node_fault_persistence_on_stop`
* **Test Procedure:**
  1. Set `MockSTM32Node.sys_mode = SYS_MODE_FAULT` with active hardware fault flag.
  2. Send `T1:STOP`.
  3. Assert `MockSTM32Node.sys_mode` remains `SYS_MODE_FAULT`.
  4. Send `T1:START`. Assert node does NOT transition to `SYS_MODE_RUNNING`.

---

## 3. RSK-002 Regression Test Specifications (UART Transmit Non-Blocking Architecture)

### 3.1 Existing Baseline & False Pass Analysis
- **Existing Test:** [`test_rs485_mock.py:test_rem_03_uart_tx_error_recovery_resumes_telemetry`](file:///c:/Users/ern0e/EAGLEULTRASONiK/test_rs485_mock.py#L801)
- **False Pass Reason:** Tests normal error callbacks in Python mock driver; does not test C-level spinlock polling when interrupts are masked or when hardware completion flags are dropped.

### 3.2 Required New Test Specifications

#### A. Physical HIL Suite ([`test_hil_uart.py`](file:///c:/Users/ern0e/EAGLEULTRASONiK/test_hil_uart.py))
* **Test Name:** `test_rsk002_hil_uart_spinlock_timeout_guard`
* **Test Procedure:**
  1. Send back-to-back command bursts over `ESP32_PORT` while forcing serial hardware lines to hold busy state.
  2. Monitor ST-Link VCP telemetry stream on `STM32_PORT`.
  3. Assert STM32 superloop tick counter advances continuously with zero core lockup or IWDG watchdog reset over a 5.0 second period.

#### B. RS485 Mock Suite ([`test_rs485_mock.py`](file:///c:/Users/ern0e/EAGLEULTRASONiK/test_rs485_mock.py))
* **Test Name:** `test_rsk002_rs485_spinlock_bounded_iteration_timeout`
* **Test Procedure:**
  1. Force `MockRS485Bus` direction pin stuck in `TX` mode or simulate stuck `tx_busy` status.
  2. Call transmit function.
  3. Assert execution completes within 10 ms (bounded iteration guard).
  4. Assert `tx_busy` is forcefully cleared to `False` and transceiver direction restores to `RX`.

---

## 4. RSK-003 Regression Test Specifications (Dual-Node Touch Lockout)

### 4.1 Existing Baseline & False Pass Analysis
- **Existing Test:** [`test_hmi_mock.py:test_degas_04_active_degas_locks_recipe_editing`](file:///c:/Users/ern0e/EAGLEULTRASONiK/test_hmi_mock.py#L1093)
- **False Pass Reason:** Tests touch lockout during `DEGAS` mode only; failed to test setpoint touch input handlers (`TIME_UP/DOWN`, `TEMP_UP/DOWN`, `GUC_UP/DOWN`, `CMD_FREQ`, `P1_SEL`) during normal `RUNNING` wash cycles.

### 4.2 Required New Test Specifications

#### A. Physical HIL Suite ([`test_hil_uart.py`](file:///c:/Users/ern0e/EAGLEULTRASONiK/test_hil_uart.py))
* **Test Name:** `test_rsk003_hil_setpoint_touch_lockout_in_running`
* **Test Procedure:**
  1. Start wash cycle on Tank 1 (`T1:START`), verify `mode == RUNNING`.
  2. Inject setpoint modification frames via `ESP32_PORT`: `T1:SET_TIME:30`, `T1:SET_TEMP:70`, `T1:SET_POWER:90`, `T1:SET_FREQ:40`.
  3. Monitor `STM32_PORT` telemetry stream.
  4. Assert STM32 ground truth active setpoints (`rem_sec`, `power_pct`, `frequency_khz`) remain strictly unchanged and `ERR:LOCKED_ACTIVE_MODE` is returned.

#### B. HMI Mock Suite ([`test_hmi_mock.py`](file:///c:/Users/ern0e/EAGLEULTRASONiK/test_hmi_mock.py))
* **Test Name:** `test_rsk003_hmi_running_cycle_touch_lockout_all_inputs`
* **Test Procedure:**
  1. Set `makine_calisiyor[1] = True` (Tank 1 wash cycle active).
  2. Record initial setpoints (`hedef_sure[1]`, `hedef_sicaklik[1]`, `guc_seviyesi`, `aktif_program`).
  3. Dispatch touch commands sequentially via `komutIsle()`:
     - `TIME_UP`, `TIME_DOWN`, `SET_TIME:45`
     - `TEMP_UP`, `TEMP_DOWN`, `SET_TEMP:80`
     - `GUC_UP`, `GUC_DOWN`
     - `CMD_FREQ|40`
     - `P1_SEL`, `P2_SEL`, `P3_SEL`
  4. Assert setpoints match initial values exactly (0 state mutation).
  5. Assert `stm32_tx_log` contains zero outbound setpoint commands.

---

## 5. Master Regression Test Matrix

| Risk ID | Suite File | Existing Baseline Test | New Required Test Name | Target Verification Focus |
| :--- | :--- | :--- | :--- | :--- |
| **RSK-001** | `test_hil_uart.py` | `test_04_stop_clears_fault` | `test_rsk001_active_hardware_fault_stop_rejection` | Physical hardware fault persistence on `STOP` & `CLEAR_FAULT` |
| **RSK-001** | `test_hmi_mock.py` | `test_03_cmd_stop_command` | `test_rsk001_hmi_stop_under_active_fault` | ESP32 HMI fault UI preservation & `CMD_START` rejection |
| **RSK-001** | `test_rs485_mock.py` | `test_22_rx_silence_watchdog_timeout` | `test_rsk001_rs485_node_fault_persistence_on_stop` | Layer 2 slave fault mode retention on `STOP` |
| **RSK-002** | `test_hil_uart.py` | `test_16_safety_watchdog_comm_loss` | `test_rsk002_hil_uart_spinlock_timeout_guard` | MCU main loop responsiveness during serial line stress |
| **RSK-002** | `test_rs485_mock.py` | `test_rem_03_uart_tx_error_recovery` | `test_rsk002_rs485_spinlock_bounded_iteration_timeout` | Bounded iteration loop forces `tx_busy = False` on timeout |
| **RSK-003** | `test_hil_uart.py` | `test_15_running_commissioning_rejection` | `test_rsk003_hil_setpoint_touch_lockout_in_running` | Mid-wash setpoint frame rejection (`ERR:LOCKED_ACTIVE_MODE`) |
| **RSK-003** | `test_hmi_mock.py` | `test_degas_04` | `test_rsk003_hmi_running_cycle_touch_lockout_all_inputs` | 100% touch edit lockout when `makine_calisiyor == True` |
| **RSK-003** | `test_rs485_mock.py` | `test_10_running_mode_interlock` | `test_rsk003_rs485_slave_running_setpoint_override_rejection` | Layer 2 setpoint mutation rejection in `SYS_MODE_RUNNING` |

---
*Document generated as part of Phase 13 P0 Remediation Design Review. Zero files modified.*
