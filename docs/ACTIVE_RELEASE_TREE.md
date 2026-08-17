# EAGLEULTRASONİK — ACTIVE RELEASE TREE

---

## 1. Executive Summary

This document defines the authoritative, minimal **Active Release Tree** required to build, compile, flash, test, operate, and document the EAGLEULTRASONiK controller system.

Starting from the actual compiler, flashing, HMI, test runner, and Agent OS entrypoints, this release tree identifies the exact set of **197 active release files** (186 Authoritative + 11 Required Support) out of the 590 total repository files.

### Master Release Metrics:
```text
TOTAL REPOSITORY FILES:        590
ACTIVE RELEASE TREE FILES:     197 (186 Authoritative [A] + 11 Required Support [B])
HISTORICAL ARCHIVE FILES:      173 (Category C)
GENERATED / LOCAL OUTPUTS:     183 (Category E)
TEMPORARY / SCRATCH FILES:      29 (Category F)
UNUSED LEGACY FILES:             8 (Category D)
```

---

## 2. Minimal Active Release Tree Table

### 2.1 STM32 Core Firmware & Drivers (126 Files) — Criticality: REQUIRED

| Path | Role | Why Required | Referenced By | Criticality |
| :--- | :--- | :--- | :--- | :--- |
| `STM32/Ultrasonik_G4_Master/Core/Src/main.c` | System Boot & Loop | System initialization, RCC 170MHz setup, superloop timer dispatch | `tools/build_stm32.sh`, `Makefile` | **REQUIRED** |
| `STM32/Ultrasonik_G4_Master/Core/Src/system_state.c` | State Machine & Safety | Core mode transitions (`IDLE`, `RUNNING`, `DEGAS`, `FAULT`) & SafeStop disarm | `main.c`, `esp32_uart.c` | **REQUIRED** |
| `STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c` | RS485 / Telemetry | USART3 ASCII command parser, 10-field STAT telemetry streaming | `main.c`, `system_state.c` | **REQUIRED** |
| `STM32/Ultrasonik_G4_Master/Core/Src/ultrasonic_pwm.c` | PWM & DEGAS Timing | TIM15 PWM complimentary output, 500ms soft-start, gated DEGAS bursts | `main.c`, `system_state.c` | **REQUIRED** |
| `STM32/Ultrasonik_G4_Master/Core/Src/x9c103s.c` | Frequency & Sweep | Digital pot wiper modulation (28k/40k center, 400ms sweep period) | `main.c`, `esp32_uart.c` | **REQUIRED** |
| `STM32/Ultrasonik_G4_Master/Core/Src/pt100_adc.c` | Temperature Acquisition | OPAMP3 PA1 ADC2 conversion, digital filter, open/short fault detection | `main.c`, `system_state.c` | **REQUIRED** |
| `STM32/Ultrasonik_G4_Master/Core/Src/heater_relay.c` | Heating & Phase Control | PB0 relay hysteresis control ($\pm 1.0^\circ\text{C}$), zero-cross Triac delay | `main.c`, `pt100_adc.c` | **REQUIRED** |
| `STM32/Ultrasonik_G4_Master/Core/Src/process_timer.c` | Process Countdown | Non-blocking countdown timer, 00:00 completion trigger | `main.c`, `system_state.c` | **REQUIRED** |
| `STM32/Ultrasonik_G4_Master/Core/Src/stm32g4xx_hal_msp.c` | Low-Level Pin Init | Peripheral MSP initializations for GPIO, TIM, ADC, OPAMP, UART | `stm32g4xx_hal.c` | **REQUIRED** |
| `STM32/Ultrasonik_G4_Master/Core/Src/stm32g4xx_it.c` | Interrupt Handlers | SysTick, USART3 IRQ, EXTI zero-cross, TIM15 OC interrupt service routines | Hardware Vector Table | **REQUIRED** |
| `STM32/Ultrasonik_G4_Master/Core/Src/system_stm32g4xx.c` | Clock Configuration | SystemCoreClock CMSIS clock tree configuration | `startup_stm32g474retx.s` | **REQUIRED** |
| `STM32/Ultrasonik_G4_Master/Core/Src/syscalls.c` | Newlib Runtime | C runtime library system call stubs (`_write`, `_sbrk`, etc.) | GCC Runtime | **REQUIRED** |
| `STM32/Ultrasonik_G4_Master/Core/Src/sysmem.c` | Memory Management | Dynamic heap allocation stubs for embedded runtime | `syscalls.c` | **REQUIRED** |
| `STM32/Ultrasonik_G4_Master/Core/Startup/startup_stm32g474retx.s` | Vector Table | ARM Cortex-M4 vector table, stack pointer init, Reset_Handler | `tools/build_stm32.sh` | **REQUIRED** |
| `STM32/Ultrasonik_G4_Master/Core/Inc/*.h` (10 files) | Core Headers | Function prototypes, typedefs, register structures, HAL config | `Core/Src/*.c` | **REQUIRED** |
| `STM32/Ultrasonik_G4_Master/Drivers/STM32G4xx_HAL_Driver/*` (48 files) | HAL Peripheral Drivers | STMicroelectronics hardware abstraction layer for STM32G4 | `Core/Src/*.c` | **REQUIRED** |
| `STM32/Ultrasonik_G4_Master/Drivers/CMSIS/*` (52 files) | CMSIS Core & Registers | ARM Cortex-M4 CMSIS core register and peripheral memory map | HAL Drivers | **REQUIRED** |
| `STM32/Ultrasonik_G4_Master/STM32G474RETX_FLASH.ld` | Flash Linker Script | Memory layout definition (Flash 512KB, SRAM 128KB, Page 127 reserve) | `tools/build_stm32.sh` | **REQUIRED** |
| `STM32/Ultrasonik_G4_Master/STM32G474RETX_RAM.ld` | RAM Linker Script | RAM-based execution layout definition for specialized debugging | Debug Configurations | **OPTIONAL** |
| `STM32/Ultrasonik_G4_Master/Ultrasonik_G4_Master.ioc` | CubeMX Descriptor | STM32CubeMX graphical pinout and peripheral descriptor | STM32CubeIDE | **IMPORTANT** |
| `STM32/Ultrasonik_G4_Master/Ultrasonik_G4_Master.cfg` | OpenOCD Config | OpenOCD target connection script for ST-Link V3 SWD interface | `flash_stm32.py` | **REQUIRED** |

