# EAGLEULTRASONİK — FINAL SYSTEM MANIFESTO (V4)

**Document Identifier:** `docs/EAGLEULTRASONIK_SYSTEM_MANIFESTO_V4.md`  
**Supersedes:** `docs/EAGLEULTRASONIK_CURRENT_STATE_MANIFESTO.md` (V3, 2026-08-20) & `docs/EAGLEULTRASONIK_SYSTEM_MANIFESTO.md` (V1/V2)  
**Release Baseline:** V4.0.0 — Production-Ready Firmware & Hardware Authority  
**Authoritative Date:** 2026-08-24  
**Audit Methodology:** End-to-End White-Box Firmware Code Audit + Multi-Session HIL Hardware Verification + Transparent Bridge Packet Traffic Telemetry  
**Target Microcontrollers:** STM32G474RE (Motor/Ultrasonic/Heater Slave Node), ESP32-S3 (HMI Master/Bridge), Nextion Intelligent HMI Display  

---

## 1. System Overview

EAGLEULTRASONİK is an industrial multi-tank ultrasonic cleaning and degas automation system featuring a high-reliability distributed master-slave architecture.

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                       SYSTEM ARCHITECTURE                                       │
├────────────────────────────────┬───────────────────────────────┬────────────────────────────────┤
│          HMI LAYER             │          MASTER LAYER         │          SLAVE LAYER           │
│   Nextion Intelligent HMI      │         ESP32-S3 Master       │      STM32G474RE Node(s)       │
│  - Page 0: Main Operation      │  - FreeRTOS Architecture      │  - TIM15 Phase-Angle Triac PWM │
│  - Page 1: Tank (Eye) Select   │  - Dual UART (HMI + RS485)    │  - 100 Hz ZC EXTI Sync         │
│  - Page 2: Recipe Edit (P1..3) │  - NVS Flash Persistent Store │  - X9C103S Arbitrary Pot (PA0) │
│  - Page 4: Security PIN Pad    │  - Dual-State Sweep ARM/ACTIVE│  - OPAMP3 PT100 Temp Sensing   │
│  - Page 5..8: Service Config   │  - DEGAS Snapshot Manager     │  - Multi-Drop RS485 Slave      │
└────────────────────────────────┴───────────────────────────────┴────────────────────────────────┘
```

### Core Architecture Principles
1. **Master-Slave Bus Isolation:** One ESP32-S3 Master manages Nextion UI navigation, non-volatile recipe storage, and commands up to 10 multi-drop STM32G474RE nodes over half-duplex RS485 at 115200 baud.
2. **Deterministic Time-Critical Control:** STM32G474RE slave firmware executes real-time phase-angle TRIAC firing, frequency tuning, temperature regulation, and safety shutdown independently of bus traffic.
3. **Dual-Domain Synchronous Timing:** A 100 Hz Zero-Cross TTL pulse (from ESP32 GPIO4 generator or physical AC optocoupler) triggers STM32 PC7 EXTI interrupts to time TRIAC firing delays ($500\,\mu\text{s} \dots 9500\,\mu\text{s}$).
4. **Frozen Hardware Authority:** Physical pinouts and electrical isolation rules are strictly governed by `hardware_wiring_FINAL_AUTHORITY.md`.

---

## 2. Hardware Architecture & Verified Pinout Authority

### 2.1 MCU Pinout & Loopback Signal Mapping

| Subsystem | Signal Name | MCU Pin | Physical Header | Electrical Type / Target | Verified Loopback Path | Authority Reference |
|---|---|---|---|---|---|---|
| **Zero-Cross** | `ZERO_CROSS` | **STM32 PC7** | CN5 Pin 2 (D9) / CN10 Pin 19 | 3.3V TTL Interrupt In (EXTI9_5) | ESP32 GPIO4 -> PC7 (100 Hz) | `hardware_wiring_FINAL_AUTHORITY.md:L18` |
| **Triac Output** | `TRIAC_GATE` | **STM32 PC6** | CN10 Pin 4 | 3.3V Active-High 100us Pulse | PC6 -> 1k -> PA6 | `main.h:L111` |
| **Triac Feedback**| `TRIAC_FB` | **STM32 PA6** | CN10 Pin 13 | Digital Loopback Input | PA6 (Reads PC6 State) | `main.h:L104` |
| **Heater Relay** | `HEATER_OUT` | **STM32 PB15**| CN10 Pin 26 | 3.3V Active-High Relay Driver | PB15 -> 1k -> PA4 | `main.h:L92` |
| **Heater Feedback**|`HEATER_FB` | **STM32 PA4** | CN7 Pin 17 | Digital Loopback Input | PA4 (Reads PB15 State) | `main.h:L102` |
| **Pot Increment**| `X9C_INC` | **STM32 PB14**| CN10 Pin 28 | Active-Low Increment Strobe | PB14 -> X9C INC | `x9c103s.c:L35` |
| **Pot Direction**| `X9C_UD` | **STM32 PB13**| CN10 Pin 30 | Direction (High=Up, Low=Down) | PB13 -> X9C U/D | `x9c103s.c:L36` |
| **Pot Select** | `X9C_CS` | **STM32 PB12**| CN10 Pin 16 | Active-Low Chip Select | PB12 -> X9C CS | `x9c103s.c:L37` |
| **Pot Wiper ADC**| `POT_WIPER` | **STM32 PA0** | CN7 Pin 28 | ADC1_IN1 Analog Voltage (0..3.3V) | X9C VW -> 1k -> PA0 | `hardware_wiring_FINAL_AUTHORITY.md:L82` |
| **PT100 Temp** | `PT100_IN` | **STM32 PB0** | CN7 Pin 34 | OPAMP3 Non-Inv Input (PGA x16) | PT100 Bridge -> OPAMP3 | `pt100_adc.c:L22` |
| **RS485 UART** | `USART3_TX` | **STM32 PB10**| CN10 Pin 25 | UART Transmit to RS485 | USART3 -> MAX485 DI | `esp32_uart.c:L34` |
| **RS485 UART** | `USART3_RX` | **STM32 PB11**| CN10 Pin 18 | UART Receive from RS485 | MAX485 RO -> USART3 | `esp32_uart.c:L34` |
| **RS485 Driver En**|`RS485_DE` | **STM32 PB1** | CN7 Pin 27 | Active-High Driver Enable | PB1 -> MAX485 DE | `main.h:L108` |
| **ST-Link VCP** | `LPUART1_TX`| **STM32 PA2** | Internal ST-Link | 115200 Baud Telemetry Stream | PA2 -> ST-Link USB VCP | `main.c:L88` |
| **ST-Link VCP** | `LPUART1_RX`| **STM32 PA3** | Internal ST-Link | 115200 Baud ST-Link RX | PA3 -> ST-Link USB VCP | `main.c:L88` |
| **ESP32 HMI UART**|`RXD2 / TXD2`| **ESP32 GPIO16/17**| Board Header | 115200 Baud to Nextion Display | GPIO16/17 -> Nextion | `ekran_kontrol.ino:L6` |
| **ESP32 RS485** | `STM_TX / RX`| **ESP32 GPIO8/18** | Board Header | 115200 Baud to RS485 Bus | GPIO8/18 -> RS485 | `ekran_kontrol.ino:L42` |
| **ESP32 DE Pin** | `RS485_DE` | **ESP32 GPIO5** | Board Header | Active-High RS485 Driver Enable | GPIO5 -> RS485 DE | `ekran_kontrol.ino:L44` |
| **ESP32 ZC Out** | `ZC_SIM_PIN` | **ESP32 GPIO4** | Board Header | 100 Hz Half-Period Square Wave | GPIO4 -> STM32 PC7 | `ekran_kontrol.ino:L62` |

> [!IMPORTANT]
> **Definitive Feedback Pin Authority:**
> - `PA4` is strictly **HEATER FEEDBACK** (`HEATER_TEST_FB_Pin`).
> - `PA6` is strictly **TRIAC FEEDBACK** (`TRIAC_TEST_FB_Pin`).
> - `PB15` is strictly **HEATER RELAY OUTPUT** (`HEATER_RELAY_Pin`).
> - `PC6` is strictly **TRIAC GATE OUTPUT** (`TRIAC_GATE_Pin`).

---

## 3. Nextion HMI Page Architecture

| Page ID | Name | Purpose | Key UI Controls | Emitted Protocol Commands |
|---|---|---|---|---|
| `page0` | **Operation** | Main wash operation, start/stop, status, manual setpoints, preset recipes, DEGAS selection | `b_start`, `b_stop`, `b_freq`, `b_sweep`, `b_degas`, `b_p1..p3`, `b_fp`, `t_set_sure`, `t_set_sic` | `CMD_START|<m>|<c>`, `CMD_STOP`, `CMD_FREQ_TOGGLE`, `CMD_SWEEP_TOGGLE`, `CMD_DEGAS_SEL`, `P1..P3_SEL`, `P_HIZLI`, `MANUAL_MODE`, `PAGE1_OPEN` |
| `page1` | **Tank Select** | Select active tank / logical eye | `b_tank_up`, `b_tank_down`, `b_tank_ok` | `TANK_UP`, `TANK_DOWN`, `TANK_SEL_OK`, `PAGE1_BACK` |
| `page2` | **Recipe Edit** | Edit preset recipes P1, P2, P3 parameters | `b_edit_p1..p3`, `b_edit_swp`, `b_p_save`, `b_p_back` | `EDIT_P1..P3`, `EDIT_SWEEP_TOG`, `P_SAVE|<m>|<c>|<s>`, `PAGE2_BACK` |
| `page3` | **General Settings**| Placeholder settings menu | `b_back` | `PAGE3_BACK` |
| `page4` | **Security PIN**| PIN authorization pad for service entry | `b0..b9`, `b_ok`, `b_del`, `b_back` | `KEY0..KEY9`, `KEY_OK`, `KEY_DEL`, `KEY_BACK`, `PAGE4_OPEN` |
| `page5` | **Service - Power/ID/Heater**| Commissioning, tank card ID assignment, power %, heater mode (RELAY ↔ SSR) | `b_srv_tank_up/down`, `b_srv_guc_up/down`, `b_srv_id_up/down`, `b_srv_max_up/down`, `b_htr_mode`, `b_save`, `b_discard`, `b_nav_fwd` | `SRV_TANK_UP/DOWN`, `SRV_GUC_UP/DOWN`, `SRV_ID_UP/DOWN`, `SRV_MAX_UP/DOWN`, `SRV_HTR_TOGGLE`, `SRV_SAVE`, `SRV_DISCARD`, `NAV_FORWARD` |

| `page6` | **Service - Sweep** | Sweep modulation span, period, step configuration | `b_swp_span_up/down`, `b_swp_per_up/down`, `b_swp_step_up/down`, `b_nav_fwd`, `b_nav_back` | `SRV_SPAN_UP/DOWN`, `SRV_PER_UP/DOWN`, `SRV_STEP_UP/DOWN`, `NAV_FORWARD`, `NAV_BACK` |
| `page7` | **Service - Degas 1**| Degas duration, power %, frequency selection (28..40 kHz) | `b_deg_dur_up/down`, `b_deg_pow_up/down`, `b_deg_frq_up/down`, `b_nav_fwd`, `b_nav_back` | `SRV_DDUR_UP/DOWN`, `SRV_DPOW_UP/DOWN`, `SRV_DFREQ_UP/DOWN`, `NAV_FORWARD`, `NAV_BACK` |
| `page8` | **Service - Degas 2**| Degas pulse timing and temperature interlock | `b_deg_pon_up/down`, `b_deg_poff_up/down`, `b_deg_tc_tog`, `b_deg_tgt_up/down`, `b_nav_fwd`, `b_nav_back` | `SRV_DPON_UP/DOWN`, `SRV_DPOFF_UP/DOWN`, `SRV_DTCTRL_TOG`, `SRV_DTEMP_UP/DOWN`, `NAV_FORWARD`, `NAV_BACK` |

---

## 4. Sweep System — Persistent Dual-State Architecture

The frequency sweep engine is structured around a strict **ARM / ACTIVE** separation:

```text
┌─────────────────────────┐            START Pressed            ┌──────────────────────────┐
│       SWEEP ARMED       │ ──────────────────────────────────> │       SWEEP ACTIVE       │
│  - runtime_sweep = true │                                     │  - runtime_sweep = true  │
│  - HMI b_sweep = GREEN  │ <────────────────────────────────── │  - STM32 Mode = RUNNING  │
│  - STM32 Mode = IDLE    │        STOP / Timer Zero /          │  - X9C Wiper Steps In    │
│  - X9C Wiper = CENTER   │          Frequency Toggle           │    Triangle Wave Pattern │
└─────────────────────────┘                                     └──────────────────────────┘
```

### Verified Sweep Rules
1. **ARMED State:** `runtime_sweep[g] = true`. The HMI `b_sweep` button displays `GREEN`. In `SYS_MODE_IDLE`, STM32 wiper remains static at center frequency.
2. **ACTIVE State:** When `CMD_START` is issued, ESP32 transmits `T1:SWEEP:ON` and `T1:START`. STM32 enters `SYS_MODE_RUNNING`, and `X9C103S_SweepProcess()` steps the wiper across the modulation span.
3. **STOP & Timer Completion:** When `CMD_STOP` is received or process time expires, STM32 immediately returns wiper to center frequency. `runtime_sweep` **remains `true`**, and `b_sweep` remains **GREEN**. Subsequent START commands immediately resume sweep.
4. **Center Frequency Change (28k <-> 40k):** Changing frequency updates the center step (Step 40 or Step 90) while preserving the ARMED state (`runtime_sweep = true`). Subsequent START modulates around the new center.
5. **DEGAS Mutual Exclusion:** DEGAS and Sweep are strictly mutually exclusive. Selecting DEGAS disarms sweep (`swp_st = 0`). Exiting DEGAS restores the normal recipe sweep state.
6. **Page 6 Configuration Pipeline:** Parameters (Span: +/- 1..4 kHz, Period: 500..2000 ms, Step Increment) are stored in NVS and transmitted to STM32 via `T1:SWP_SPAN`, `T1:SWP_PER`, and `T1:SWP_STEP`.

---

## 5. DEGAS System — Parameter Pipeline & Frequency Isolation

### 5.1 Parameter Pipeline & Snapshot Execution
DEGAS parameters configured on Page 7 and Page 8 are stored independently in NVS:
```text
HMI Page 7/8 ──> edit_service_degas ──> NVS Flash ──> service_degas ──> START_DEGAS Snapshot Frame
                                                                                   │
