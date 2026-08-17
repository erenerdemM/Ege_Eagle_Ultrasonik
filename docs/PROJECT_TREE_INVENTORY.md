# EAGLEULTRASONİK — COMPLETE PROJECT TREE INVENTORY

---

## 1. Executive Summary

This inventory documents the complete repository file tree, subsystem composition, and source-of-truth mapping for the EAGLEULTRASONiK project across all 590 tracked and untracked files.

### Repository Summary Statistics:
- **Total Tracked & Untracked Files:** **590**
- **Authoritative Source / Keep (A):** **186**
- **Support & Tooling / Keep (B):** **11**
- **Historical Reports / Archive Candidates (C):** **173**
- **Legacy Files / Delete Candidates (D):** **8**
- **Generated Build & Cache Outputs (E):** **183**
- **Temporary Scripts & Backup Snapshots (F):** **29**
- **Unknown / Human Review (G):** **0**

---

## 2. Source of Truth Analysis Matrix

| Domain | Authoritative Primary Path | Support / Tooling Path | Stale / Duplicate / Legacy Path |
| :--- | :--- | :--- | :--- |
| **STM32 Firmware** | `STM32/Ultrasonik_G4_Master/Core/Src/*`<br>`STM32/Ultrasonik_G4_Master/Core/Inc/*` | `STM32/Ultrasonik_G4_Master/Drivers/*`<br>`STM32/Ultrasonik_G4_Master/*.ld` | `main.c` (Root mirror)<br>`x9c103s.c` (Root mirror)<br>`heater_triac_bench_test.*`<br>`id_bench_test.*` |
| **ESP32 Firmware** | `esp32/ekran_kontrol/ekran_kontrol.ino` | — | — |
| **Nextion HMI GUI** | `EKRAN/arayuz.HMI`<br>`EKRAN/arayuz.tft` | — | `EKRAN/Arayuz.zi` (Backup zip) |
| **Automated Tests** | `test_hil_uart.py`<br>`test_hmi_mock.py`<br>`test_rs485_mock.py`<br>`id_full_lifecycle_test.py` | `rpi_exec.py`<br>`flash_stm32.py` | `dip_switch_test.py` (Obsolete) |
| **Build & Flash** | `tools/build_stm32.sh`<br>`tools/build_stm32.ps1` | `STM32/.../Ultrasonik_G4_Master.cfg`<br>`flash_stm32.py` | `TEST_ARTIFACTS_BACKUP_*/deploy_*.ps1` |
| **Hardware Authority** | `hardware_wiring_FINAL_AUTHORITY.md` | — | Root `PHASE_6_2_*.md` wiring duplicates |
| **System Manifesto** | `docs/EAGLEULTRASONIK_SYSTEM_MANIFESTO.md` | `docs/EAGLEULTRASONIK_MANIFESTO_TRACEABILITY.md` | `SYSTEM_MANIFESTO.md` (Legacy root)<br>`Manifesto_V3.md` (Legacy root) |
| **Risk Baseline** | `docs/SYSTEM_FINAL_RISK_CLOSURE.md`<br>`docs/SYSTEM_RISK_REGISTER.md` | `docs/SYSTEM_RISK_DECISION_MATRIX.md` | Root historical risk registers |
| **Agent OS Core** | `AGENTS.md`<br>`GEMINI.md`<br>`PROJECT_STATE.md`<br>`.agents/rules/*`<br>`.agents/skills/*` | `.antigravityignore`<br>`.gitignore` | `.agent/reports/*` (Historical phase logs) |

---

## 3. Subsystem Breakdown and Complete File Tree

### 3.1 STM32 Subsystem (`STM32/Ultrasonik_G4_Master/`) — 172 Files
- **Core Source Files (Authoritative — 14 files):**
  - `Core/Src/main.c` (System boot, 170MHz RCC clock, superloop timer dispatch)
  - `Core/Src/system_state.c` (Mode state machine, SafeStop atomic disarm)
  - `Core/Src/esp32_uart.c` (USART3 RS485 communication, ASCII parser, STAT telemetry)
  - `Core/Src/ultrasonic_pwm.c` (TIM15 PWM soft-start duty ramp, gated DEGAS burst)
  - `Core/Src/x9c103s.c` (Digital pot 28k/40k step control, non-blocking sweep modulation)
  - `Core/Src/pt100_adc.c` (OPAMP3 PA1 PT100 ADC sampling and digital filter)
  - `Core/Src/heater_relay.c` (PB0 relay hysteresis, EXTI zero-cross Triac gate firing)
  - `Core/Src/process_timer.c` (Non-blocking countdown timer, 00:00 auto-SafeStop)
  - `Core/Src/stm32g4xx_hal_msp.c` (HAL low-level pin and peripheral MSP initializers)
  - `Core/Src/stm32g4xx_it.c` (Core interrupt service routines)
  - `Core/Src/system_stm32g4xx.c` (CMSIS clock tree setup)
  - `Core/Src/syscalls.c`, `sysmem.c` (Newlib C standard library runtime stubs)
  - `Core/Startup/startup_stm32g474retx.s` (ARM Cortex-M4 vector table and reset handler)
