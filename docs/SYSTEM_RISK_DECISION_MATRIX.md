# EAGLEULTRASONİK — SYSTEM RISK DECISION & PRIORITY MATRIX

---

## 1. Executive Summary

This matrix presents the formal decision and priority classification for all 15 risks evaluated during Phase 11 Risk Validation Gate. All findings are derived from empirical code inspection and independent specialist review. Per project rules, **zero code or configuration modifications were made** during this phase.

---

## 2. Risk Reclassification & Decision Matrix

| Risk ID | Original Severity | Validation Status | Evidence Location | Requirement / ADR Conflict | Test Detection Status | Final Classification | Confidence | Priority |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **RSK-001** | **CRITICAL** | **CONFIRMED DEFECT** | `esp32_uart.c:335`, `system_state.c:106` | Safety State Invariants ([Manifesto_V3.md:§3](file:///c:/Users/ern0e/EAGLEULTRASONiK/Manifesto_V3.md)) | 🔴 False Pass (`test_04`) | **CONFIRMED DEFECT** | **HIGH** | **P0** |
| **RSK-002** | **CRITICAL** | **CONFIRMED DEFECT** | `esp32_uart.c:69-83` | Real-Time Driver Rules ([Manifesto_V3.md:§7](file:///c:/Users/ern0e/EAGLEULTRASONiK/Manifesto_V3.md)) | ⚪ Not Detected | **CONFIRMED DEFECT** | **HIGH** | **P0** |
| **RSK-003** | **HIGH** | **CONFIRMED DEFECT** | `ekran_kontrol.ino:1046-1109` | Active Process Lockout Invariants | 🔴 False Pass (`test_degas_04`) | **CONFIRMED DEFECT** | **HIGH** | **P0** |
| **RSK-004** | **HIGH** | **CONFIRMED DEFECT** | `ekran_kontrol.ino:582` | Master Telemetry & Response Protocol | 🟡 Blind Spot (`test_15`) | **CONFIRMED DEFECT** | **HIGH** | **P1** |
| **RSK-005** | **HIGH** | **CONFIRMED DEFECT** | `esp32_uart.c:635-653` | Serialization Safety ([Manifesto_V3.md:§7](file:///c:/Users/ern0e/EAGLEULTRASONiK/Manifesto_V3.md)) | ⚪ Not Detected | **CONFIRMED DEFECT** | **HIGH** | **P1** |
| **RSK-006** | **HIGH** | **CONFIRMED DESIGN RISK** | `esp32_uart.c:178` | Provisioning Security Invariants | ⚪ Not Detected | **CONFIRMED DESIGN RISK** | **HIGH** | **P1** |
| **RSK-007** | **HIGH** | **CONFIRMED DESIGN RISK** | `esp32_uart.c:696,725` | Peripheral Driver Fault-Tolerance | 🟡 Symptom Only (`test_16`) | **CONFIRMED DESIGN RISK** | **HIGH** | **P1** |
| **RSK-008** | **MEDIUM** | **CONFIRMED DEFECT** | `ekran_kontrol.ino:951,1110` | Service Authentication Rules | 🔴 False Pass (`test_09`) | **CONFIRMED DEFECT** | **HIGH** | **P1** |
| **RSK-009** | **MEDIUM** | **CONFIRMED DEFECT** | `ekran_kontrol.ino:1408-1416` | UI Connection Status Synchronization | 🔴 False Pass (`test_11`) | **CONFIRMED DEFECT** | **HIGH** | **P1** |
| **RSK-010** | **MEDIUM** | **CONDITIONAL RISK** | `main.c:152,212` | Real-Time Interrupt Latency | ⚪ Not Detected | **CONDITIONAL RISK** | **MEDIUM**| **P2** |
| **RSK-011** | **MEDIUM** | **CONFIRMED DESIGN RISK** | `system_state.h:40`, `ultrasonic_pwm.c:225` | Concurrency & Data Integrity | ⚪ Not Detected | **CONFIRMED DESIGN RISK** | **HIGH** | **P2** |
| **RSK-012** | **MEDIUM** | **CONDITIONAL RISK** | `esp32_uart.c:627`, `pt100_adc.c:53` | Defensive Data Conversion | ⚪ Not Detected | **CONDITIONAL RISK** | **MEDIUM**| **P2** |
| **RSK-013** | **MEDIUM** | **CONFIRMED DESIGN RISK** | `esp32_uart.c:606`, `ekran_kontrol.ino:581` | RS485 Checksum Integrity | 🟢 Risk Confirmed (`test_16`) | **CONFIRMED DESIGN RISK** | **HIGH** | **P2** |
| **RSK-014** | **LOW** | **CONFIRMED DEFECT** | `ekran_kontrol.ino:75,1358` | Inactivity Session Management | 🔴 False Pass (`test_08`) | **CONFIRMED DEFECT** | **HIGH** | **P3** |
| **RSK-015** | **LOW** | **CONFIRMED DESIGN RISK** | `esp32_uart.c:95,696` | High-Baud Rate DMA Best Practices | ⚪ Not Detected | **CONFIRMED DESIGN RISK** | **HIGH** | **P3** |

---

## 3. Decision Priority Breakdown

### P0 — Must Resolve Before Further Feature Work (3 Risks)
Critical safety, spinlock, or touch lockout defects that compromise real-time safety:
1. **RSK-001:** `STOP` command clears active hardware fault bitmask (`esp32_uart.c:335`).
2. **RSK-002:** `RS485_Transmit_Blocking()` infinite spinlock deadlock inside ISR context (`esp32_uart.c:69-83`).
3. **RSK-003:** Setpoint touch edits bypass normal washing cycle lockout (`ekran_kontrol.ino:1046-1109`).

### P1 — Must Resolve Before Final Production Release (6 Risks)
High functional, security, or error-handling issues:
1. **RSK-004:** ESP32 master response blind spot for slave ACK/NACK/ERR frames (`ekran_kontrol.ino:582`).
2. **RSK-005:** `snprintf` return value over-read in UART IT transmit buffer (`esp32_uart.c:635-653`).
3. **RSK-006:** Asymmetric DEGAS provisioning interlock check on STM32 (`esp32_uart.c:178`).
4. **RSK-007:** Unchecked HAL driver return statuses in UART error callbacks (`esp32_uart.c:696`).
5. **RSK-008:** Unauthenticated administrative configuration commands on ESP32 (`ekran_kontrol.ino:951`).
6. **RSK-009:** Stale UI status display on RS485 disconnection (`ekran_kontrol.ino:1408`).

### P2 — Engineering Improvements (4 Risks)
Real design risks that do not immediately block basic operation:
1. **RSK-010:** Interrupt disabling during Flash page erase operations (`main.c:152`).
2. **RSK-011:** Multi-word `g_system_state` data race between superloop and EXTI ISR (`system_state.h:40`).
3. **RSK-012:** Unchecked float-to-int conversion in telemetry string generator (`esp32_uart.c:627`).
4. **RSK-013:** Lack of checksum / CRC validation on standard ASCII telemetry frames (`esp32_uart.c:606`).

### P3 — Documentation / Test / Minor Improvements (2 Risks)
Minor functional or session timing enhancements:
1. **RSK-014:** Service session inactivity timer never refreshes during active setup (`ekran_kontrol.ino:1358`).
2. **RSK-015:** Single-byte RX interrupt overhead at 115200 baud (`esp32_uart.c:95`).

### DEFERRED — Hardware Revalidation (1 Item)
1. **DR-001 / `test_17`:** Physical PT100 RTD sensor probe & AC heater/SSR load revalidation.
