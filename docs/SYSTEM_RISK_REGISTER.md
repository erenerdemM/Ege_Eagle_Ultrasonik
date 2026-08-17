# EAGLEULTRASONİK — SYSTEM FORENSIC RISK REGISTER

---

## 1. Executive Summary

This document presents the complete forensic risk register for the EAGLEULTRASONiK dual-node system. All risks were identified through empirical runtime audit, code inspection, and architectural analysis. Per project rules, **zero code or configuration fixes were performed**; all items are reported for human engineering decision.

---

## 2. Risk Classification Summary

* **CRITICAL RISKS:** 2
* **HIGH RISKS:** 5
* **MEDIUM RISKS:** 6
* **LOW RISKS:** 2
* **TOTAL REGISTERED RISKS:** 15

---

## 3. Comprehensive Risk Register

### RSK-001: Unresolved Hardware Fault Bypass on `STOP` Command
* **Severity:** **CRITICAL**
* **Subsystem:** STM32 Firmware (`esp32_uart.c` & `system_state.c`)
* **Trigger:** Receiving an RS485 `STOP` command while an active hardware fault (e.g. PT100 open circuit or Zero-Cross lost) is active.
* **Failure Mode:** `SystemState_SafeStop(STOP_REASON_USER_STOP)` unconditionally sets `g_system_state.mode = SYS_MODE_IDLE` AND clears `fault_flags = FAULT_NONE`.
* **Consequence:** If a `START` command arrives within the same superloop cycle before `PT100_ADC_Process()` re-evaluates, the system transitions to `SYS_MODE_RUNNING` with an un-cleared hardware fault!
* **Current Mitigation:** ADC polling re-evaluates fault on subsequent loop iteration.
* **Recommended Next Action:** Modify `STOP` handler in `esp32_uart.c` to clear `fault_flags` only if the hardware fault condition is no longer present.

---

### RSK-002: Spinlock Deadlock in Blocking RS485 Transmit Function
* **Severity:** **CRITICAL**
* **Subsystem:** STM32 Firmware (`esp32_uart.c`)
* **Trigger:** Calling `RS485_Transmit_Blocking()` while `tx_busy == 1` from an interrupt handler or critical section where USART3 IRQ is masked.
* **Failure Mode:** Code spins indefinitely in `while (tx_busy)` waiting for `HAL_UART_TxCpltCallback` (executed in USART3 ISR) to clear `tx_busy`.
* **Consequence:** The MCU deadlocks, halting superloop execution and preventing hardware shutdown calls (`HeaterRelay_ForceOff()`, `TriacForceOff()`).
* **Current Mitigation:** Hardware IWDG watchdog resets MCU after 1000 ms timeout.
* **Recommended Next Action:** Add a non-blocking timeout counter to `while (tx_busy)` and force-clear `tx_busy` if timeout elapses.

---

### RSK-003: Critical Touch Lockout Omissions During Normal Washing Cycles
* **Severity:** **HIGH**
* **Subsystem:** ESP32 HMI Firmware (`ekran_kontrol.ino`)
* **Trigger:** Operator tapping time adjustments, temperature setpoints, power levels, or frequency selections on Nextion HMI during an active normal washing cycle (`makine_calisiyor == true`).
* **Failure Mode:** HMI handlers check `if (degas_active[secili_goz]) return;`, but **fail to check `makine_calisiyor[secili_goz]`**.
* **Consequence:** Operator can alter target time, target temperature, power level, or switch frequency between 28kHz and 40kHz while ultrasound transducers and relays are actively energized.
* **Current Mitigation:** None in ESP32; STM32 accepts parameter changes dynamically.
* **Recommended Next Action:** Update ESP32 touch handlers to block setpoint/recipe edits when `makine_calisiyor[secili_goz]` is `true`.

---

### RSK-004: ESP32 Master Response Blind Spot for STM32 ACK/NACK/ERR Responses
* **Severity:** **HIGH**
* **Subsystem:** ESP32 / STM32 Inter-Node RS485 Communication Protocol
* **Trigger:** STM32 slave returning an ACK, NACK, or ERR response (`ERR:LOCKED_SYS_RUNNING`, `ERR:SWEEP_PROHIBITED_IN_DEGAS`, `NACK,STAGE_ID,...`).
* **Failure Mode:** ESP32 background loop (`loop()`) **ONLY parses incoming lines starting with `STAT,`**.
* **Consequence:** All error and rejection responses sent by STM32 slaves are discarded. The ESP32 Master receives no notification when a command is rejected by a slave.
* **Current Mitigation:** None. ESP32 assumes command succeeded until telemetry sync occurs.
* **Recommended Next Action:** Expand `stmTelemetryIsle()` in `ekran_kontrol.ino` to parse `ACK`, `NACK`, and `ERR` frames and update UI alert state.

