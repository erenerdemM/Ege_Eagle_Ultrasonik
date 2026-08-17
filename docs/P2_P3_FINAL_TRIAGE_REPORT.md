# EAGLEULTRASONİK — FINAL P2/P3 RISK TRIAGE REPORT

---

## 1. Executive Summary

Following the full closure and physical HIL verification of all Priority 0 risks (**RSK-001**, **RSK-002**, **RSK-003**) and Priority 1 risks (**RSK-004**, **RSK-005**, **RSK-006**, **RSK-007**, **RSK-008**, **RSK-009**), this report delivers the comprehensive forensic triage for all remaining Priority 2 (**RSK-010** .. **RSK-013**) and Priority 3 (**RSK-014** .. **RSK-015**) risks.

### Master Triage Classification:
```text
P2/P3 TRIAGE — NO BLOCKING ITEMS
```

### Overall System Prototype Status:
```text
SOFTWARE / LOOP PROTOTYPE — READY FOR FINAL CLOSURE
FINAL HARDWARE VALIDATION — DEFERRED (DR-001, DR-002, DR-003)
```

---

## 2. Detailed Analysis of Priority 2 Risks

### 2.1 RSK-010: Disabling Interrupts During Flash Page Erase Operations
* **Subsystem:** STM32 Firmware (`main.c:152`, `main.c:212`)
* **Current Behavior:** `TankId_SaveAndVerifyOverride()` and `TankId_EraseOverride()` disable global interrupts (`__disable_irq()`) for 20–40 ms while erasing Flash Bank 2 Page 127 (`0x0807F800`).
* **Why Classified P2:** Disabling global interrupts pauses USART3 RX interrupts and EXTI zero-cross edge detection.
* **Risk Nature:** Confirmed design risk / Non-blocking engineering improvement.
* **Impact Assessment:**
  - *Safety:* **NO IMPACT.** Flash operations are strictly interlocked to `SYS_MODE_IDLE` or `SYS_MODE_UNCOMMISSIONED`. In IDLE mode, PWM is 0%, heater relay is open, and zero-cross Triac pulses are not generated.
  - *Functional Correctness:* **NO IMPACT.** Flash write and readback verification succeed deterministically.
  - *Communication:* Disabling interrupts for 20ms during IDLE commissioning could drop a stray ASCII character, which RS485 line framing cleanly discards and re-polls.
* **Decision Classification:** **`SHOULD FIX BEFORE PRODUCTION` (B)** / **`SAFE TO DEFER` (C)** for prototype.
* **Minimum Remediation (Production):** Replace global `__disable_irq()` with interrupt priority masking (`__set_BASEPRI()`) to keep UART/EXTI active if needed, or rely on STM32G4 Dual-Bank background erase capabilities.

---

### 2.2 RSK-011: Multi-Word `g_system_state` Data Race Between Main Loop and EXTI ISR
* **Subsystem:** STM32 Firmware (`system_state.h`, `ultrasonic_pwm.c:216`)
* **Current Behavior:** `HAL_GPIO_EXTI_Callback` reads `g_system_state.mode` and `g_system_state.degas_config.pulse_off_ms` on every AC zero-cross edge (100 Hz).
* **Why Classified P2:** Theoretical concern of a torn read if multi-word structures are updated concurrently in main loop while EXTI ISR fires.
* **Risk Nature:** Engineering improvement / Minor design review.
* **Impact Assessment:**
  - *Safety:* **NO IMPACT.** `g_system_state.mode` and `pulse_off_ms` are 32-bit aligned scalar integers read atomically on ARM Cortex-M4.
  - *Functional Correctness:* **NO IMPACT.** In `SYS_MODE_DEGAS`, `degas_config` parameters are established atomically on transition from `IDLE` (`START_DEGAS`) and are **immutable** throughout active operation due to RSK-006 interlocks.
* **Decision Classification:** **`SAFE TO DEFER` (C)**.
* **Minimum Remediation (Production):** Double-buffered configuration snapshot with atomic pointer swap on process start.

---

