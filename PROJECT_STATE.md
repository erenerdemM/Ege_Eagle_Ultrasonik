# EAGLEULTRASONİK — PROJECT STATE

## SYSTEM STATUS

Current Phase: Production Acceptance & Release Baseline
Active Objective: Full HIL Verified Embedded Automation System
System Status: Production Ready (V4.0.0 Final Acceptance PASS)
Last Verified: 2026-08-24
Agent OS Version: 2.0.0 (Workspace Declarative Engine)

## HARDWARE BASELINE

MCU Architecture: Dual STM32G474RE Slave Nodes + ESP32-S3 Master Node + Nextion HMI
STM32 Nodes: Node ID 1 (Primary Ultrasonic Generator, TIM15 Triac Gate, Heater Control, X9C103S Digital Pot), Node ID 2 (Secondary Slave)
ESP32 Node: ESP32-S3 Master (UART Bridge, NVS Recipe & Service Management, Nextion HMI Sync)
HMI: Nextion HMI Display (USART2, 115200 Baud, Dual-Buffer UI Sync)
Communication Bus: Multi-Drop RS485 / ASCII UART Bus (`T<ID>:<CMD>`)
Hardware Revision: Rev 2.1 Final Authority Package (Protected under `hardware_wiring_FINAL_AUTHORITY.md`)
Verified Loopbacks: TRIAC (PC6 -> 1k -> PA6), Heater (PB15 -> 1k -> PA4), Zero-Cross (ESP32 GPIO4 -> STM32 PC7)

## FIRMWARE BASELINE

STM32: STM32G474 HAL-based, TIM15 Triac Phase-Angle PWM (500us..9500us), X9C103S Arbitrary 28..40 kHz Integer Mapping, OPAMP3 PT100 Signal Conditioning, Dual-Mode Heater Control (Bang-Bang Mechanical Relay + DC SSR PID / Time-Proportional PWM), USART3/LPUART1
ESP32: FreeRTOS Dual-Core C++, Persistent Sweep ARM/ACTIVE Dual-State Architecture, DEGAS Snapshot Frame Pipeline, Dual Heater Mode NVS Selection (`htr_m_<g>`), Nextion Page 5 Sync
HMI: Nextion HMI Interface (`arayuz.HMI`, `arayuz.tft`, Page 0..Page 8, `b_htr_mode` RELAY ↔ SSR Toggle)

## COMMUNICATION BASELINE

Protocol: Addressable ASCII Protocol Matrix (`T<ID>:SET_POWER:50`, `T<ID>:SET_HEATER_MODE:SSR`, `T<ID>:START_DEGAS:...`, `STAT,<ID>,<MODE>,...`)
Baudrate: 115200 Baud, 8N1
Frame Format: Line-terminated (`\n`), Ring-buffered Interrupt Transceiver, Clamping & Checksum Guard
Node Architecture: Master-Slave Multi-drop (ESP32 Master -> STM32 Slaves ID 1 & ID 2)

## TEST BASELINE

HIL Tests: `test_hil_uart.py` (Pytest Suite for Hardware-in-the-Loop UART Integration)
Heater Unit Tests: `test_heater_control.py` (133/133 Unit & Plant Simulation Tests Passed)
HMI Mock: `test_hmi_mock.py` (Nextion Display Protocol Mock Test)
RS485 Mock: `test_rs485_mock.py` (Multi-drop RS485 Bus Collision & Timing Test)
HIL Manual & Loopback Audits: Dual Heater Session `MANUAL_PAGE5_HEATER_20260824_155742` PASS (1533/1533 Loopback Matches, 0 Mismatch)
Last Known Test Status: All Mock & Hardware Verification Suites Passed (RUNTIME-PROVEN)


## AUTHORITATIVE DOCUMENTS

Hardware Authority: `hardware_wiring_FINAL_AUTHORITY.md` (IMMUTABLE SINGLE SOURCE OF TRUTH)
Master Manifest: `docs/EAGLEULTRASONIK_SYSTEM_MANIFESTO_V4.md` (Authoritative V4 Current State System Manifesto)
Historical Manifests: `docs/EAGLEULTRASONIK_CURRENT_STATE_MANIFESTO.md` (V3) & `docs/EAGLEULTRASONIK_SYSTEM_MANIFESTO.md` (V1/V2)
Communication: `UART_Entegrasyon_Raporu.md` & `.agent/reports/protocol-analysis.md`
Architecture: `.agent/reports/SYSTEM-MASTER-DOCUMENT.md` & `.agent/reports/architecture.md`

## ACTIVE WORK

Current Task: System Manifest V4 Update & Full GitHub Release Push
Current Phase: Release Finalization
Next Task: Factory Commissioning & On-Site Installation

## KNOWN ISSUES

- None blocking. All historical firmware bugs (Code 68, Sweep IDLE drift, Sweep disarm on STOP/Freq change, DEGAS 35k mapping) resolved and verified.

## BLOCKERS

- None.

## RECENT DECISIONS

- Decision 01: Hardware Source of Truth established as immutable (`hardware_wiring_FINAL_AUTHORITY.md`).
- Decision 02: Agent OS V2 adopted with 7 specialized subagent roles and 5-tier Task Complexity Routing.
- Decision 03: Historical audit directory `.agent/reports/` archived for on-demand context loading.
- Decision 04: Phase 5.2 Physical RS485 direction control (DE/RE) implemented on STM32 (PB1) and ESP32 (GPIO5) with 100% test pass rate.

## AGENT OS

Master: `Antigravity` (Orchestrator & Task Router)
Specialists: `system-architect`, `stm32-specialist`, `esp32-hmi-specialist`, `hardware-engineer`, `communication-specialist`, `qa-test-engineer`, `code-reviewer`
Rules: `00-global-engineering`, `01-source-of-truth`, `02-scope-control`, `03-stm32`, `04-esp32`, `05-communication`, `06-testing`, `07-agent-orchestration`
Skills: `project-bootstrap`, `stm32-firmware`, `esp32-freertos`, `uart-rs485`, `nextion-hmi`, `hardware-validation`, `hil-testing`, `code-review`

## CONTEXT POLICY

Level 1: `PROJECT_STATE.md` (Immediate status bootstrap ~500 tokens)
Level 2: Relevant `.agents/rules/*.md` files loaded on-demand
Level 3: Relevant `.agents/skills/*/SKILL.md` loaded on-demand
Level 4: Targeted source files (`STM32/`, `esp32/`, `EKRAN/`) using line-range viewing
Level 5: Authoritative documentation (`hardware_wiring_FINAL_AUTHORITY.md`) when verifying pinouts
Level 6: Historical reports (`.agent/reports/`) STRICTLY ONLY when explicitly requested or during deep escalation
