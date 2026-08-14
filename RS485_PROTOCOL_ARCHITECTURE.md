# EAGLEULTRASONİK — RS485 PROTOCOL ARCHITECTURE & ARBITRATION SPECIFICATION

**Document Version:** 1.0.0  
**Phase:** Phase 5.2 — RS485 Physical Communication Implementation  
**Date:** 2026-08-11  

---

## 1. Protocol Layering Model

```
+-------------------------------------------------------------------------+
|                        APPLICATION LAYER                                |
|  - ESP32: HMI State Machine, NVS Recipe Storage, Tank Provisioning      |
|  - STM32: soft-start PWM, PT100 ADC, Triac/Heater, Safety Interlock     |
+-------------------------------------------------------------------------+
                                    |
+-------------------------------------------------------------------------+
|                        PROTOCOL LAYER (ASCII Matrix)                    |
|  - Address Framing: T<ID>:<COMMAND> (Unicast T1..T10, Broadcast T0)     |
|  - Telemetry Frame: STAT,<ID>,<MODE>,<SEC>,<TEMP*10>,<RELAY>,<PWR>,...  |
|  - Line Termination: \r\n (CRLF) or \n, 64 bytes maximum frame length   |
+-------------------------------------------------------------------------+
                                    |
+-------------------------------------------------------------------------+
|                        RS485 TRANSPORT LAYER                            |
|  - Direction Management: Driver Enable (DE) & Receiver Enable (/RE)     |
|  - Transmission Complete (TC) Verification                              |
|  - Bus Arbitration & Turnaround Delays                                  |
+-------------------------------------------------------------------------+
                                    |
+-------------------------------------------------------------------------+
|                        HARDWARE / DRIVER LAYER                          |
|  - STM32: USART3 (115200 8N1) + GPIO PB1 (DE)                           |
|  - ESP32: UART1 (115200 8N1) + GPIO5 (DE)                               |
+-------------------------------------------------------------------------+
```

---

## 2. Address & Command Matrix

| Frame Format | Type | Initiator | Target | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| `T<ID>:SET_POWER=<0-100>` | Unicast | ESP32 Master | STM32 Node `<ID>` | Set ultrasonic generator power level |
| `T<ID>:SET_TEMP=<0-90>` | Unicast | ESP32 Master | STM32 Node `<ID>` | Set bath temperature setpoint |
| `T<ID>:SET_TIME=<0-100>` | Unicast | ESP32 Master | STM32 Node `<ID>` | Set process timer duration (minutes) |
| `T<ID>:SET_FREQ=<28|40>` | Unicast | ESP32 Master | STM32 Node `<ID>` | Switch frequency matching step |
| `T<ID>:START` | Unicast | ESP32 Master | STM32 Node `<ID>` | Start ultrasonic generator & heater cycle |
| `T<ID>:STOP` | Unicast | ESP32 Master | STM32 Node `<ID>` | Emergency stop / Clear active fault |
| `T0:DISCOVER` | Broadcast | ESP32 Master | Uncommissioned Nodes | Discover unconfigured slave nodes |
| `T0:ASSIGN_ID:<ID>:<UID>`| Broadcast | ESP32 Master | Target Node `<UID>` | Assign persistent node ID |
| `STAT,<ID>,<MODE>,...` | Telemetry | STM32 Node `<ID>` | ESP32 Master | Standardized 10-field telemetry response |

---

## 3. Bus Arbitration & Timing Parameters

1. **Master Polling Turnaround Gap ($t_{\text{poll}}$):** $\ge 10\text{ ms}$ silence between master transmissions.
2. **Driver Enable Preamble ($t_{\text{pre}}$):** $\ge 5\ \mu\text{s}$ DE assertion before emitting start bit.
3. **Driver Enable Postamble ($t_{\text{post}}$):** Verified `TC` flag + $5\ \mu\text{s}$ before DE de-assertion.
4. **Collision Avoidance:** Slave nodes response disabled unless explicitly addressed by matching `T<ID>:`.
