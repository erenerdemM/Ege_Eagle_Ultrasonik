# EAGLEULTRASONiK Phase 5.0 — Human Gate Analysis & Decision Audit

> **Document Version:** 5.0.0-GATE  
> **Status:** Official Human Gate Audit & Engineering Decision Protocol  
> **Author:** Adversarial Reviewer for Safety-Critical Embedded Systems  
> **Target Subsystems:** Entire Control Architecture (STM32G474RE, ESP32-S3, RS485 Bus, Nextion HMI, Power Stage)  
> **Target File:** `C:\Users\ern0e\EAGLEULTRASONiK\.agent\reports\phase-5.0-human-gate.md`  

---

## 1. Executive Summary & Purpose of Human Gate Analysis

In safety-critical industrial automation, automated code analysis, static linters, and simulated unit tests cannot substitute for **Human Engineering Responsibility**. Software modifications that control 16A thermal elements, 500W ultrasonic piezoceramic drive circuits, and high-voltage AC zero-cross triac firing directly impact physical equipment safety, electrical fire hazards, and operator security.

The **Human Gate Analysis** establishes a strict, uncompromising boundary: **No code implementation or refactoring in Phase 5.0 may be merged into production branches without explicit, recorded human sign-off on the 6 Mandatory Design Gates detailed in this report.**

---

## 2. Mandatory Human Decision Gates

Each gate presents the engineering context, trade-offs, hazard if unapproved, and mandatory decision choices required from the Human System Architect.

---

### 2.1 Decision Gate 1: Enforcement of Production Boot Mode Baseline (`BENCH_DEV_MODE_ID = 0`)

- **Context:** In existing [`main.c:L53`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L53), `#define BENCH_DEV_MODE_ID 1` forces `MY_TANK_ID = 1`, bypassing DIP switches and Flash reads.
- **Trade-off:**
  - *Dev Mode (ID = 1):* Allows quick bench testing on a single board without setting DIP switches or programming Flash.
  - *Production Mode (ID = 0):* Enforces Flash Page 127 override (`TankId_Load()`) and physical DIP switch reading (`ReadDipSwitchId()`). Uncommissioned boards enter `UNCOMMISSIONED` state (`MY_TANK_ID = 0`).
- **Hazard if Denied:** Flashing production firmware with `BENCH_DEV_MODE_ID 1` causes multiple replacement boards connected to an active RS485 bus to clash on ID 1, driving transceiver line contention and crashing bus communications.
- **Mandatory Human Action:** Approve setting `#define BENCH_DEV_MODE_ID 0` for all Phase 5.0 builds.
- **Decision:** `[ ] APPROVED` | `[ ] REJECTED`

---

### 2.2 Decision Gate 2: Adoption of Heater Relay Guard Timers (10s Min ON / 10s Min OFF)

- **Context:** Existing [`heater_relay.c`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/heater_relay.c) lacks Minimum ON and Minimum OFF guard timers. Analog noise near setpoint causes relay chatter.
- **Trade-off:**
  - *10s Guard Timers:* Protects mechanical relay contacts from rapid switching (chatter), extending contact life from a few days to 10+ years. Increases thermal overshoot slightly ($\pm 1.5^\circ\text{C} \dots \pm 2.5^\circ\text{C}$) due to thermal inertia.
  - *Unprotected Hysteresis:* Faster temperature correction, but rapid relay chatter under sensor noise causes contact arcing, welding, and fire risk.
- **Safety Precedence:** Emergency process stop (`mode != SYS_MODE_RUNNING` or `FAULT`) immediately forces relay `OFF`, completely bypassing `HEATER_MIN_ON_TIME_MS`.
- **Mandatory Human Action:** Approve 10-second minimum ON/OFF guard timer parameters.
- **Decision:** `[ ] APPROVED` | `[ ] REJECTED`

---

### 2.3 Decision Gate 3: Activation of Hardware IWDG (1000ms) & RX Silence Watchdog (3000ms)

- **Context:** Existing STM32 firmware has no hardware watchdog (IWDG) initialized, and no communication timeout if the ESP32 serial link breaks during active washing.
- **Trade-off:**
  - *With Watchdogs:* If MCU hangs or serial cable disconnects during `SYS_MODE_RUNNING`, system forcibly shuts down outputs within 1000 ms (IWDG) or 3000 ms (RX timeout).
  - *Without Watchdogs:* Code continues running unmonitored; MCU lockup with `PB15 HIGH` causes fluid boil-off and thermal runaway.
- **Mandatory Human Action:** Approve 1000 ms hardware IWDG enablement in `main.c` and 3000 ms UART RX silence timeout in `esp32_uart.c`.
- **Decision:** `[ ] APPROVED` | `[ ] REJECTED`

---

### 2.4 Decision Gate 4: Standardizing `EAGLE-PROV-v2` Provisioning Protocol

- **Context:** Uncommissioned STM32 boards require a deterministic, collision-free method for assigning Flash-stored Tank IDs ($1..10$) over a multi-drop RS485 bus.
- **Trade-off:**
  - *Option A (Manual Sequential Plug-in):* Boards must be plugged into the bus one at a time by an operator. Simple, but prone to human operator error.
  - *Option B (EAGLE-PROV-v2 Standard):* Uses 96-bit MCU UIDs, slotted ALOHA backoff, temporary `T99` staging, and binary tree partitioning. Fully automated and collision-safe, but requires structured state machine code on ESP32 and STM32.