---

### RSK-005: Out-of-Bounds Buffer Read in Telemetry Formatting
* **Severity:** **HIGH**
* **Subsystem:** STM32 Firmware (`esp32_uart.c`)
* **Trigger:** `snprintf()` returning a formatted length greater than `TX_LINE_MAX` (64 bytes).
* **Failure Mode:** `len` is passed directly to `HAL_UART_Transmit_IT(&huart3, (uint8_t *)tx_line, (uint16_t)len)`.
* **Consequence:** `HAL_UART_Transmit_IT` reads past the end of the 64-byte `tx_line` buffer, transmitting out-of-bounds SRAM data over RS485.
* **Current Mitigation:** Standard telemetry string is ~58 bytes (under 64 bytes).
* **Recommended Next Action:** Clamp `len` to `(sizeof(tx_line) - 1)` before passing to `HAL_UART_Transmit_IT`.

---

### RSK-006: Asymmetric DEGAS Provisioning Interlock Between Master and Slave
* **Severity:** **HIGH**
* **Subsystem:** STM32 Firmware (`esp32_uart.c`)
* **Trigger:** Sending a raw provisioning frame (`STAGE_ID`, `ASSIGN_ID`) directly to STM32 over UART during `SYS_MODE_DEGAS`.
* **Failure Mode:** [esp32_uart.c:178](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L178) checks `if (g_system_state.mode == SYS_MODE_RUNNING)` only.
* **Consequence:** `SYS_MODE_DEGAS` (mode 3) is not blocked by line 178, allowing flash identity mutations during an active DEGAS cycle if sent via raw serial.
* **Current Mitigation:** ESP32 master blocks provisioning during DEGAS via `isAnyTankRunning()`.
* **Recommended Next Action:** Change check to `if (g_system_state.mode == SYS_MODE_RUNNING || g_system_state.mode == SYS_MODE_DEGAS)`.

---

### RSK-007: Unhandled Return Statuses of Critical HAL Drivers
* **Severity:** **HIGH**
* **Subsystem:** STM32 Firmware (`esp32_uart.c`, `ultrasonic_pwm.c`)
* **Trigger:** `HAL_UART_Receive_IT` returning `HAL_BUSY` or `HAL_ERROR` inside `HAL_UART_ErrorCallback`.
* **Failure Mode:** Return status is ignored without retry.
* **Consequence:** UART RX fails to re-arm, causing permanent loss of communication on the slave node until hardware watchdog resets MCU.
* **Current Mitigation:** 3000 ms comm watchdog trips `SafeStop` when serial communication ceases.
* **Recommended Next Action:** Add loop retry or force-reset USART3 peripheral if `HAL_UART_Receive_IT` returns error.

---

### RSK-008: Unauthenticated Administrative Configuration Commands on ESP32
* **Severity:** **MEDIUM**
* **Subsystem:** ESP32 HMI Firmware (`ekran_kontrol.ino`)
* **Trigger:** Receiving administrative commands (`CMD_SET_STEP_INC`, `CMD_SET_SWP_SPAN`, `CMD_SET_SWP_PER`, `GUC_UP`, `P_SAVE`) over serial bridge without service authentication.
* **Failure Mode:** Command handlers omit `g_service_authenticated` check.
* **Consequence:** Unauthenticated touch panel or serial inputs can overwrite global recipe defaults and sweep parameters in NVS.
* **Current Mitigation:** Service PIN required for node provisioning and discovery pages.
* **Recommended Next Action:** Wrap administrative configuration handlers with `isProvisioningAllowed()`.

---

### RSK-009: Stale UI Status Display on RS485 Disconnection
* **Severity:** **MEDIUM**
* **Subsystem:** ESP32 HMI Firmware (`ekran_kontrol.ino`)
* **Trigger:** RS485 communication line breaking during an active wash cycle.
* **Failure Mode:** ESP32 connection watchdog sets `stm_bagli[i] = false`, but **does NOT update `durum_metni[i]`**.
* **Consequence:** Nextion HMI continues to display `"YIKAMA DEVAM EDIYOR..."` indefinitely instead of warning operator that communication was lost.
* **Current Mitigation:** Status updates to `"Kart Yok!"` if operator manually taps START button.
* **Recommended Next Action:** Explicitly set `durum_metni[i] = "Kart Yok!"` inside the connection watchdog loop when node goes offline.

