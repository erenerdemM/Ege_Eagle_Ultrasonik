# REMOTE NEXTION SIMULATOR TEST MATRIX
**Project:** EAGLEULTRASONİK  
**Document Revision:** 1.0.0  
**Target Architecture:** Windows Nextion Simulator $\longleftrightarrow$ Raspberry Pi 5 $\longleftrightarrow$ ESP32-S3 $\longleftrightarrow$ STM32G474RE  
**Status:** `VERIFICATION MATRIX SPECIFIED`

---

## 1. Master Remote HIL Test Matrix

| Test ID | Simulator Action | UART Command | ESP32 Expected State | RS485 Expected Frame | STM32 Expected State | HMI Response (Nextion Display) | Physical Observable |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **TS-01** | Press `b_goz` (Page 0) | `PAGE1_OPEN\n` | `temp_goz = 1`, opens quick select | None | Unaffected (`IDLE`) | `page page1`, `t0.txt="1"` | Simulator switches to Page 1 |
| **TS-02** | Press `b_up` then `b_ok` (Page 1) | `UP\n` followed by `SEL\n` | `secili_goz = 2`, reloads Tank 2 setpoints | `T2:SET_TIME:..`, `T2:SET_TEMP:..` | Tank 2 Target Synchronized | `page page0`, updates setpoints for Göz 2 | Page 0 header displays Tank 2 data |
| **TS-03** | Press `b_p1` (Page 0) | `P1_SEL\n` | `hedef_sure=p_sure[1]`, `hedef_sicaklik=p_sic[1]`, `runtime_sweep=p_sw[1]` | `T<g>:SET_TIME:15\n`, `T<g>:SET_TEMP:40\n`, `T<g>:SWEEP:OFF\n` | STM32 updates time & temp targets | `t_set_sure.txt="15"`, `t_set_sic.txt="40"`, `b_swe.bco=50712` | Simulator setpoint text fields update to 15m / 40°C |
| **TS-04** | Press `b_p2` (Page 0) | `P2_SEL\n` | `hedef_sure=p_sure[2]`, `hedef_sicaklik=p_sic[2]`, `runtime_sweep=p_sw[2]` | `T<g>:SET_TIME:20\n`, `T<g>:SET_TEMP:50\n`, `T<g>:SWEEP:OFF\n` | STM32 updates time & temp targets | `t_set_sure.txt="20"`, `t_set_sic.txt="50"`, `b_swe.bco=50712` | Simulator setpoint text fields update to 20m / 50°C |
| **TS-05** | Press `b_p3` (Page 0) | `P3_SEL\n` | `hedef_sure=p_sure[3]`, `hedef_sicaklik=p_sic[3]`, `runtime_sweep=p_sw[3]` | `T<g>:SET_TIME:25\n`, `T<g>:SET_TEMP:60\n`, `T<g>:SWEEP:OFF\n` | STM32 updates time & temp targets | `t_set_sure.txt="25"`, `t_set_sic.txt="60"`, `b_swe.bco=50712` | Simulator setpoint text fields update to 25m / 60°C |
| **TS-06** | Press `b_swe` (Page 0) | `b_swe\n` | `runtime_sweep[g] = true`, disarms DEGAS | `T<g>:SWEEP:ON\n` | STM32 frequency sweep activated in TIM15 modulation | `b_swe.bco=2016` (Green), `b_deg.bco=50712` | `b_swe` button illuminates green on simulator |
| **TS-07** | Press `b_deg` (Page 0) | `b_deg\n` | `degas_armed[g] = true`, disables Sweep | `T<g>:SWEEP:OFF\n` | STM32 disarms sweep modulation | `b_deg.bco=2016` (Green), `b_swe.bco=50712` | `b_deg` button illuminates green on simulator |
| **TS-08** | Press `b_start` (Page 0) | `CMD_START\|15\|40\n` | `makine_calisiyor[g] = true`, `kalan_saniye = 900` | `T<g>:START\n` | STM32 enters `SYS_RUNNING`, starts PWM soft-start | `t_durum.txt="YIKAMA DEVAM EDIYOR..."` | Oscilloscope shows TIM15 PWM output ramp (PB14/15) |
| **TS-09** | Stream STM32 Telemetry | (STM32 Timer 1000ms) | ESP32 stores `anlik_sicaklik`, `kalan_saniye` | `STAT,1,RUNNING,899,250,0,50,28,0\n` | STM32 executes PID & countdown | `t_kalan_sure.txt="14:59"`, `t_anlik_sic.txt="25.0"` | Simulator timer counts down every second in real-time |
| **TS-10** | Press `b_stop` (Page 0) | `CMD_STOP\n` | `makine_calisiyor=false`, `degas_active=false`, `degas_armed=false` | `T<g>:STOP\n` | STM32 enters `SYS_IDLE`, PWM disabled immediately | `t_durum.txt="SISTEM DURDURULDU"`, `b_deg.bco=50712` | PWM waveform on scope stops immediately |
| **TS-11** | Open Service Menu (Page 4 Keypad) | `KEY1` `KEY2` `KEY3` `KEY4` `KEY5` `KEY6` `KEY_OK` | Validates PIN "123456", `g_service_authenticated=true` | None | Unaffected (`IDLE`) | `page page5`, `t_goz_num.txt="Goz: 1"` | Simulator transitions to Page 5 |
| **TS-12** | Change Tank on Page 5 | `b_down\n` (`PAGE5_GOZ_DOWN`) | `secili_goz = 2`, loads Tank 2 service settings | None | Unaffected (`IDLE`) | `t_goz_num.txt="Goz: 2"`, `t_guc.txt=".."`, `t_id.txt=".."` | Page 5 parameters refresh for Tank 2 |
| **TS-13** | Forward Cyclic Navigation | `b_forwoard\n` (`NAV_FORWARD`) | `current_service_page = 6` | None | Unaffected (`IDLE`) | `page page6`, `t_goz_num.txt="Goz: 2"` | Simulator displays Page 6 (Sweep Service) |
| **TS-14** | Configure Sweep Settings (Page 6) | `SWP_SPAN_UP\n`, `PAGE6_SWP_TOGGLE\n`, `PAGE6_SAVE\n` | `service_sweep[2].span_khz=3`, `enabled=true`, NVS written | None | Unaffected (`IDLE`) | `t_swp_span.txt="3"`, `b_swe.bco=2016`, `b_save.bco=2016` (Flash) | Simulator shows span=3, sweep ON, save green flash |
| **TS-15** | Forward Cyclic Navigation | `b_forwoard\n` (`NAV_FORWARD`) | `current_service_page = 7` | None | Unaffected (`IDLE`) | `page page7`, `t_goz_num.txt="Goz: 2"` | Simulator displays Page 7 (DEGAS Service) |
| **TS-16** | Configure DEGAS Settings (Page 7) | `DEG_DUR_UP\n`, `PAGE7_SAVE\n` | `service_degas[2].duration_minutes=16`, NVS written | None | Unaffected (`IDLE`) | `t_deg_dur.txt="16"`, `b_save.bco=2016` (Flash) | Simulator shows duration=16, save green flash |
| **TS-17** | Exit Service Menu (Page 7) | `b_exit\n` (`SERVICE_EXIT`) | Exits service group, preserves NVS | None | Unaffected (`IDLE`) | `page page3` | Simulator returns to Page 3 |
| **TS-18** | DEGAS / Sweep Mutual Exclusion | With `b_deg` ON, press `b_swe` | ESP32 disarms DEGAS, activates Sweep | `T<g>:SWEEP:ON\n` | STM32 sets sweep active | `b_swe.bco=2016`, `b_deg.bco=50712` | Mutual exclusion verified on screen |
| **TS-19** | Active RUNNING Lockout | While running, press `P1_SEL` or `TIME_UP` | Commands rejected by ESP32 lockout | None | Continues executing process uninterrupted | Visual setpoints remain unchanged | Simulator ignores setpoint touch while running |
| **TS-20** | Communication Timeout Watchdog | Disconnect RS485 bus wire for $>3000\text{ms}$ | ESP32 marks node offline | None | Watchdog timeout | `t_durum.txt="Kart Yok!"` | Simulator displays offline error banner |

---

## 2. Execution Protocol

1. **Step 1:** Establish physical USB-UART connection to ESP32 GPIO16/17.
2. **Step 2:** Start daemon on Raspberry Pi: `python rpi_hmi_bridge.py /dev/ttyUSB0 9600`.
3. **Step 3:** Start TCP-to-Virtual-COM redirector on Windows PC: `com2tcp.exe \\.\CNCB0 100.99.150.99 8888`.
4. **Step 4:** Launch Nextion Editor Simulator in Debug Mode, select `CNCA0` @ 9600 Baud, click "Start".
5. **Step 5:** Execute test scenarios TS-01 through TS-20 sequentially while monitoring real-time STM32 VCP telemetry via `python live_monitor.py`.