- **Mandatory Human Action:** Approve adoption of `EAGLE-PROV-v2` as the standard commissioning protocol.
- **Decision:** `[ ] APPROVED` | `[ ] REJECTED`

---

### 2.5 Decision Gate 5: Removal of `__disable_irq()` Interrupt Blackout in X9C103S Driver

- **Context:** `X9C103S_SetStep()` in [`x9c103s.c:L94`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/x9c103s.c#L94) invokes `__disable_irq()`, disabling global interrupts for **520 µs** during frequency changes.
- **Trade-off:**
  - *With `__disable_irq()`:* Guarantees exact pulse timing to X9C digital pot, but blocks EXTI zero-cross interrupts (causing triac mis-firing) and UART RX interrupts (causing byte overruns).
  - *Without `__disable_irq()`:* Eliminates zero-cross and UART interrupt blackouts. Step timing is maintained via microsecond delay loops without disabling global interrupts.
- **Mandatory Human Action:** Approve removing global `__disable_irq()` from `X9C103S_SetStep()`.
- **Decision:** `[ ] APPROVED` | `[ ] REJECTED`

---

### 2.6 Decision Gate 6: Service Role Security & Dual-Layer Runtime Reconfiguration Lock

- **Context:** Reconfiguring parameters (`SET_ID`, `SET_FREQ`, `HEATER_MODE`) while process is in `SYS_MODE_RUNNING` damages mechanical relays and piezoceramic transducers. Keypad password `"123456"` is hardcoded in RAM.
- **Trade-off:**
  - *Cryptographic Auth + Runtime Lock:* Replaces plain-text static PIN with dynamic HMAC Challenge-Response (or PIN backoff), and enforces dual-layer checks on both ESP32 and STM32 to block configuration edits during active washing.
  - *Legacy Plain-Text PIN:* Trivial to bypass; allows mid-process parameter mutations.
- **Mandatory Human Action:** Approve dynamic HMAC challenge-response authentication and dual-layer runtime configuration locking.
- **Decision:** `[ ] APPROVED` | `[ ] REJECTED`

---

## 3. Formal Pre-Implementation Human Sign-Off Audit Table

The following sign-off table must be completed and signed by the designated Lead Systems Architect prior to executing Phase 5.0 code changes:

```
+---------------------------------------------------------------------------------------------------+
| PHASE 5.0 MANDATORY HUMAN ENGINEERING SIGN-OFF AUDIT TABLE                                        |
+----+----------------------------------------------+--------------------+--------------------------+
| Gate | Decision Gate Description                  | Human Status       | Engineer Signature / Date|
+----+----------------------------------------------+--------------------+--------------------------+
| Gate 1 | Production Dev Mode Removal (`BENCH_DEV_MODE_ID = 0`) | [ ] APPROVED     | ________________________ |
| Gate 2 | Heater Relay Guard Timers (10s Min ON/OFF) | [ ] APPROVED       | ________________________ |
| Gate 3 | Hardware IWDG (1000ms) & RX Silence Wdt  | [ ] APPROVED       | ________________________ |
| Gate 4 | EAGLE-PROV-v2 Commissioning Protocol     | [ ] APPROVED       | ________________________ |
| Gate 5 | X9C103S IRQ Blackout Removal             | [ ] APPROVED       | ________________________ |
| Gate 6 | Service Auth & Runtime Reconfig Lock     | [ ] APPROVED       | ________________________ |
+----+----------------------------------------------+--------------------+--------------------------+
```

### Formal Human Acceptance Statement:
> *"I hereby certify that I have reviewed the Phase 5.0 Risk Register, Implementation Order, and Human Gate Analysis. I approve the technical decisions checked above and authorize the commencement of Phase 5.0 First Implementation Package code changes under the strict boundary constraints specified."*

**Lead Systems Architect Name:** ___________________________  
**Date:** ___________________________  
**Signature:** ___________________________  

---

## 4. Post-Implementation Hardware Verification Gates (Physical Testing)

Once First Implementation Package code changes are completed, the following 4 physical hardware verification gates MUST be passed before production release:

1. **Gate HW-1 (Oscilloscope Zero-Cross & Triac Gate Verification):** Verify on a dual-channel oscilloscope that `PC6` Triac Gate pulse fires at exact target delay after `PC7` Zero-Cross edge without jitter when X9C frequency changes are executed.
2. **Gate HW-2 (Thermal Camera & Relay Chatter Test):** Monitor `PB15` Heater Relay contacts under full 16A resistive load with a thermal imaging camera for 2 hours. Confirm zero contact chatter and contact temperature rise $\Delta T < 25^\circ\text{C}$.
3. **Gate HW-3 (RS485 Bus Contention & Noise Test):** Attach 4 STM32 slave boards to the RS485 bus. Verify zero frame errors (`USART_ISR_FE`) and zero transceiver collision spikes on bus differential voltage ($V_A - V_B$).
4. **Gate HW-4 (IWDG Lockup Recovery Test):** Force software crash via debug command; confirm MCU resets within $1000 \pm 50 \text{ ms}$ and `PB15` relay drops immediately.