- **Core Header Files (Authoritative — 12 files):**
  - `Core/Inc/main.h`, `system_state.h`, `esp32_uart.h`, `ultrasonic_pwm.h`, `x9c103s.h`, `pt100_adc.h`, `heater_relay.h`, `process_timer.h`, `stm32g4xx_hal_conf.h`, `stm32g4xx_it.h`
- **Hardware Abstraction Layer & CMSIS (Authoritative — 100 files):**
  - `Drivers/STM32G4xx_HAL_Driver/Inc/*`, `Drivers/STM32G4xx_HAL_Driver/Src/*` (HAL drivers for ADC, TIM, UART, OPAMP, FLASH, RCC, GPIO, IWDG, PWR)
  - `Drivers/CMSIS/*` (Cortex-M4 CMSIS core access and register definitions)
- **Linker Scripts & Project Descriptors (Authoritative / Support — 5 files):**
  - `STM32G474RETX_FLASH.ld`, `STM32G474RETX_RAM.ld`, `Ultrasonik_G4_Master.ioc`, `Ultrasonik_G4_Master.cfg`, `.project`, `.cproject`, `.mxproject`
- **Build Output Directory (`build-stm32/` — Generated — 41 files):**
  - Compiled object files (`.o`), `Ultrasonik_G4_Master.elf`, `Ultrasonik_G4_Master.bin`, `Ultrasonik_G4_Master.hex`, `Ultrasonik_G4_Master.map`

---

### 3.2 ESP32 & HMI Subsystems (`esp32/`, `EKRAN/`) — 4 Files
- **ESP32 Firmware (Authoritative — 1 file):**
  - `esp32/ekran_kontrol/ekran_kontrol.ino` (Master FreeRTOS tasks, NVS recipe persistence, 3000ms RS485 connection watchdog, Nextion parser, Service PIN `123456` authentication)
- **Nextion HMI GUI Assets (Authoritative — 2 files):**
  - `EKRAN/arayuz.HMI` (Nextion Editor project source file containing Page 0..5, keypad, recipe controls, red alert popup)
  - `EKRAN/arayuz.tft` (Compiled binary display asset flashed directly to Nextion screen)
- **Backup Artifact (Temporary — 1 file):**
  - `EKRAN/Arayuz.zi` (Legacy zip backup of HMI assets)

---

### 3.3 Automated Test Suites & Support Scripts (Root) — 8 Files
- **Authoritative Test Suites (Category A — 4 files):**
  - `test_hil_uart.py` (Authoritative physical Hardware-in-the-Loop test suite — 40 tests, 100% pass)
  - `test_hmi_mock.py` (Authoritative Nextion HMI dual-core state mock suite — 55 tests, 100% pass)
  - `test_rs485_mock.py` (Authoritative RS485 multi-drop ASCII collision & discovery suite — 37 tests, 100% pass)
  - `id_full_lifecycle_test.py` (Authoritative multi-node discovery and Flash Page 127 commit test)
- **Tooling & Bench Support (Category B — 4 files):**
  - `rpi_exec.py` (SSH remote test execution wrapper on Raspberry Pi 5)
  - `flash_stm32.py` (OpenOCD ST-Link SWD firmware flasher)
  - `list_serial_devices.py` (USB serial port enumeration tool)
  - `live_monitor.py` (Real-time RS485 bus telemetry visualizer)

---

### 3.4 Active Agent OS Infrastructure (`AGENTS.md`, `GEMINI.md`, `.agents/`) — 20 Files
- **Root Entrypoints (Authoritative — 3 files):**
  - `AGENTS.md` (Agent OS declarative registry & delegation scope)
  - `GEMINI.md` (Core engineering rules, HAL compliance, FreeRTOS standards)
  - `PROJECT_STATE.md` (Fast context bootstrap and authoritative state ledger)
- **Subagent Rules (`.agents/rules/` — Authoritative — 8 files):**
  - `00-global-engineering.md`, `01-source-of-truth.md`, `02-scope-control.md`, `03-stm32.md`, `04-esp32.md`, `05-communication.md`, `06-testing.md`, `07-agent-orchestration.md`