---

### 2.2 ESP32 Master Firmware & Nextion HMI Assets (3 Files) — Criticality: REQUIRED

| Path | Role | Why Required | Referenced By | Criticality |
| :--- | :--- | :--- | :--- | :--- |
| `esp32/ekran_kontrol/ekran_kontrol.ino` | ESP32 Master Controller | FreeRTOS tasks, NVS recipe persistence, 3000ms watchdog, Nextion parser | Arduino IDE / CLI | **REQUIRED** |
| `EKRAN/arayuz.HMI` | Nextion GUI Source | Graphical project containing all pages, fonts, keypad, and touch buttons | Nextion Editor | **REQUIRED** |
| `EKRAN/arayuz.tft` | Nextion Binary Asset | Precompiled display binary for direct SD-card or serial HMI upload | Nextion Hardware | **REQUIRED** |

---

### 2.3 Hardware Authority (1 File) — Criticality: REQUIRED

| Path | Role | Why Required | Referenced By | Criticality |
| :--- | :--- | :--- | :--- | :--- |
| `hardware_wiring_FINAL_AUTHORITY.md` | Hardware Source of Truth | Definitive physical pinout mapping, jumper configurations, and wiring rules | All Tests, Agents, Docs | **REQUIRED** |

---

### 2.4 Automated Test Suites (4 Files) — Criticality: REQUIRED

| Path | Role | Why Required | Referenced By | Criticality |
| :--- | :--- | :--- | :--- | :--- |
| `test_hil_uart.py` | Physical HIL Test Suite | Primary 40-test physical Hardware-in-the-Loop regression test suite | CI / pytest runner | **REQUIRED** |
| `test_hmi_mock.py` | ESP32 HMI Mock Suite | 55-test mock verification suite for Nextion GUI & NVS storage | CI / pytest runner | **REQUIRED** |
| `test_rs485_mock.py` | RS485 Protocol Mock Suite | 37-test multi-drop ASCII framing, discovery, and collision suite | CI / pytest runner | **REQUIRED** |
| `id_full_lifecycle_test.py` | Node ID Lifecycle Test | Standalone multi-node discovery, staging, and Page 127 commit test | Bench Verification | **IMPORTANT** |