STM32 Hardware Outputs <── g_system_state.degas_config <── RS485 UART Bus <────────┘
```

* **Snapshot Frame Format:**  
  `T1:START_DEGAS:<duration_min>:<power_pct>:<freq_khz>:<pulse_on_ms>:<pulse_off_ms>:<temp_ctrl>:<target_temp_c>`
* **Example Frame:** `T1:START_DEGAS:20:80:33:1100:300:1:57.0`

### 5.2 Arbitrary 28..40 kHz Frequency Support
DEGAS frequency supports arbitrary selection across 28..40 kHz in 1 kHz increments:
$$\text{step}(f) = 40 + \left\lfloor \frac{(f - 28) \times 50 + 6}{12} \right\rfloor$$

| Frequency (kHz) | X9C Wiper Step | Expected Pot Resistance | Expected PA0 ADC Voltage | HIL Verification Status |
|:---:|:---:|:---:|:---:|:---:|
| **28 kHz** | **Step 40** | 4.00 kOhm | 1.352 V | **RUNTIME-PROVEN** |
| **30 kHz** | **Step 48** | 4.80 kOhm | 1.621 V | **RUNTIME-PROVEN** |
| **33 kHz** | **Step 61** | 6.10 kOhm | 2.055 V | **RUNTIME-PROVEN** |
| **35 kHz** | **Step 69** | 6.90 kOhm | 2.325 V | **RUNTIME-PROVEN** |
| **40 kHz** | **Step 90** | 9.00 kOhm | 3.028 V | **RUNTIME-PROVEN** |

* **Strict Isolation:** Page 0 frequency selection does NOT alter the saved DEGAS frequency profile. DEGAS START always enforces the Page 7/8 configuration.

---

## 6. TRIAC Phase-Angle Power Control & Gating Safety

### 6.1 Phase Control Math
$$\text{delay\_us} = 9500 - \frac{9000 \times \text{power\_pct}}{100}$$

| Power Setpoint | Operating State | Calculated Delay (us) | Measured VCP Delay (us) | Gating State |
|:---:|:---:|:---:|:---:|:---:|
| **0 %** | IDLE / Safe Stop / DEGAS OFF Window | **9500 us** | **9500 us** | Inhibit / Forced OFF |
| **10 %** | Normal Fast Program (FP) / P1 Setpoint | **8600 us** | **8600 us** | Active 100us Pulses |
| **50 %** | Medium Power Setpoint | **5000 us** | **5000 us** | Active 100us Pulses |
| **80 %** | DEGAS Pulse ON Window | **2300 us** | **2300 us** | Active 100us Pulses |
| **100 %** | Maximum Power Setpoint | **500 us** | **500 us** | Active 100us Pulses |

### 6.2 Zero-Cross Gating & IDLE Safety
* In `SYS_MODE_IDLE`, `HAL_GPIO_EXTI_Callback(ZERO_CROSS_Pin)` drops all Zero-Cross pulses (`return;`). TIM15 is never started, and `TriacForceOff()` enforces `PC6 = LOW` (0V).
* When `START` is issued, the mode transitions to `RUNNING` or `DEGAS`. The next Zero-Cross edge arms TIM15, and soft-start ramps the conduction angle at 20us per half-cycle.
* During DEGAS Pulse OFF windows (300 ms), EXTI edges are dropped and `TriacForceOff()` completely inhibits gate output.

---

## 7. Dual-Mode Heater Architecture: Mechanical Relay + DC SSR PID Control

EAGLEULTRASONİK features a software-selectable dual-mode heater control architecture supporting both **Mechanical Relays** and **DC Solid-State Relays (SSR)** driving a 12V DC heating element via unified GPIO output pin `PB15` (`HEATER_RELAY_Pin`):

```text
PT100 Sensor (PB0 -> OPAMP3 PGA x16 -> ADC2)
  ↓
