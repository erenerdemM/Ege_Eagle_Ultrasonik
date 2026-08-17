# EAGLEULTRASONİK — PRIORITY 0 (P0) REMEDIATION DESIGN SPECIFICATION

---

## 1. Executive Summary

This document presents the authoritative technical remediation design for the **3 Priority 0 (P0)** safety-critical vulnerabilities identified in the EAGLEULTRASONiK architecture: **RSK-001**, **RSK-002**, and **RSK-003**.

This phase is **DESIGN-ONLY**. No firmware, application source code, or unit test files have been modified. The remediation designs specified herein enforce the project's 8-level precedence hierarchy:
$$\text{SafeStop (L1)} > \text{Fault (L2)} > \text{Running (L3)} > \text{Mode Interlocks (L4)} > \text{Process Drivers (L5)} > \text{Operator UI (L6)} > \text{Service (L7)} > \text{Telemetry (L8)}$$

---

## 2. RSK-001 Remediation Design (STOP Fault Retention & `CLEAR_FAULT` Command)

### 2.1 Problem Description & Root Cause
In [`esp32_uart.c:335`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L335) and [`system_state.c:106`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/system_state.c#L106), receiving `T<id>:STOP` calls `SystemState_SafeStop(STOP_REASON_USER_STOP)`, which unconditionally sets `mode = SYS_MODE_IDLE` AND clears `fault_flags = FAULT_NONE`. If `STOP` is sent while a hardware fault (e.g. PT100 open/short or zero-cross loss) is active, `fault_flags` is wiped to `0`. A subsequent `START` command will immediately transition the slave to `SYS_MODE_RUNNING` before sensor polling re-evaluates physical pins.

### 2.2 Detailed Remediation Design

#### A. Fault Retention in `SystemState_SafeStop()`
When `STOP_REASON_USER_STOP` occurs:
1. `STOP` **always** de-energizes all power hardware (`HeaterRelay_ForceOff()`, `TriacForceOff()`, softstart reset to `TRIAC_MAX_DELAY_US`, `remaining_seconds = 0`).
2. If `g_system_state.mode == SYS_MODE_FAULT`, `STOP` **retains** `SYS_MODE_FAULT` and **preserves** `g_system_state.fault_flags` (does NOT wipe them to `FAULT_NONE`).
3. If `g_system_state.mode != SYS_MODE_FAULT`, `STOP` transitions mode to `SYS_MODE_IDLE` and resets `g_system_state.fault_flags = FAULT_NONE`.

**Proposed Code Implementation for [`system_state.c:105-110`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/system_state.c#L105-L110)**:
```c
    case STOP_REASON_USER_STOP:
      /* Retain FAULT mode and fault flags if currently in fault state; otherwise transition to IDLE */
      if (g_system_state.mode != SYS_MODE_FAULT)
      {
        g_system_state.mode        = SYS_MODE_IDLE;
        g_system_state.fault_flags = FAULT_NONE;
      }
      g_system_state.remaining_seconds = 0u;
      break;
```

#### B. Explicit `CLEAR_FAULT` Command Protocol Extension
A dedicated `CLEAR_FAULT` ASCII command is added to the serial command parser in `esp32_uart.c`:
1. Receiving `T<id>:CLEAR_FAULT` when in `SYS_MODE_FAULT` re-evaluates current physical sensor readbacks (`g_system_state.current_temp_c`, zero-cross state).
2. If physical hardware remains out of valid bounds, `CLEAR_FAULT` is rejected with `NACK:FAULT_PERSISTENT\n` and `SYS_MODE_FAULT` is retained.
3. If physical hardware is within safe bounds, `fault_flags` is reset to `FAULT_NONE`, `mode` transitions to `SYS_MODE_IDLE`, and `ACK:FAULT_CLEARED\n` is returned.

**Proposed Code Implementation for [`esp32_uart.c:337`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L337)**:
```c
  else if (strcmp(cmd, "STOP") == 0)
  {
    SystemState_SafeStop(STOP_REASON_USER_STOP);
  }
  else if (strcmp(cmd, "CLEAR_FAULT") == 0 || strcmp(cmd, "FAULT_CLEAR") == 0)
  {
    if (g_system_state.mode == SYS_MODE_FAULT)
    {
      uint8_t persistent_hardware_fault = 0U;
      if ((g_system_state.fault_flags & (FAULT_PT100_OPEN | FAULT_PT100_SHORT)) != 0U)
      {
        if (g_system_state.current_temp_c < -20.0f || g_system_state.current_temp_c > 150.0f)
        {
          persistent_hardware_fault = 1U;
        }
      }

      if (persistent_hardware_fault != 0U)
      {
        const char *err_msg = "NACK:FAULT_PERSISTENT\n";
        RS485_Transmit_Blocking((const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
        HAL_UART_Transmit(&hlpuart1, (const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      }
      else
      {
        g_system_state.fault_flags = FAULT_NONE;
        g_system_state.mode        = SYS_MODE_IDLE;
        const char *ack_msg = "ACK:FAULT_CLEARED\n";
        RS485_Transmit_Blocking((const uint8_t *)ack_msg, (uint16_t)strlen(ack_msg), 10);
        HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ack_msg, (uint16_t)strlen(ack_msg), 10);
        ESP32_UART_SendStatus();
      }
    }
    else
    {
      const char *ack_msg = "ACK:NO_FAULT\n";
      RS485_Transmit_Blocking((const uint8_t *)ack_msg, (uint16_t)strlen(ack_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)ack_msg, (uint16_t)strlen(ack_msg), 10);
    }
  }
```

### 2.3 State Machine Transition Matrix

| Initial State | Event / Command Received | Action Executed | Next State | Next `fault_flags` |
| :--- | :--- | :--- | :--- | :--- |
| `SYS_MODE_RUNNING` | `STOP` | Cut Relay & Triac, clear remaining seconds | `SYS_MODE_IDLE` | `FAULT_NONE` |
| `SYS_MODE_FAULT` | `STOP` | Cut Relay & Triac, clear remaining seconds | **`SYS_MODE_FAULT`** | **Retained** |
| `SYS_MODE_FAULT` | `START` | Return `NACK:FAULT_ACTIVE` | `SYS_MODE_FAULT` | Retained |
| `SYS_MODE_FAULT` | `CLEAR_FAULT` (HW safe) | Clear flags, update telemetry status | `SYS_MODE_IDLE` | `FAULT_NONE` |
| `SYS_MODE_FAULT` | `CLEAR_FAULT` (HW unsafe)| Return `NACK:FAULT_PERSISTENT` | `SYS_MODE_FAULT` | Retained |

---

## 3. RSK-002 Remediation Design (Non-Blocking RS485 Transmit Architecture)

### 3.1 Problem Description & Root Cause
In [`esp32_uart.c:69-83`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L69-L83), `RS485_Transmit_Blocking()` contains two unbounded spinloops:
1. `while (tx_busy)`
2. `while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_TC) == RESET)`

If called when interrupts are masked or when UART hardware experiences a dropped completion interrupt, these spinloops block CPU execution permanently until the 1000ms hardware IWDG watchdog trips.

### 3.2 Detailed Remediation Design

Refactor `RS485_Transmit_Blocking()` to enforce **strict tick-based timeout bounds** (max 10 ms) on both wait loops, ensuring that RS485 direction control (`RS485_RX_ENABLE()`) and `tx_busy` flag are guaranteed to release even if transmission fails.

**Proposed Code Implementation for [`esp32_uart.c:69-83`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L69-L83)**:
```c
static void RS485_Transmit_Blocking(const uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
  uint32_t start_tick = HAL_GetTick();

  /* Guard 1: Timeout-bounded check on concurrent background IT transmission */
  while (tx_busy)
  {
    if ((HAL_GetTick() - start_tick) >= Timeout)
    {
      /* Force-unlock tx_busy to prevent main superloop deadlock */
      tx_busy = 0;
      g_bus_diag.tx_nack_count++;
      break;
    }
  }

  RS485_TX_ENABLE();
  tx_busy = 1;

  HAL_StatusTypeDef status = HAL_UART_Transmit(&huart3, (uint8_t *)pData, Size, Timeout);

  /* Guard 2: Timeout-bounded wait for UART Transmission Complete (TC) flag */
  uint32_t tc_start = HAL_GetTick();
  while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_TC) == RESET)
  {
    if ((HAL_GetTick() - tc_start) >= Timeout)
    {
      g_bus_diag.tx_nack_count++;
      break; /* Timeout reached; exit loop to restore bus RX mode */
    }
  }

  /* Always restore RS485 transceiver to RX mode and release TX lock */
  RS485_RX_ENABLE();
  tx_busy = 0;

  if (status != HAL_OK)
  {
    g_bus_diag.tx_nack_count++;
  }
}
```

---

## 4. RSK-003 Remediation Design (Dual-Node Active Process Touch Lockout)

### 4.1 Problem Description & Root Cause
In [`ekran_kontrol.ino:L1046-L1109`](file:///c:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino#L1046-L1109), Nextion touch handlers (`TIME_UP/DOWN`, `TEMP_UP/DOWN`, `GUC_UP/DOWN`, `CMD_FREQ`, `P1_SEL`) evaluated `if (degas_active[secili_goz]) return;`, but **omitted `makine_calisiyor[secili_goz]`**. Touch edits during normal wash cycles mutate local ESP32 setpoint memory and transmit UART setpoint frames while transducers are energized. Furthermore, `esp32_uart.c` on STM32 accepted setpoint updates in `SYS_MODE_RUNNING`.

### 4.2 Detailed Remediation Design (Dual-Node Architecture)

#### Node 1: Primary HMI Input Guarding (ESP32 - [`ekran_kontrol.ino`](file:///c:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino))
Update all operational touch handlers to check:
$$\text{IsLocked}(g) = \text{makine\_calisiyor}[g] \lor \text{degas\_active}[g]$$

**Proposed Changes in [`ekran_kontrol.ino`](file:///c:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino)**:
```c
/* Update in komutIsle() for TIME, TEMP, POWER, FREQ, and RECIPE handlers */
if (makine_calisiyor[secili_goz] || degas_active[secili_goz])
{
    Serial.println("--> INPUT LOCKED: ACTIVE PROCESS IN PROGRESS");
    return; /* Block local memory edit and serial transmission */
}
```

#### Node 2: Secondary Interlock Guarding (STM32 - [`esp32_uart.c`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c))
Enforce Layer 2 setpoint rejection inside `ProcessLine()` when `g_system_state.mode == SYS_MODE_RUNNING` or `SYS_MODE_DEGAS`.

**Proposed Changes in [`esp32_uart.c:176`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L176)**:
```c
  if (g_system_state.mode == SYS_MODE_RUNNING || g_system_state.mode == SYS_MODE_DEGAS)
  {
    if (strncmp(cmd, "SET_TIME:", 9) == 0  ||
        strncmp(cmd, "SET_TEMP:", 9) == 0  ||
        strncmp(cmd, "SET_POWER:", 10) == 0 ||
        strncmp(cmd, "SET_FREQ:", 9) == 0)
    {
      const char *err_msg = "ERR:LOCKED_ACTIVE_MODE\n";
      RS485_Transmit_Blocking((const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      HAL_UART_Transmit(&hlpuart1, (const uint8_t *)err_msg, (uint16_t)strlen(err_msg), 10);
      return; /* REJECT IMMEDIATELY; NO PARAMETER MUTATION */
    }
  }
```

---

## 5. Cross-Subsystem Impact & Trade-Off Analysis

### 5.1 Cross-Subsystem Impact Matrix

| Subsystem | RSK-001 Remediation Impact | RSK-002 Remediation Impact | RSK-003 Remediation Impact |
| :--- | :--- | :--- | :--- |
| **STM32** | Preserves `SYS_MODE_FAULT` until explicit `CLEAR_FAULT` re-checks ADC. | Hard-caps transmit delay to 10 ms; guarantees continuous superloop execution. | Rejects setpoint change commands in `RUNNING`/`DEGAS` modes. |
| **ESP32** | Displays fault notification until cleared via user confirmation. | Offline watchdog cleanly handles aborted TX frames without CPU lockup. | Blocks touch setpoint updates when `makine_calisiyor[g] == true`. |
| **HMI** | Renders fault state popup until physical fault is resolved. | Prevents screen freeze on serial bus loss. | Touch buttons visual feedback indicates locked state during wash cycle. |
| **RS485** | Adds `CLEAR_FAULT` ASCII command frame support. | Releases DE/RE direction pin on timeout, preventing bus contention lock. | Suppresses mid-wash setpoint frames, conserving bus bandwidth. |
| **Telemetry** | `STAT` telegram `fault_flags` accurately reflects hardware status. | Mirror output (LPUART1) remains active even during USART3 timeout. | Telemetry setpoints remain immutable throughout wash cycle. |

### 5.2 Remediation Design Options & Alternatives

| Defect ID | Smallest Reasonable Change | Safer Alternative (Recommended) | Complexity Impact | Regression Risk |
| :--- | :--- | :--- | :--- | :--- |
| **RSK-001** | Modify `STOP` to retain `SYS_MODE_FAULT` if `fault_flags != 0`. | Add explicit `CLEAR_FAULT` command with physical ADC sensor re-validation. | Very Low (+15 lines C) | Low (Preserves existing `SafeStop` hardware cuts) |
| **RSK-002** | Add basic 10,000 loop iteration counter in `RS485_Transmit_Blocking`. | Add tick-based timeout guards (`HAL_GetTick()`) + `RS485_RX_ENABLE()` recovery on timeout. | Low (+12 lines C) | Very Low (Fixes lockup without altering valid transmit flow) |
| **RSK-003** | Add `if (makine_calisiyor[secili_goz]) return;` in `ekran_kontrol.ino`. | Dual-node interlock: ESP32 HMI touch guard + STM32 `ProcessLine()` setpoint rejection. | Low (+20 lines total) | Very Low (Prevents mid-wash setpoint desynchronization) |

---

## 6. Backward Compatibility & Open Engineering Decisions

1. **Backward Compatibility:** All existing protocol framing, Tank ID addressing, DEGAS execution sequences, Frequency Sweep drivers, and NVS recipe structures remain 100% untouched and compatible.
2. **Open Engineering Decisions:**
   - **DEC-P0-01:** Should `CLEAR_FAULT` be auto-triggered on HMI page navigation or require a dedicated "Hata Sıfırla" button touch? *(Recommended: Dedicated HMI button touch or explicit `CLEAR_FAULT` serial command)*.

---
*Document generated as part of Phase 13 P0 Remediation Design Review. Zero files modified.*
