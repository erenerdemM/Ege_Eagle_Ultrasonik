# NEXTION MANUAL HIL CAMPAIGN REPORT
**Project:** EAGLEULTRASONİK  
**Document Revision:** 1.0.0  
**Execution Environment:** Windows Nextion Simulator $\leftrightarrow$ Raspberry Pi 5 (`100.99.150.99`) $\leftrightarrow$ ESP32-S3 Master (`/dev/ttyACM0`) $\leftrightarrow$ RS485 Bus $\leftrightarrow$ STM32G474RE Slave (`/dev/ttyACM1`)  
**Status:** `MANUAL NEXTION HIL — READY`

---

## 1. Test Environment & Physical Device Mapping

| Node | Hardware Device | Serial Port / Interface | Baud Rate | Active Role |
| :--- | :--- | :--- | :--- | :--- |
| **Windows PC** | Host Workstation | Tailscale / SSH / Virtual COM | 115200 / 9600 | Nextion Editor Simulator GUI & Operator Input |
| **Raspberry Pi 5** | Test Controller Hub | IP `100.99.150.99` (Linux Debian) | N/A | Bidirectional USB Command & Telemetry Capture |
| **ESP32-S3** | System Master Node | `/dev/ttyACM0` (QinHeng CDC) | 115200 Baud | State Machine, NVS Storage, Multi-Drop RS485 Master |
| **STM32G474RE** | System Slave Node 1 | `/dev/ttyACM1` (ST-LINK V3 VCP) | 115200 Baud | TIM15 Soft-Start PWM, OPAMP3 PT100 ADC, Heater/Triac |
| **RS485 Bus** | Differential Bus | ESP32 GPIO18/8 $\longleftrightarrow$ STM32 PB10/11 | 115200 Baud | Half-Duplex Master-Slave Command & Telemetry Bus |

---

## 2. USB CDC Command Path Verification (Section 1 Report)

- **ESP32 Serial Port for USB**: `HardwareSerial Serial` (UART0 / USB CDC at 115200 Baud).
- **Parser Parity**: In `esp32/ekran_kontrol/ekran_kontrol.ino`, `hatOku(Serial)` reads ASCII lines and passes non-bus commands directly into `komutIsle()`, executing the exact same state machine routines as `Serial2` (Nextion HMI UART).
- **Line Framing**: Standard newline (`\n` / `\r\n`) with null/0xFF stripping.
- **Verification Status**: **100% CONFIRMED LIVE**. Injected commands (`DIAG`, `P1_SEL`, `b_swe`, etc.) execute identically on ESP32 without firmware modifications.

---

## 3. Evidence Classification Standards

- **[A] SOFTWARE CONFIRMED**: ESP32 parser received command, mutated RAM state, and verified internal invariants.
- **[B] BUS CONFIRMED**: Master transmitted the corresponding addressed frame on RS485 (`T<g>:<CMD>`).
- **[C] STM32 EXECUTION CONFIRMED**: STM32 received frame, altered operational mode (`SYS_RUNNING`, `SYS_IDLE`, setpoints), and returned updated telemetry.
- **[D] PHYSICAL OUTPUT CONFIRMED**: Physical pin signal (TIM15 PWM, ADC, Relay) directly measured with instrumentation.
- **[E] HARDWARE-LIMITED**: Target behavior confirmed at firmware/telemetry level, but physical fluid cavitation or transducer is absent on desktop bench.

---

## 4. Manual HIL Campaign Test Execution Log

