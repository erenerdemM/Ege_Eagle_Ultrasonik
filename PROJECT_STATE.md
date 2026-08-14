# EAGLEULTRASONİK — PROJECT STATE

## SYSTEM STATUS

Current Phase: Phase 5.2 — Systems Integration & Safety Hardening Baseline
Active Objective: Agent OS V2 Production Usage & Task-Aware Orchestration
System Status: Production Ready (Agent OS V2 Final Acceptance PASS)
Last Verified: 2026-08-11
Agent OS Version: 2.0.0 (Workspace Declarative Engine)

## HARDWARE BASELINE

MCU Architecture: Dual STM32G474RE Slave Nodes + ESP32-S3 Master Node + Nextion HMI
STM32 Nodes: Node ID 1 (Primary Ultrasonic Generator & Heater Control), Node ID 2 (Secondary Slave)
ESP32 Node: ESP32-S3 Master (UART Bridge, NVS Recipe Management, Nextion HMI Sync)
HMI: Nextion HMI Display (USART2, 115200 Baud, Dual-Buffer UI Sync)
Communication Bus: Multi-Drop RS485 / ASCII UART Bus (`T<ID>:<CMD>`)
Hardware Revision: Rev 2.1 Final Authority Package

## FIRMWARE BASELINE

STM32: STM32G474 HAL-based, TIM15 PWM Soft-start (20kHz-40kHz), OPAMP3 PT100 Signal Conditioning, USART3/LPUART1
ESP32: FreeRTOS Dual-Core C++, 100Hz `esp_timer` Zero-cross Simulator, NVS Flash Storage, 3000ms Watchdog
HMI: Nextion HMI Interface (`arayuz.HMI`, `arayuz.tft`)

## COMMUNICATION BASELINE

Protocol: Addressable ASCII Protocol Matrix (`T<ID>:SET_PWR=50`, `STAT,ID=1,PWR=50,TEMP=65.5,ERR=0`)
Baudrate: 115200 Baud, 8N1
Frame Format: Line-terminated (`\r\n`), 64-byte Maximum Frame Size, Clamping & Checksum Guard
Node Architecture: Master-Slave Multi-drop (ESP32 Master → STM32 Slaves ID 1 & ID 2)

## TEST BASELINE

HIL Tests: `test_hil_uart.py` (Pytest Suite for Hardware-in-the-Loop UART Integration)
HMI Mock: `test_hmi_mock.py` (Nextion Display Protocol Mock Test)
RS485 Mock: `test_rs485_mock.py` (Multi-drop RS485 Bus Collision & Timing Test)
Last Known Test Status: All Mock & Integration Test Suites Verified Passed

## AUTHORITATIVE DOCUMENTS

Hardware Authority: `hardware_wiring_FINAL_AUTHORITY.md` (IMMUTABLE SINGLE SOURCE OF TRUTH)
Manifest: `Manifesto_V3.md` (Master Project Requirements & Architectural Manifesto)
Communication: `UART_Entegrasyon_Raporu.md` & `.agent/reports/protocol-analysis.md`
Architecture: `.agent/reports/SYSTEM-MASTER-DOCUMENT.md` & `.agent/reports/architecture.md`

## ACTIVE WORK

Current Task: Phase 5.2 — RS485 Physical Communication Implementation
Current Phase: Phase 5.2 (RS485 Half-Duplex DE/RE Hardware Integration Completed)
Next Task: Desktop Hardware RS485 Transceiver Wiring & Physical HIL Acceptance

## KNOWN ISSUES

- Issue #1: High telemetry burst rates at 115200 baud can cause RX buffer queue buildup if FreeRTOS task priority is lowered.
- Issue #2: Soft-start PWM ramping requires strictly non-blocking zero-cross timing synchronization on STM32 TIM15.

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