- **Subagent Skills (`.agents/skills/` — Authoritative — 8 files):**
  - `code-review/SKILL.md`, `esp32-freertos/SKILL.md`, `hardware-validation/SKILL.md`, `hil-testing/SKILL.md`, `nextion-hmi/SKILL.md`, `project-bootstrap/SKILL.md`, `stm32-firmware/SKILL.md`, `uart-rs485/SKILL.md`
- **Agent Registry (`.agents/AGENTS.md` — Authoritative — 1 file)**

---

### 3.5 Authoritative Project Documentation (`docs/`) — 31 Core Documents
- **Manifesto & Traceability:**
  - `docs/EAGLEULTRASONIK_SYSTEM_MANIFESTO.md`, `docs/EAGLEULTRASONIK_MANIFESTO_TRACEABILITY.md`, `docs/EAGLEULTRASONIK_FINAL_MANIFESTO_RECONCILIATION.md`
- **Risk Closure & Reconciliation:**
  - `docs/SYSTEM_FINAL_RISK_CLOSURE.md`, `docs/SYSTEM_RISK_REGISTER.md`, `docs/SYSTEM_RISK_DECISION_MATRIX.md`, `docs/SYSTEM_RISK_RECONCILIATION_REPORT.md`, `docs/SYSTEM_DEFERRED_REVALIDATION_REGISTER.md`
- **System Specifications & Test Plans:**
  - `docs/SYSTEM_FUNCTION_INVENTORY.md`, `docs/SYSTEM_MASTER_E2E_TEST_PLAN.md`, `docs/SYSTEM_E2E_EXECUTION_REPORT.md`, `docs/SYSTEM_FUNCTION_TRACEABILITY_MATRIX.md`, `docs/SYSTEM_TEST_ENVIRONMENT_MATRIX.md`, `docs/SYSTEM_CAPABILITIES.md`, `docs/SYSTEM_ENGINEERING_DECISION_REGISTER.md`
- **P0 / P1 / P2 / P3 Implementation Reports:**
  - `docs/P0_FINAL_CLOSURE_REPORT.md`, `docs/P0_IMPLEMENTATION_REPORT.md`, `docs/P0_PHYSICAL_REVALIDATION_REPORT.md`, `docs/P0_REMEDIATION_DESIGN.md`, `docs/P0_REGRESSION_TEST_DESIGN.md`, `docs/P0_REGRESSION_TEST_RESULTS.md`, `docs/P1_RISK_TRIAGE_REPORT.md`, `docs/P1_BATCH1_IMPLEMENTATION_REPORT.md`, `docs/P1_BATCH1_TEST_INTEGRITY_AUDIT.md`, `docs/P1_BATCH2_IMPLEMENTATION_REPORT.md`, `docs/RSK008_AUTHENTICATION_CONSISTENCY_REPORT.md`, `docs/RSK008_DOCUMENTATION_RECONCILIATION_REPORT.md`, `docs/P2_P3_FINAL_TRIAGE_REPORT.md`
- **Feature Verification Reports:**
  - `docs/B_DEGAS_SOFTWARE_CLOSURE_REPORT.md`, `docs/B_DEGAS_E2E_VERIFICATION_REPORT.md`, `docs/C_SWEEP_FINAL_VERIFICATION_REPORT.md`, `docs/ID_FINAL_VERIFICATION_REPORT.md`

---

### 3.6 Historical Phase Reports & Logs (`.agent/reports/`, Root `.md`, `logs/`) — 380 Files
- **Agent OS Historical Reports (`.agent/reports/` — Category C — 86 files):** Phase 1 through Phase 14 historical audit and verification reports.
- **Root Historical Phase Reports (Category C — 58 files):** `PHASE_6_2_*.md`, `FINAL_*.md`, `RS485_*.md`, `Manifesto_V3.md`.
- **Docs Historical Drafts (Category C — 29 files):** Superseded architectural audit drafts.
- **Test Execution Logs (`logs/`, `test_results.log` — Category E — 121 files):** Automated execution logs from historical pytest runs.
- **Temporary Scripts & Backups (`scratch/`, `TEST_ARTIFACTS_BACKUP_*` — Category F — 28 files):** One-off scripts and dated backups.
- **Legacy Files (Category D — 8 files):** Root duplicate source files (`main.c`, `x9c103s.c`), obsolete bench tests (`heater_triac_bench_test.*`, `id_bench_test.*`, `dip_switch_test.py`), and legacy PIC code (`500W_Display.mbas`).

---
*Inventory completed under Phase 16 read-only analysis.*