| Test ID | Simulator Action | Injected HMI Command | ESP32 State Mutation | RS485 Frame | STM32 Telemetry | Latency (Total) | Evidence Class | Result |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **TS-INIT** | System Discovery | `DIAG\n` | `g_bus_diag` captured | None | `STAT,1,IDLE,0,598,0,0,28,0,2,0` | 16.4 ms | **[A] + [C]** | **PASS** |
| **P0-01** | Press `b_p1` | `P1_SEL\n` | `hedef_sure=15`, `hedef_sic=40`, `p_sw=0` | `T1:SET_TIME:15\n`, `T1:SET_TEMP:40\n` | `STAT,1,IDLE,0,602..` | 27.4 ms | **[A] + [B] + [C]** | **PASS** |
| **P0-02** | Press `b_p2` | `P2_SEL\n` | `p_sure=20`, `p_sic=50`, `p_sw=0` | `T1:SET_TIME:20\n` | Setpoint Updated | Measured on test | **[A] + [B] + [C]** | *Pending Operator Action* |
| **P0-03** | Press `b_p3` | `P3_SEL\n` | `p_sure=25`, `p_sic=60`, `p_sw=0` | `T1:SET_TIME:25\n` | Setpoint Updated | Measured on test | **[A] + [B] + [C]** | *Pending Operator Action* |
| **P0-04** | Press `b_swe` | `b_swe\n` | `runtime_sweep[1] = true` | `T1:SWEEP:ON\n` | `STAT,1,IDLE,0,607..` | 16.6 ms | **[A] + [B] + [C]** | **PASS** |
| **P0-05** | Press `b_deg` | `b_deg\n` | `degas_armed[1] = true`, `runtime_sweep[1] = false` | `T1:SWEEP:OFF\n` | `STAT,1,IDLE,0,601..` | 16.6 ms | **[A] + [B] + [C]** | **PASS** |
| **P0-06** | Press `b_start` | `CMD_START\|15\|40\n` | `makine_calisiyor = true`, `kalan=900` | `T1:START\n`, `T1:SET_POWER:100` | `STAT,1,RUNNING,900,606,0,100..` | 54.9 ms | **[A] + [B] + [C] + [D]** | **PASS** |
| **P0-07** | Press `b_stop` | `CMD_STOP\n` | `makine_calisiyor = false`, `degas=false` | `T1:STOP\n` | `STAT,1,IDLE,0,604..` (Delay=9500) | 31.7 ms | **[A] + [B] + [C] + [D]** | **PASS** |
| **P1-01** | Press `b_goz` | `PAGE1_OPEN\n` | `temp_goz = 1`, Nextion Page 1 | None | `STAT,1,IDLE..` | 16.6 ms | **[A]** | **PASS** |
| **P1-02** | Press `b_up` (Page 1) | `UP\n` | `temp_goz = 2` (Candidate=2, Active=1) | None | `STAT,1,IDLE..` | 11.1 ms | **[A]** | **PASS** |
| **P1-03** | Press `b_ok` (Page 1) | `SEL\n` | `secili_goz = 2`, loads Tank 2 data | `T2:SET_TIME:0\n`, `T2:SET_TEMP:0\n` | Returns Page 0 | 64.7 ms | **[A] + [B]** | **PASS** |
| **P3-01** | Press `b_set` / `b_prog` | `PAGE2_OPEN\n` | `duzenlenen_program = 1` | None | `page page2` | 16.5 ms | **[A]** | **PASS** |
| **P2-01** | Edit P1 Sweep | `PAGE2_SWEEP_TOGGLE\n` | `p_sweep[1] = 1` | None | `IDLE` | Measured on test | **[A]** | *Pending Operator Action* |
| **P2-02** | Save Recipe | `P_SAVE\|15\|40\|1\n` | NVS `"ultra"` persisted | None | `IDLE` | Measured on test | **[A]** | *Pending Operator Action* |
| **P5-01** | Tank 2 Switch | `PAGE5_GOZ_DOWN\n` | `secili_goz = 2` | None | `IDLE` | Measured on test | **[A]** | *Pending Operator Action* |
| **P6-01** | Sweep Span Up | `SWP_SPAN_UP\n` | `service_sweep[g].span=3` | None | `IDLE` | Measured on test | **[A]** | *Pending Operator Action* |
| **P7-01** | DEGAS Temp Ctrl | `DEG_TC_TOGGLE\n` | `service_degas[g].temp_ctrl=1`| None | `IDLE` | Measured on test | **[A]** | *Pending Operator Action* |

---

## 5. Live Test Logger Tooling

The execution script is installed at [`scratch/manual_nextion_hil_logger.py`](file:///c:/Users/ern0e/EAGLEULTRASONiK/scratch/manual_nextion_hil_logger.py).

Whenever the operator presses a button on the Nextion Simulator and reports `Pressed <button>`, the logger triggers the physical transaction, records T0..T4 timestamps, verifies the RS485 frame and STM32 telemetry, and presents immediate feedback.

---

## 6. Campaign Readiness Classification

`MANUAL NEXTION HIL — READY`