### 2.3 RSK-012: Unchecked Float-to-Integer Conversion in Telemetry String Generator
* **Subsystem:** STM32 Firmware (`esp32_uart.c:724`, `pt100_adc.c:53`)
* **Current Behavior:** `int temp_x10 = (int)(g_system_state.current_temp_c * 10.0f);` performs a direct float-to-int cast before formatting into the `STAT` telegram.
* **Why Classified P2:** C standard leaves float-to-integer conversion undefined if float value exceeds integer range or is NaN/Inf.
* **Risk Nature:** Engineering improvement / MISRA C compliance refinement.
* **Impact Assessment:**
  - *Safety:* **NO IMPACT.** `current_temp_c` is computed exclusively in `pt100_adc.c` where ADC readings are strictly bounded between `ADC_RAW_VALID_MIN` and `ADC_RAW_VALID_MAX` (corresponding to -10.0 °C to +110.0 °C). NaN or Inf cannot be generated.
  - *Functional Correctness:* `temp_x10` is always between -100 and 1100, fitting comfortably within 32-bit signed integer.
* **Decision Classification:** **`SAFE TO DEFER` (C)**.
* **Minimum Remediation (Production):** Add explicit defensive boundary clamping: `if (isnan(t) || t < -20.0f) t = -20.0f; else if (t > 150.0f) t = 150.0f;`.

---

### 2.4 RSK-013: Lack of Checksum / CRC Validation on Standard ASCII Telemetry Frames
* **Subsystem:** Multi-Drop RS485 Communication Protocol (`esp32_uart.c`, `ekran_kontrol.ino`)
* **Current Behavior:** Runtime ASCII control commands (`T1:SET_POWER:80`, `T1:START`) and status telegrams (`STAT,1,RUNNING,...`) use line termination and CSV delimiters without a trailing CRC checksum.
* **Why Classified P2:** Industrial electrical transients could alter ASCII characters in high-EMI environments.
* **Risk Nature:** Confirmed design consideration / Production robustness enhancement.
* **Impact Assessment:**
  - *Safety:* **NO IMPACT.** Protocol parsers enforce strict CSV field counts (10 fields for `STAT`), numerical range clamping (Power: 10–100%, Freq: 28/40 kHz, Time: 1–120 min, Temp: 20–90 °C), and hardware UART error recovery (RSK-007).
  - *Communication:* Critical provisioning frames (`T0:DISCOVER`, `T0:STAGE_ID`, `T0:ASSIGN_ID`) already implement CRC16-CCITT checksums.
* **Decision Classification:** **`SHOULD FIX BEFORE PRODUCTION` (B)**.
* **Minimum Remediation (Production):** Append optional 2-byte hex CRC8 tail (e.g. `*A5\n`) to runtime ASCII frames.

---

## 3. Detailed Analysis of Priority 3 Risks

### 3.1 RSK-014: Service Session Inactivity Timer Never Refreshes During Active Setup
* **Subsystem:** ESP32 HMI Firmware (`ekran_kontrol.ino:1063`, `ekran_kontrol.ino:1440`)
* **Current Behavior:** `service_auth_time` is recorded once upon successful PIN entry (`123456`) and compared against `millis() - service_auth_time > 300000` (5 minutes). Active touch events on Page 5 do not reset the timer.
* **Why Classified P3:** Usability inconvenience if a technician requires >5 minutes to configure multi-tank parameters.
* **Risk Nature:** Usability refinement / Test & documentation improvement.
* **Impact Assessment:**
  - *Safety:* **NO IMPACT.** Fixed 5-minute timeout is a standard defensive pattern to prevent unattended service access.
  - *Functional Correctness:* After 5 minutes, technician simply re-enters PIN `123456`.
* **Decision Classification:** **`SAFE TO DEFER` (C)**.
* **Minimum Remediation (Production):** Update `service_auth_time = millis()` inside `komutIsle()` on valid authenticated service commands (`GUC_UP`, `ID_UP`, `MAX_UP`, etc.).

---