---

### RSK-010: Disabling Interrupts During Flash Page Erase Operations
* **Severity:** **MEDIUM**
* **Subsystem:** STM32 Firmware (`main.c`)
* **Trigger:** Tank ID assignment triggering Flash Bank 2 Page 127 erase.
* **Failure Mode:** `TankId_SaveAndVerifyOverride()` wraps `HAL_FLASHEx_Erase()` in `__disable_irq()` / `__enable_irq()`.
* **Consequence:** Interrupts are blocked for 20–40 ms, causing EXTI zero-cross edge misses and potential UART RX overflow.
* **Current Mitigation:** Provisioning interlocked to IDLE mode.
* **Recommended Next Action:** Avoid disabling global IRQs during flash page erase; rely on peripheral IRQ priority grouping.

---

### RSK-011: Multi-Word `g_system_state` Data Race Between Main Loop and EXTI ISR
* **Severity:** **MEDIUM**
* **Subsystem:** STM32 Firmware (`system_state.h`, `ultrasonic_pwm.c`)
* **Trigger:** Zero-cross EXTI ISR reading `degas_config` struct while main loop updates it.
* **Failure Mode:** Non-atomic multi-word struct read in ISR context.
* **Consequence:** Risk of torn reads where zero-cross ISR executes based on partially updated degas parameters.
* **Current Mitigation:** Parameters updated sequentially in main loop.
* **Recommended Next Action:** Use double-buffered shadow structures or short critical sections during state updates.

---

### RSK-012: Unchecked Float-to-Integer Conversion in Telemetry String Generator
* **Severity:** **MEDIUM**
* **Subsystem:** STM32 Firmware (`esp32_uart.c`)
* **Trigger:** `current_temp_c` containing NaN, Inf, or out-of-range float values.
* **Failure Mode:** Direct cast `(int)(g_system_state.current_temp_c * 10.0f)` without range validation.
* **Consequence:** Undefined integer truncation in C standard.
* **Current Mitigation:** Sensor bounds check clamps float reading between -10.0 °C and +110.0 °C.
* **Recommended Next Action:** Add explicit float range check before integer cast.

---

### RSK-013: Lack of Checksum / CRC Validation on Standard ASCII Telemetry Frames
* **Severity:** **MEDIUM**
* **Subsystem:** Multi-Drop RS485 Communication Protocol
* **Trigger:** Electrical noise glitching characters on the RS485 bus.
* **Failure Mode:** Frame parser checks ASCII delimiters (`STAT,`, `\n`) but has no checksum verification on numeric setpoints.
* **Consequence:** Glitched numeric value (e.g. `SET_POWER:90` turning into `SET_POWER:900`) could be parsed if syntax appears valid.
* **Current Mitigation:** Bounds checking on numeric values clamps power to 100%.
* **Recommended Next Action:** Add optional CRC8 tail to control and telemetry frames.

---

### RSK-014: Service Session Inactivity Timer Never Refreshes During Active Setup
* **Severity:** **LOW**
* **Subsystem:** ESP32 HMI Firmware (`ekran_kontrol.ino`)
* **Trigger:** Service technician interacting with service pages for more than 5 minutes.
* **Failure Mode:** `service_auth_time` is set ONLY on initial password entry and never refreshed on subsequent touch events.
* **Consequence:** Active technician is logged out mid-configuration after 300,000 ms.
* **Current Mitigation:** Technician must re-enter PIN "123456".
* **Recommended Next Action:** Update `service_auth_time = millis()` on every valid service page touch event.

---

### RSK-015: Single-Byte RX Interrupt Overhead at 115200 Baud
* **Severity:** **LOW**
* **Subsystem:** STM32 Firmware (`esp32_uart.c`)
* **Trigger:** Heavy bus traffic at 115200 baud generating ~11,500 interrupts/second.
* **Failure Mode:** CPU cycles spent in single-byte ISR entry/exit.
* **Consequence:** Increased CPU overhead during continuous telemetry streaming.
* **Current Mitigation:** STM32G474 170 MHz core clock easily processes 11.5 kHz interrupts.
* **Recommended Next Action:** Upgrade USART3 RX to DMA circular buffer (`HAL_UART_Receive_DMA`).
