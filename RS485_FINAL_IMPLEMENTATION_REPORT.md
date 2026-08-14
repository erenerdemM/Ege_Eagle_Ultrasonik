# EAGLEULTRASONİK — PHASE 5.2 RS485 FINAL IMPLEMENTATION & HARDWARE VALIDATION REPORT

**Document Version:** 2.0.0  
**Phase:** Phase 5.2 — RS485 Physical Communication Implementation  
**Date:** 2026-08-11  
**Lead Engineer:** Lead Embedded Systems Engineer / Hardware Integration Architect Agent  
**Final Verdict:** **PASS WITH WARNINGS** (Pending Physical Transceiver Hardware Gate Confirmation)

---

## 1. Executive Summary

Phase 5.2 establishes the physical half-duplex RS485 communication link between the **ESP32-S3 Master Node** and up to 10 **STM32G474RE Slave Nodes**. All subagent audits (Datasheet/Electrical, STM32 Firmware, ESP32 Firmware, Protocol Architecture, Test & Verification) have been completed with cross-verified passing results. 

The software simulation test suite passed **48/48 active test cases** with zero regressions.

---

## 2. Subagent Findings

| Subagent Role | Audit Scope | Status | Key Findings & Actions Taken |
| :--- | :--- | :---: | :--- |
| **SUBAGENT-1 (Electrical Auditor)** | STM32G474RE, ESP32-S3, RS485 Transceiver datasheets | **CONDITIONAL PASS** | STM32 PB10/PB11 are 5V-tolerant (FT). ESP32-S3 GPIOs (8, 18, 5) are strictly 3.3V logic. If 5V MAX485 is used, RO requires a voltage divider / series resistor for ESP32. 3.3V SP3485 is strongly recommended. |
| **SUBAGENT-2 (STM32 Auditor)** | `esp32_uart.c/h`, `main.c/h` | **PASS** | USART3 PB10 (TX), PB11 (RX), PB1 (DE/RE) verified. Hardware TC flag poll `__HAL_UART_GET_FLAG(&huart3, UART_FLAG_TC)` enforces complete shift register flush before DE de-assertion to LOW. |
| **SUBAGENT-3 (ESP32 Auditor)** | `ekran_kontrol.ino` | **PASS** | UART1 GPIO8 (TX), GPIO18 (RX), GPIO5 (DE/RE) verified safe with no strapping/flash conflicts. `rs485Transmit()` helper with 10μs pre-delay, `Serial1.flush()`, and 5μs post-delay implemented. Fixed raw `Serial1.print` call in `stmSetIdBroadcast()`. |
| **SUBAGENT-4 (Protocol Architect)** | ASCII Matrix, `.agents/rules/05-communication.md` | **PASS** | `T<ID>:<COMMAND>` unicast and `T0:<COMMAND>` broadcast matrix, 10-field CSV STAT telemetry, master-slave polling, and CRC16 slotted discovery verified sufficient for half-duplex RS485. |
| **SUBAGENT-5 (Test Engineer)** | `test_rs485_mock.py`, `test_hil_uart.py`, `test_hmi_mock.py` | **PASS** | Audited test suite, added `test_22_rx_silence_watchdog_timeout` for 3000ms silence watchdog abort. Test suite executed with **48 PASSED, 18 SKIPPED (0 Failures)**. |

---

## 3. Datasheet Verification Matrix

| Item | Datasheet Specification | Firmware Configuration | Hardware Interface | Result |
| :--- | :--- | :--- | :--- | :---: |
| **STM32 UART** | USART3 AF7 (PB10=TX, PB11=RX) | `huart3` at 115200 8N1 | Nucleo CN10 Pins 25 & 18 | **PASS** |
| **STM32 DE Pin** | PB1 (GPIO Output Push-Pull, High Speed) | `RS485_DE_Pin` (PB1) | Nucleo CN10 Pin 24 | **PASS** |
| **ESP32 UART** | UART1 (GPIO8=TX, GPIO18=RX) | `Serial1` at 115200 8N1 | ESP32-S3 Pins 8 & 18 | **PASS** |
| **ESP32 DE Pin** | GPIO5 (Output, Initial LOW) | `RS485_DE_PIN 5` | ESP32-S3 Pin 5 | **PASS** |
| **STM32 5V Tolerance** | PB10/PB11 designated as FT (5V tolerant) | Standard HAL Driver | Safe for 3.3V or 5V Transceivers | **PASS** |
| **ESP32 3.3V Limit** | GPIO18 absolute max input = 3.6V | `Serial1` RX Input | 3.3V SP3485 native (or 5V MAX485 + Divider) | **WARNING (Gate 2)** |
| **Bus Load** | Minimum rated transceiver load = 54Ω | Differential Twisted Pair | 120Ω \|\| 120Ω = 60Ω Bus Load | **PASS** |

---

## 4. Final Pinout Assignment

### STM32G474RE Node
- **USART3_TX:** `PB10` (CN10 Pin 25) $\to$ RS485 Module A `DI`
- **USART3_RX:** `PB11` (CN10 Pin 18) $\leftarrow$ RS485 Module A `RO`
- **RS485 DE/RE:** `PB1` (CN10 Pin 24) $\to$ RS485 Module A `DE` & `/RE` (tied together)

