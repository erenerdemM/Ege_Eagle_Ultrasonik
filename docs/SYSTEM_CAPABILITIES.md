# EAGLEULTRASONiK — Implemented System Capabilities

## 1. Core Process Control

- Dual-frequency operation with selectable 28 kHz and 40 kHz operating modes.
- Ultrasonic power-level control from 0% to 100%.
- Heating control with PT100 temperature measurement and heater relay control.
- Process timer with configurable process duration.
- START and STOP process control.
- Automatic safe shutdown when the configured process timer expires.
- Quick-wash preset functionality.

## 2. Recipe and HMI Management

- Multiple programmable recipes (P1, P2, P3).
- Recipe selection from the HMI.
- Recipe parameter editing.
- Persistent recipe storage using ESP32 NVS.
- Tank selection and per-tank process configuration.
- Configurable machine/tank count within the supported 1–10 node range.
- Service-mode authentication.
- Automatic service-session timeout after inactivity.

## 3. Multi-Node RS485 Architecture

- Half-duplex multi-drop RS485 communication architecture.
- ESP32 operates as the bus master/controller.
- STM32 nodes operate as addressed slave/control nodes.
- Addressed command frames using the `T<id>:` addressing scheme.
- `T0:` universal broadcast addressing.
- Support for up to 10 addressed STM32 nodes.
- Automatic node discovery using STM32 hardware UID information.
- CRC16-based slotted discovery response timing.
- Managed Tank ID provisioning.
- Hardware UID verification before ID assignment.
- Rejection of invalid or mismatched UID assignment requests.
- Rejection of direct ID assignment to active nodes without the required provisioning flow.
- Atomic ID-swap workflow.
- Write-ahead-log (WAL) based recovery for ID-swap operations.

## 4. Safety and Fault Handling

- Hardware independent watchdog (IWDG).
- Communication-loss watchdog for the RS485 control path.
- Safe shutdown on RS485 communication timeout during active operation.
- Fault-state handling.
- Safe shutdown on process completion.
- Safe shutdown on user STOP commands.
- Heater forced to a safe OFF state during safe shutdown.
- Ultrasonic/Triac power output forced to a safe OFF state during safe shutdown.
- PT100/sensor fault handling.
- Watchdog-reset detection and safe-stop recovery.

## 5. Bus Diagnostics and Observability

- RS485 valid-frame diagnostics.
- CRC error counters.
- Malformed-frame counters.
- Receive timeout counters.
- Dropped-frame counters.
- Transmitted-frame counters.
- ACK/NACK counters.
- Diagnostic query support.
- Broadcast diagnostic response suppression to prevent multi-node bus collisions.
- STM32 ground-truth telemetry reporting.
- ESP32 telemetry forwarding.
- HIL/debug telemetry through the ST-LINK VCP channel.
- Raw PT100 ADC observability.
- Triac firing-delay observability.
- Heater output and feedback observability.
- Triac output and feedback observability.

## 6. Frequency and Power Control

- Runtime selection between 28 kHz and 40 kHz operation.
- X9C103S-controlled frequency selection path.
- Runtime ultrasonic power control.
- Triac phase-angle power control.
- Soft-start power ramping.
- Runtime validation of supported frequency values.
- Rejection of unsupported frequency requests.

## 7. Commissioning and Provisioning

- Discovery of uncommissioned STM32 nodes.
- UID-based node identification.
- Controlled STAGE_ID workflow.
- Controlled ASSIGN_ID workflow.
- RESET_ID workflow.
- Provisioning-state tracking.
- Isolation of staging nodes from normal discovery responses.
- Prevention of unauthorized provisioning while the process is RUNNING.
- Persistent Tank ID storage.
- Verification of persisted Tank ID information.

## 8. HMI / ESP32 Control Layer

- HMI command parsing and validation.
- START/STOP command handling.
- Frequency command handling.
- Timer and temperature parameter handling.
- Recipe save/load handling.
- Tank selection handling.
- Machine connection/offline-state handling based on telemetry freshness.
- Prevention of START when a required control card/node is considered offline.
- Service-function permission control.

## 9. Verification and Test Infrastructure

- Software-only RS485 mock simulation.
- Software-only HMI/ESP32 mock simulation.
- Real hardware-in-the-loop UART/RS485 verification.
- STM32 ground-truth telemetry verification.
- ESP32-to-STM32 telemetry consistency verification.
- Physical heater/Triac feedback loopback verification.
- Communication-loss safety verification.
- Provisioning security verification.
- Frequency-control verification.
- PT100 ADC-to-temperature correlation verification.
- Reproducible STM32 GCC build workflow.
- Automated STM32 ELF/BIN/HEX generation.

## 10. Important Capability Boundary

The current system implements automatic node discovery and managed ID provisioning.

It does NOT currently implement a fully autonomous "find the first free Tank ID and assign it automatically without an explicit provisioning decision" algorithm.

Therefore the supported capability should be described as:

**Automatic node discovery and managed ID provisioning**

rather than:

**Fully autonomous automatic ID assignment.**

## 11. Verification Baseline

The implemented capabilities are supported by the current verification baseline:

- RS485 software mock suite: 26/26 tests passed.
- HMI software mock suite: 22/22 tests passed.
- Real hardware-in-the-loop suite: 20/20 tests passed.
- STM32 clean GCC build: passed.
- STM32 firmware flash programming: passed.
- STM32 flash verification: passed.
- STM32 runtime/HIL telemetry: passed.
- Local Git `main` and `origin/main`: synchronized at the current verified baseline.

## 12. Scope Note

This document describes implemented software and firmware capabilities.

It does not by itself constitute complete product certification, EMC/EMI qualification, electrical safety certification, long-duration reliability qualification, or production acceptance testing.