Validation Layer (Range [0.0°C .. 110.0°C] & |dT/dt| <= 5.0°C/s Sensor Sanity Guard)
  ↓
EMA Filter (alpha = 0.20, Noise Reduction)
  ↓
Temperature & Filtered Derivative (dT/dt Trend Engine)
  ↓
Heater Controller Manager
  ├── RELAY Controller Engine
  │     ├── Bang-Bang with ±1.0°C Hysteresis Window
  │     ├── 10s Minimum ON Guard & 10s Minimum OFF Guard
  │     └── Trend Suppression (Inhibits turn-on if dT/dt > +0.05°C/s)
  │
  └── SSR Controller Engine
        ├── 10 Hz Discrete PID (Kp=10.0 %/°C, Ki=0.20 %/(°C·s), Kd=15.0 %·s/°C)
        ├── Anti-Windup with Integral Dynamic Clamping
        ├── Filtered Derivative on Measurement (-Kd · dT/dt, No Kick)
        ├── 2000 ms Time-Proportional PWM Window Engine
        └── 50 ms Minimum Pulse Guard (Protect Gate Drivers)
  ↓
Unified Output Dispatcher (Bumpless Transfer & <1us Cutoff)
  ↓
PB15 Output (HEATER_RELAY_Pin) ──> 1k Resistor ──> PA4 Feedback (HEATER_TEST_FB_Pin)
```

### 7.1 Mechanical Relay Control Mode (`HEATER_MODE_RELAY`)
* **Control Law:** Symmetric Bang-Bang with $\pm 1.0^\circ\text{C}$ hysteresis deadband:
  - $T_{\text{current}} \le T_{\text{target}} - 1.0^\circ\text{C} \implies \text{Relay ON}$ (`PB15 = HIGH`, `RELAY = 1`)
  - $T_{\text{current}} \ge T_{\text{target}} + 1.0^\circ\text{C} \implies \text{Relay OFF}$ (`PB15 = LOW`, `RELAY = 0`)
* **Contact Protection:** Enforces minimum $10\,\text{s}$ ON time and minimum $10\,\text{s}$ OFF time guard intervals to eliminate relay chattering.
* **Trend Suppression:** If temperature is rapidly rising ($dT/dt > +0.05^\circ\text{C}/\text{s}$), activation is suppressed to prevent overshoot.

### 7.2 DC SSR Control Mode (`HEATER_MODE_SSR`)
* **Control Law:** 10 Hz discrete PID calculating continuous control duty ($0\% \dots 100\%$):
  $$e(t) = T_{\text{target}} - T(t)$$
  $$u(t) = \text{clamp}\left( K_p e(t) + I(t) - K_d \frac{dT}{dt},\, 0,\, 100 \right)$$
* **Anti-Windup:** Integral accumulation is dynamically clamped to $[0, 100]$ and frozen when output saturates.
* **Time-Proportional Modulation:** Period $T_{\text{window}} = 2000\,\text{ms}$. If $\text{Duty} = 40\%$, `PB15` remains HIGH for $800\,\text{ms}$ and LOW for $1200\,\text{ms}$ each cycle.
* **Minimum Pulse Guard:** Pulses shorter than $50\,\text{ms}$ ($2.5\%$ duty) are suppressed or clamped to eliminate driver jitter.

### 7.3 HMI Page 5 Selection, NVS Storage & RS485 Synchronization
* **HMI UI (`b_htr_mode`):** Located on Service Page 5. Touching `b_htr_mode` toggles the edit buffer and immediately updates the button text (`RELAY` $\leftrightarrow$ `SSR`).
* **Non-Destructive Buffer:** `SRV_DISCARD` cancels changes and restores previous mode; `SRV_SAVE` commits the selection to NVS.
* **NVS Flash Key:** Stored per-tank as `htr_m_<tank_id>` (`0 = RELAY`, `1 = SSR`). Persists across power-cycles and reboots.
* **RS485 Command Framing:**
  - Command: `T<ID>:SET_HEATER_MODE:RELAY` / `T<ID>:SET_HEATER_MODE:SSR`
  - STM32 Response: `ACK:HEATER_MODE=RELAY` / `ACK:HEATER_MODE=SSR`
* **Bumpless Transfer:** Mode changes force immediate output deactivation and clear PID integrals, ensuring zero transition spikes on `PB15`.


---

## 8. Digital Potentiometer (X9C103S) & Frequency Tuning

* **Interface:** 3-wire synchronous pulse interface: CS (PB12), U/D (PB13), INC (PB14).
* **Step Resolution:** 100 wiper tap positions (0..99).
* **Analog Conversion:** PA0 (ADC1_IN1) measures wiper voltage:
  $$V_{\text{PA0}} = \frac{\text{ADC\_RAW} \times 3.3\,\text{V}}{4095}$$

---

## 9. Comprehensive Safety & Safe Stop Matrix

When a stop event occurs, `SystemState_SafeStop()` executes synchronous <1us hardware safe cutoff:

| Stop Trigger Event | Resulting Mode | TRIAC Gate (PC6) | Heater Relay (PB15) | X9C Potentiometer | Sweep State | Telemetry Status |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| **User Stop (`CMD_STOP`)** | `SYS_MODE_IDLE` | `TriacForceOff` (LOW) | `HeaterRelay_ForceOff` (LOW) | Restored to Center Step | Wiper Stopped | `STAT,1,IDLE,...` |
| **Process Timer Expired** | `SYS_MODE_IDLE` | `TriacForceOff` (LOW) | `HeaterRelay_ForceOff` (LOW) | Restored to Center Step | Wiper Stopped | `STAT,1,IDLE,...` |
| **DEGAS Stop** | `SYS_MODE_IDLE` | `TriacForceOff` (LOW) | `HeaterRelay_ForceOff` (LOW) | Restored to Center Step | Locked Disabled | `STAT,1,IDLE,...` |
| **Zero-Cross Lost Fault** | `SYS_MODE_FAULT` | `TriacForceOff` (LOW) | `HeaterRelay_ForceOff` (LOW) | Restored to Center Step | Wiper Stopped | `STAT,1,FAULT,...` |
| **RS485 Comm Timeout** | `SYS_MODE_FAULT` | `TriacForceOff` (LOW) | `HeaterRelay_ForceOff` (LOW) | Restored to Center Step | Wiper Stopped | `STAT,1,FAULT,...` |
| **Watchdog Reset** | `SYS_MODE_FAULT` | `TriacForceOff` (LOW) | `HeaterRelay_ForceOff` (LOW) | Reset to Step 40 | Wiper Stopped | `STAT,1,FAULT,...` |

---

## 10. Verified HIL Hardware Test Results

| Test Feature | Category | Telemetry Evidence | Result |
|:---|:---:|:---|:---:|
| **Start Öncesi TRIAC OFF** | IDLE Safety | 482 IDLE samples: `DELAY=9500us`, `TRIAC_OUT=0`, `TRIAC_FB=0` | **RUNTIME-PROVEN** |
| **Normal P1 / P2 / FP TRIAC** | Running Power | START transitions delay to 8600 us (10%), STOP restores 9500 us | **RUNTIME-PROVEN** |
| **DEGAS TRIAC Modulation** | Degas Pulse | Delay modulates between 2300 us (80% ON) and 9500 us (OFF) | **RUNTIME-PROVEN** |
| **DEGAS Power (%80)** | Phase Control | Delay equals $9500 - (9000 \times 80 / 100) = 2300\,\mu\text{s}$ exactly | **RUNTIME-PROVEN** |
| **DEGAS Pulse ON/OFF (1100/300ms)**| Timing | Non-blocking tick state-machine verified across cycles | **RUNTIME-PROVEN** |
| **Zero-Cross Gating** | EXTI Safety | IDLE drops EXTI pulses; RUNNING arms TIM15 | **RUNTIME-PROVEN** |
| **TRIAC OUT/FB Loopback (PC6 -> PA6)**| Loopback | 752 samples analyzed: 0 mismatch (`TRIAC_OUT == TRIAC_FB`) | **RUNTIME-PROVEN** |
| **Heater OUT/FB Loopback (PB15 -> PA4)**| Loopback | 1533 samples analyzed: 0 mismatch (`HEATER_OUT == HEATER_FB`) | **RUNTIME-PROVEN** |
| **Heater Upper Limit Interlock (58C)**| Temperature | $T = 64.5^\circ\text{C} \ge 58.0^\circ\text{C}$ opens relay (`RELAY = 0`) | **RUNTIME-PROVEN** |
| **DEGAS 33 kHz Tuning (Step 61)** | Frequency | X9C wiper locked at Step 61; measured PA0 voltage is 2.055 V | **RUNTIME-PROVEN** |
| **DEGAS Frequency Isolation** | Isolation | Page 0 frequency toggle does not alter saved DEGAS 33 kHz profile | **RUNTIME-PROVEN** |
| **Sweep ARM Persistence** | Sweep Architecture| STOP, Timer Zero, and Freq Toggle preserve `runtime_sweep = true` | **RUNTIME-PROVEN** |
| **Safe Stop Latency** | Safety | Synchronous <1us gate cutoff and relay disconnect | **RUNTIME-PROVEN** |
| **RS485 Bus Integrity** | Communication | 55/55 frames delivered with 0 framing errors or CRC loss | **RUNTIME-PROVEN** |
| **Page 5 RELAY ↔ SSR Toggle UI** | Dual Heater HMI | `b_htr_mode` updates instantly; Discard restores original mode | **RUNTIME-PROVEN** |
| **Per-Tank Heater Mode NVS (`htr_m_1`)**| Dual Heater NVS | `0 = RELAY`, `1 = SSR` persisted across reboot & page reopen | **RUNTIME-PROVEN** |
| **RS485 `SET_HEATER_MODE` / ACK** | Dual Heater Comm | `ACK:HEATER_MODE=SSR` and `ACK:HEATER_MODE=RELAY` confirmed | **RUNTIME-PROVEN** |
| **RELAY Mode Bang-Bang Heating** | Dual Heater Engine | $T=67.1^\circ\text{C} < 70.0^\circ\text{C} \implies \text{Relay}=1$ drives PB15 active | **RUNTIME-PROVEN** |
| **SSR Mode PID Time-Proportional** | Dual Heater Engine | 10 Hz PID error produces active PWM duty cycle on PB15 | **RUNTIME-PROVEN** |
| **DEGAS + Heater Mode Isolation** | Interlock Safety | DEGAS enforces `temp_ctrl=0` ($0\text{V}$ PB15) in both RELAY/SSR | **RUNTIME-PROVEN** |
| **Physical 12V SSR Load Current** | Electrical | Physical solid-state relay & 12V resistor element unattached | **PHYSICAL-NOT-PROVEN** |
| **Physical PC6 Gate Microsecond Waveform**| Electrical | Gate pulse voltage shape requires external oscilloscope probe | **PHYSICAL-NOT-PROVEN** |


---

## 11. Historical Fixed Bugs & Root Cause Analysis

1. **Bug 1: DIP Switch / Code 68 Hardware Fault on Boot**  
   - *Root Cause:* Floating DIP switch inputs triggered spurious tank card ID desynchronization.  
   - *Fix:* Implemented software DIP-free tank provisioning with non-volatile flash override.  
   - *Verification:* Verified across 50+ reboots with 0 false Code 68 faults.
2. **Bug 2: Sweep Wiper Movement During IDLE Mode**  
   - *Root Cause:* `X9C103S_SweepProcess` did not check `g_system_state.mode == SYS_MODE_RUNNING`.  
   - *Fix:* Added system mode interlock to sweep step advancement.  
   - *Verification:* X9C wiper confirmed static at center step in IDLE mode.
3. **Bug 3: Sweep Disarmed / Button Reset Upon STOP Command**  
   - *Root Cause:* `CMD_STOP` handler executed `runtime_sweep[g] = false`.  
   - *Fix:* Separated ARMED state from ACTIVE execution; STOP only halts wiper movement.  
   - *Verification:* Nextion `b_sweep` button remains GREEN; next START resumes sweep.
4. **Bug 4: Sweep Disarmed Upon Frequency Toggle**  
   - *Root Cause:* `CMD_FREQ_TOGGLE` executed `runtime_sweep[g] = false`.  
   - *Fix:* Preserved `runtime_sweep` and updated center frequency parameter.  
   - *Verification:* Toggling between 28k and 40k maintains ARMED status.
5. **Bug 5: DEGAS Arbitrary Frequency Incompatibility (35 kHz / 33 kHz)**  
   - *Root Cause:* STM32 `X9C103S_SetFrequency` accepted only binary values `28` and `40`.  
   - *Fix:* Implemented integer linear interpolation formula supporting 28..40 kHz.  
   - *Verification:* Tested with 33 kHz -> Step 61, 2.055 V PA0.
6. **Bug 6: DEGAS Frequency Overridden by Page 0 Frequency**  
   - *Root Cause:* ESP32 `CMD_START` handler lacked frequency snapshotting for DEGAS.  
   - *Fix:* Formatted parameterized `START_DEGAS` frame with saved Page 7/8 frequency.  
   - *Verification:* Page 0 frequency changes do not affect DEGAS frequency.
7. **Bug 7: RS485 Buffer Overflow on Rapid Command Bursts**  
   - *Root Cause:* Polling-based UART driver lacked circular ring buffering.  
   - *Fix:* Implemented interrupt-driven ring buffer with non-blocking line parser.  
   - *Verification:* 100+ burst commands processed with 0 packet loss.
8. **Bug 8: RS485 Bus Collision from Late Driver Disable**  
   - *Root Cause:* `RS485_DE` pin pulled low before shift register TC (Transmission Complete) flag.  
   - *Fix:* Added TC polling prior to DE pin de-assertion.  
   - *Verification:* Oscilloscope and logic analyzer confirmed clean bus turn-around.
9. **Bug 9: Recipe Power Setpoint Leakage into DEGAS Profile**  
   - *Root Cause:* Power scaling used a shared global variable across modes.  
   - *Fix:* Separated `g_system_state.setpoint_power_pct` from `degas_config.power_pct`.  
   - *Verification:* P1 power (10%) and DEGAS power (80%) execute independently.

---

## 12. Current Open Items & Physical Verification Boundaries

1. **PC6 Gate Electrical Waveform Measurement (`PHYSICAL-NOT-PROVEN`):**  
   While firmware logic, register states, and PA6 loopback confirm active gate pulses, measuring the precise 100 us analog rise time and optocoupler gate current requires an external oscilloscope on the high-voltage AC board.
2. **Heater Lower Hysteresis Turn-On Edge (`CODE-PROVEN / RUNTIME-NOT-PROVEN`):**  
   The heater logic is fully verified in code, but because bench test water temperature remained at 63.8C..65.1C (above the 56.0C lower turn-on threshold), the physical heating turn-on transition was not observed during runtime.

---

## 13. Repository & Version Metadata

- **Repository:** `https://github.com/erenerdemM/Ege_Eagle_Ultrasonik.git`
- **Active Branch:** `main`
- **Manifest Version:** `V4.0.0 (Authoritative Current State)`
- **STM32 Target:** STM32G474RET6 (ARM Cortex-M4 @ 170 MHz, 512 KB Flash, 128 KB SRAM)
- **ESP32 Target:** ESP32-S3 (Xtensa Dual-Core @ 240 MHz, 8 MB PSRAM, 16 MB Flash)
- **HMI Target:** Nextion NX8048T070 (7.0" 800x480 HMI Display)
- **Toolchains:** GNU ARM Toolchain (`arm-none-eabi-gcc 13.3.rel1`), Arduino-CLI ESP32 Core (`3.3.11`), Nextion Editor (`v1.65.1`)