---

### 2.5 Active Agent OS Core Infrastructure (21 Files) — Criticality: REQUIRED

| Path | Role | Why Required | Referenced By | Criticality |
| :--- | :--- | :--- | :--- | :--- |
| `AGENTS.md` | Master Agent Registry | Entrypoint defining subagent scopes, tool access, and permissions | Agent OS Runtime | **REQUIRED** |
| `GEMINI.md` | Core Engineering Rules | HAL usage requirements, MISRA C compliance, FreeRTOS standards | Agent OS Runtime | **REQUIRED** |
| `PROJECT_STATE.md` | Project Context Ledger | Fast bootstrap state ledger for subagent discovery | Agent OS Runtime | **REQUIRED** |
| `.agents/AGENTS.md` | Declarative Subagent Spec | Subagent declarative specifications and scope constraints | Agent OS Runtime | **REQUIRED** |
| `.agents/rules/*.md` (8 files) | Subsystem Rules | Rules 00–07 governing safety, scope, STM32, ESP32, comms, testing | Subagent Prompts | **REQUIRED** |
| `.agents/skills/*/SKILL.md` (8 files)| Specialist Skills | Operational procedures for FreeRTOS, HIL testing, Nextion, STM32 | Subagent Tooling | **REQUIRED** |
| `.gitignore`, `.antigravityignore` (2 files)| Repo Exclusion | Prevents committing generated binaries, pycache, and logs to Git | Git / Antigravity | **REQUIRED** |

---

### 2.6 Authoritative Core Documentation (31 Files) — Criticality: REQUIRED / IMPORTANT

| Path | Role | Why Required | Referenced By | Criticality |
| :--- | :--- | :--- | :--- | :--- |
| `docs/EAGLEULTRASONIK_SYSTEM_MANIFESTO.md` | System Manifesto | Single source of truth for architecture, boundaries, and baseline | All Subsystems | **REQUIRED** |
| `docs/EAGLEULTRASONIK_MANIFESTO_TRACEABILITY.md` | Manifesto Traceability | 1-to-1 traceability mapping for all 27 manifesto sections | Manifesto | **REQUIRED** |
| `docs/EAGLEULTRASONIK_FINAL_MANIFESTO_RECONCILIATION.md`| Final Reconciliation | Reconciles risk closure, test campaign, and operating baseline | Project Baseline | **REQUIRED** |
| `docs/SYSTEM_FINAL_RISK_CLOSURE.md` | Master Risk Closure | Authoritative reconciled risk ledger for all RSK-001..015 & DR-001..003 | Risk Governance | **REQUIRED** |
| `docs/SYSTEM_RISK_REGISTER.md` | Risk Register | Master catalog of system risks, triggers, and mitigations | Risk Management | **REQUIRED** |
| `docs/SYSTEM_RISK_DECISION_MATRIX.md` | Risk Decision Matrix | Severity, probability, and closure criteria matrix | Risk Management | **IMPORTANT** |
| `docs/SYSTEM_RISK_RECONCILIATION_REPORT.md` | Risk Reconciliation | Reconciles historical P0/P1/P2/P3 classifications | Risk Management | **IMPORTANT** |
| `docs/SYSTEM_DEFERRED_REVALIDATION_REGISTER.md` | Deferred Hardware Register | Official register for physical deferred items `DR-001`, `DR-002`, `DR-003` | Hardware Commissioning| **REQUIRED** |
| `docs/SYSTEM_FUNCTION_INVENTORY.md` | Function Inventory | Comprehensive catalog of 47 system functions across 9 groups | Traceability | **REQUIRED** |
| `docs/SYSTEM_MASTER_E2E_TEST_PLAN.md` | Master Test Plan | Level 1–4 verification specifications and master flows FLOW-01..10 | Testing Architecture | **REQUIRED** |
| `docs/SYSTEM_E2E_EXECUTION_REPORT.md` | E2E Execution Report | Master execution log of executable acceptance tests | QA Governance | **IMPORTANT** |
| `docs/SYSTEM_FUNCTION_TRACEABILITY_MATRIX.md`| Function Traceability | Traceability mapping linking functions to code and test cases | System Verification | **IMPORTANT** |
| `docs/SYSTEM_TEST_ENVIRONMENT_MATRIX.md` | Test Environment Matrix | Bench equipment, serial ports, baud rates, and loopback setups | Test Environment | **IMPORTANT** |
| `docs/SYSTEM_CAPABILITIES.md` | System Capabilities | Formal declaration of system capabilities and physical limits | Technical Spec | **IMPORTANT** |
| `docs/SYSTEM_ENGINEERING_DECISION_REGISTER.md`| Decision Register | Engineering decisions DEC-001, DEC-002, DEC-003 | Architecture | **IMPORTANT** |
| `docs/P0_FINAL_CLOSURE_REPORT.md` .. `docs/P0_*.md` (6 files) | P0 Remediation Reports | Evidence and design records for RSK-001, RSK-002, RSK-003 closure | Safety Records | **REQUIRED** |
| `docs/P1_BATCH1_IMPLEMENTATION_REPORT.md` .. `P1_*.md` (6 files)| P1 Remediation Reports | Evidence records for RSK-004 through RSK-009 closure | Risk Records | **REQUIRED** |
| `docs/P2_P3_FINAL_TRIAGE_REPORT.md` | P2/P3 Triage Report | Authoritative triage for non-blocking items RSK-010..RSK-015 | Risk Records | **REQUIRED** |
| `docs/B_DEGAS_*.md`, `docs/C_SWEEP_*.md`, `docs/ID_*.md` (4 files)| Feature Closure Reports | Verification records for DEGAS, Sweep, and Tank ID provisioning | Feature History | **IMPORTANT** |