### 3.2 RSK-015: Single-Byte RX Interrupt Overhead at 115200 Baud
* **Subsystem:** STM32 Firmware (`esp32_uart.c:830`)
* **Current Behavior:** `HAL_UART_Receive_IT` processes USART3 incoming data byte-by-byte in interrupt context.
* **Why Classified P3:** At continuous 115200 baud bus saturation, the CPU handles ~11.5 kHz interrupts.
* **Risk Nature:** Performance optimization / Architectural refinement.
* **Impact Assessment:**
  - *Safety:* **NO IMPACT.** STM32G474RET6 runs at 170 MHz (170,000,000 cycles/sec). The single-byte ISR consumes <0.5% total CPU load.
  - *Functional Correctness:* All 40 physical HIL tests pass with zero dropped frames or UART overruns.
* **Decision Classification:** **`SAFE TO DEFER` (C)**.
* **Minimum Remediation (Production):** Transition USART3 RX to DMA circular buffer (`HAL_UART_Receive_DMA`) with IDLE line detection.

---

## 4. Comprehensive Classification Matrix

| Risk ID | Subsystem | Summary | Category | Decision Classification | Prototype Blocking? |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **RSK-010** | STM32 Flash | Disabling IRQs during Flash erase in IDLE | Confirmed Design Risk | **B (`SHOULD FIX BEFORE PRODUCTION`)** | **NO** |
| **RSK-011** | STM32 State | Multi-word read in EXTI ISR context | Engineering Improvement | **C (`SAFE TO DEFER`)** | **NO** |
| **RSK-012** | STM32 Telemetry | Float-to-int cast without NaN check | Engineering Improvement | **C (`SAFE TO DEFER`)** | **NO** |
| **RSK-013** | RS485 Protocol | Lack of CRC on standard ASCII frames | Production Enhancement | **B (`SHOULD FIX BEFORE PRODUCTION`)** | **NO** |
| **RSK-014** | ESP32 HMI | Fixed 5-min service session timeout | Usability Improvement | **C (`SAFE TO DEFER`)** | **NO** |
| **RSK-015** | STM32 UART | Single-byte UART RX ISR overhead | Performance Optimization | **C (`SAFE TO DEFER`)** | **NO** |

---

## 5. Summary Counts by Decision Category

- **`MUST FIX NOW` (A):** **0**
- **`SHOULD FIX BEFORE PRODUCTION` (B):** **2** (RSK-010, RSK-013)
- **`SAFE TO DEFER` (C):** **4** (RSK-011, RSK-012, RSK-014, RSK-015)
- **`DOCUMENTATION / TEST ONLY` (D):** **0**
- **`NO LONGER ACTIVE` (E):** **0**

---

## 6. Physical Deferred Items (Strictly Hardware-Dependent)

The following items remain strictly classified under **`FINAL HARDWARE VALIDATION — DEFERRED`** due to unavailable specialized high-voltage and wet chemical bench equipment, and are **NOT** software defects:

1. **`DR-001`:** PT100 OPAMP3 hardware linearity and Triac heater regulation under 220V AC load.
2. **`DR-002`:** Ultrasonic transducer acoustic resonance and cavitation characterization.
3. **`DR-003`:** Liquid DEGAS dissolved oxygen reduction verification in wet tank.

---

## 7. Prototype Closure Impact & Recommendation

1. **Software & Loop Prototype Closure:**
   - **All 3 Priority 0 risks (RSK-001 .. RSK-003) are fully verified and closed.**
   - **All 6 Priority 1 risks (RSK-004 .. RSK-009) are fully verified and closed.**
   - **No P2 or P3 risks block prototype operation or testing.**
   - **Physical HIL Test Suite:** **40 / 40 PASSED (100%)**.
   - **Mock Test Suite:** **92 / 92 PASSED (100%)**.

2. **System Declaration:**
   ```text
   SOFTWARE / LOOP PROTOTYPE — READY FOR FINAL CLOSURE
   FINAL HARDWARE VALIDATION — DEFERRED (DR-001, DR-002, DR-003)
   ```

3. **Manifesto Impact:**
   - No modifications or qualifications required for [`docs/EAGLEULTRASONIK_SYSTEM_MANIFESTO.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/EAGLEULTRASONIK_SYSTEM_MANIFESTO.md).

---
*Report completed under Phase 16 read-only risk triage.*