### ESP32-S3 Node
- **UART1_TX:** `GPIO8` $\to$ RS485 Module B `DI`
- **UART1_RX:** `GPIO18` $\leftarrow$ RS485 Module B `RO`
- **RS485 DE/RE:** `GPIO5` $\to$ RS485 Module B `DE` & `/RE` (tied together)

---

## 5. Final RS485 Wiring Diagram

```
[STM32 Node]                                            [ESP32 Node]
PB10 (TX) ---> DI | RS485 Module A | RO ---> PB11 (RX)  GPIO8 (TX) ---> DI | RS485 Module B | RO ---> GPIO18 (RX)
PB1 (DE)  ---> DE |                |                    GPIO5 (DE) ---> DE |                |
              VCC | (3.3V Rail)    |                                   VCC | (3.3V Rail)    |
              GND | (Common GND)   |                                   GND | (Common GND)   |
                  +----------------+                                       +----------------+
                          |                                                        |
                        Line A (Non-Inverting) ================================= Line A
                        Line B (Inverting)     ================================= Line B
                        Common GND             --------------------------------- Common GND
                        [ 120Ω Termination ]                                 [ 120Ω Termination ]
```

---

## 6. DE/RE State Machine & Timing

```
          RX IDLE MODE (DE = LOW, /RE = LOW)
                        |
                        | Transmission Requested
                        v
          TX PREAMBLE (DE = HIGH, 5-10 μs Stabilization Delay)
                        |
                        | Start UART Transmit
                        v
          BYTE STREAM TRANSMISSION (115200 8N1)
                        |
                        | Hardware Shift Register Flush Verification:
                        | - STM32: while(__HAL_UART_GET_FLAG(&huart3, UART_FLAG_TC) == RESET)
                        | - ESP32: Serial1.flush()
                        v
          TX POSTAMBLE (5 μs Post-Delay)
                        |
                        v
          RETURN TO RX IDLE MODE (DE = LOW, /RE = LOW)
```

---

## 7. Protocol Architecture Summary

- **Format:** Unicast `T<ID>:<COMMAND>` (ID 1-10) and Broadcast `T0:<COMMAND>`
- **Telemetry:** `STAT,<ID>,<MODE>,<SEC>,<TEMP_X10>,<RELAY>,<PWR>,<FREQ>,<FAULT>,<PROV_STATE>\n` (Strict 10-field CSV)
- **Framing:** Line-terminated (`\r\n` / `\n`), maximum 64 bytes frame length.

---

## 8. Collision & Arbitration Strategy

- **Master-Slave Strict Control:** ESP32 master initiates all commands. Slaves never transmit spontaneously.
- **Slotted Discovery:** Uncommissioned (ID=0) nodes transmit `DISCOVER_ACK` with a CRC16-CCITT backoff delay (0-50 ms) to prevent simultaneous transmission collisions.
- **Turnaround Delay:** Minimum 10 ms master polling silence enforced between slave polling cycles.

---

## 9. Electrical Safety & Power-On Checklist

1. **[ ] Unpowered Continuity Check:** Verify GND-to-GND resistance < 0.5Ω across all nodes and module GND ucs.
2. **[ ] Bus Impedance Check:** Measure resistance across Line A and Line B (expected ~60Ω with two 120Ω resistors).
3. **[ ] Voltage Rail Verification:** Verify 3.3V VCC supply on transceivers before connecting logic pins.
4. **[ ] RO Voltage Inspection:** If using MAX485 (5V), measure RO pin voltage. Ensure it does not exceed 3.3V at ESP32 GPIO18 pin.
5. **[ ] DE Idle State:** Confirm DE pin voltage is 0.0V (LOW) when no transmission is active.

---

## 10. Test Execution Results

- **Command Executed:** `python -m pytest -v`
- **Results:** **48 PASSED, 18 SKIPPED (0 Failures)**
- **Coverage:** Verified direction control timing, slotted discovery backoff, WAL boot persistence, STAT 10-field schema, malformed packet resilience, and 3000ms silence watchdog abort (`test_22_rx_silence_watchdog_timeout`).

---

## 11. Remaining Risks

- Using legacy 5V MAX485 without level shifting on ESP32 RX pin could degrade ESP32-S3 silicon over extended operation. Using 3.3V SP3485 eliminates this risk entirely.

---

## 12. HUMAN GATES

> [!IMPORTANT]
> **HUMAN GATE 1 — TRANSCEIVER VOLTAGE SELECTION**  
> Visual confirmation required: Ensure RS485 transceiver IC is 3.3V native (SP3485 / MAX3485) or confirm voltage divider (2k/3.3k) installation if 5V MAX485 is used on ESP32-S3.

> [!IMPORTANT]
> **HUMAN GATE 2 — PHYSICAL WIRING CONNECTIVITY**  
> Complete physical breadboard jumper wiring between STM32 (PB10, PB11, PB1), ESP32 (GPIO8, GPIO18, GPIO5), and RS485 modules according to Section 4.

---

## 13. Final Verdict

```text
================================================================================
FINAL VERDICT: PASS WITH WARNINGS
(Firmware & Protocol Audited 100% PASS; Pending Physical Hardware Gate Connection)
================================================================================
```