---

### 2.7 Required Build, Flash, and Tooling Support (11 Files) — Criticality: IMPORTANT

| Path | Role | Why Required | Referenced By | Criticality |
| :--- | :--- | :--- | :--- | :--- |
| `tools/build_stm32.sh` | Linux Build Script | GCC ARM compiler script for STM32 clean compilation | Developer / CI | **REQUIRED** |
| `tools/build_stm32.ps1` | Windows Build Script | PowerShell build script for STM32 compilation | Developer / CI | **REQUIRED** |
| `rpi_exec.py` | Remote SSH Test Runner | SSH automation bridge to execute pytest on Raspberry Pi 5 | `test_hil_uart.py` | **REQUIRED** |
| `flash_stm32.py` | OpenOCD SWD Flasher | Python OpenOCD wrapper to flash binary to target MCU | Developer / HIL | **IMPORTANT** |
| `list_serial_devices.py` | Serial Port Tool | Enumerates `/dev/ttyACM*` and COM ports on test host | Developer / HIL | **OPTIONAL** |
| `live_monitor.py` | Telemetry Monitor | Live visualizer for RS485 ASCII status stream | Developer / HIL | **OPTIONAL** |
| `STM32/.../.project`, `.cproject`, `.mxproject` (3 files)| Eclipse IDE Config | STM32CubeIDE project metadata and build configuration | STM32CubeIDE | **IMPORTANT** |
| `STM32/.../Ultrasonik_G4_Master.launch` | Eclipse Debug Launch | OpenOCD / GDB hardware debugging launcher configuration | STM32CubeIDE | **IMPORTANT** |
| `STM32/.../.github/copilot-instructions.md` | IDE AI Instructions | Contextual repository rules for IDE assistance | IDE Assistant | **OPTIONAL** |

---

## 3. Active Release Tree Count Summary

```text
======================================================================
  ACTIVE RELEASE TREE TOTAL:                                      197
    - STM32 Core Firmware & HAL Drivers:                          126
    - ESP32 Master Firmware:                                        1
    - Nextion HMI GUI Assets:                                       2
    - Hardware Authority:                                           1
    - Automated Test Suites:                                        4
    - Active Agent OS Infrastructure:                              21
    - Authoritative Documentation:                                 31
    - Build, Flash & Tooling Support:                              11
======================================================================
```

---
*Report completed under Phase 16 read-only release validation.*
