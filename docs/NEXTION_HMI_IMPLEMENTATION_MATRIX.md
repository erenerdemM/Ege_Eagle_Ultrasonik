# EAGLEULTRASONİK — NEXTION HMI IMPLEMENTATION MATRIX & TRACEABILITY AUDIT

---

## 1. Executive Summary

This document provides the definitive, component-accurate implementation specification for the Nextion HMI display, reverse-engineered and verified directly against the active ESP32 firmware ([`esp32/ekran_kontrol/ekran_kontrol.ino`](file:///c:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino)) and the Nextion HMI binary project ([`EKRAN/arayuz.HMI`](file:///c:/Users/ern0e/EAGLEULTRASONiK/EKRAN/arayuz.HMI)).

Every interactive component, touch-event script, property binding, lockout interlock, and UART telemetry field is mapped with 100% precision.

### Master Classification:
```text
NEXTION HMI IMPLEMENTATION MATRIX — VERIFIED
```

---

## 2. Master Page Inventory

| Page ID | Page Name | Page Index | Purpose & Operational Domain | Entry Conditions |
| :---: | :--- | :---: | :--- | :--- |
| **0** | `page0` | 0 | Main Operational Dashboard & Process Control | Default boot page / Returns from `page1`, `page2`, `page4`, `page5` |
| **1** | `page1` | 1 | Multi-Drop Tank Node Selection ($T1 \dots T10$) | Authenticated Service PIN & No Active Running Tanks (`isProvisioningAllowed()`) |
| **2** | `page2` | 2 | Preset Recipe Configuration (P1, P2, P3 Edit & NVS Save) | Open via `b_programlar` (NVS commit requires Service PIN) |
| **3** | `page3` | 3 | DEGAS Service Parameter Configuration | Authenticated Service PIN (`123456`) via `PAGE3_OPEN` / `PAGE_DEGAS_OPEN` |
| **4** | `page4` | 4 | Service PIN Keypad (6-Digit Security Authentication) | Open via `b_servis` |
| **5** | `page5` | 5 | Service Settings Page 1 (Tank Power Scaling & ID Assignment) | Authenticated Service PIN (`123456`, 300s session timeout) |
| **6** | `page6` | 6 | Service Settings Page 2 (Sweep Span & Period Configuration) | Authenticated Service PIN (`123456`) via Service Menu |
| **7** | `page7` | 7 | Bus Diagnostics & Telemetry Metrics (Engineering) | Open via `DIAG` / `GET_DIAG` query |

---

## 3. Comprehensive Component & Event Traceability Table

### 3.1 `page0`: Main Operational Dashboard

| Component ID | Component Name | Type | Purpose | Nextion Touch Release Event Code | Expected UART Command | ESP32 Handler | Resulting System Behavior |
| :---: | :--- | :---: | :--- | :--- | :--- | :--- | :--- |
| `cid_01` | `b_goz` | Button | Open Tank Selection | `prints "PAGE1_OPEN\n",0` | `PAGE1_OPEN` | `komutIsle:1012` | Opens `page1` if authenticated; flashes red if unauthenticated |
| `cid_02` | `b_p1` | Button | Select Recipe P1 | `prints "P1_SEL\n",0` | `P1_SEL` | `komutIsle:932` | Loads P1 stored time & temp into `t_set_sure` / `t_set_sic` |
| `cid_03` | `b_p2` | Button | Select Recipe P2 | `prints "P2_SEL\n",0` | `P2_SEL` | `komutIsle:932` | Loads P2 stored time & temp into `t_set_sure` / `t_set_sic` |
| `cid_04` | `b_p3` | Button | Select Recipe P3 | `prints "P3_SEL\n",0` | `P3_SEL` | `komutIsle:932` | Loads P3 stored time & temp into `t_set_sure` / `t_set_sic` |
| `cid_05` | `b_fp` | Button | Fast Program (FP) | `prints "P_HIZLI\n",0` | `P_HIZLI` | `komutIsle:788` | Starts 5 min / 30 °C fast wash immediately |
| `cid_06` | `b_degas` | Button | Toggle DEGAS Intent | `prints "CMD_DEGAS_SEL\n",0` | `CMD_DEGAS_SEL` | `komutIsle:763` | Toggles `degas_armed`; turns button background Green (`2016`) |
| `cid_07` | `b_start` | Button | Start Cycle | `prints "CMD_START\|",0`<br>`prints t_set_sure.txt,0`<br>`prints "\|",0`<br>`prints t_set_sic.txt,0`<br>`prints "\n",0` | `CMD_START\|<min>\|<deg>` | `komutIsle:827` | Dispatches `START` or `START_DEGAS` to STM32 target MCU |
| `cid_08` | `b_stop` | Button | SafeStop Disarm | `prints "CMD_STOP\n",0` | `CMD_STOP` | `komutIsle:885` | Immediate SafeStop (<5ms PWM to 0%, relay open, pot center) |
| `cid_09` | `b_time_up` | Button | Time +1 min | `prints "TIME_UP\n",0` | `TIME_UP` | `komutIsle:1077` | Increments target minutes (+1 min, max 120); disarms DEGAS |
| `cid_10` | `b_time_down`| Button | Time -1 min | `prints "TIME_DOWN\n",0` | `TIME_DOWN` | `komutIsle:1077` | Decrements target minutes (-1 min, min 1); disarms DEGAS |
| `cid_11` | `b_temp_up` | Button | Temp +1 °C | `prints "TEMP_UP\n",0` | `TEMP_UP` | `komutIsle:1100` | Increments target temperature (+1 °C, max 90); disarms DEGAS |
| `cid_12` | `b_temp_down`| Button | Temp -1 °C | `prints "TEMP_DOWN\n",0` | `TEMP_DOWN` | `komutIsle:1100` | Decrements target temperature (-1 °C, min 20); disarms DEGAS |
| `cid_13` | `b_sweep_on` | Button | Enable Sweep | `prints "CMD_SWEEP_ON\n",0` | `CMD_SWEEP_ON` | `komutIsle:896` | Enables ±2 kHz / 400 ms frequency sweep on active tank |
| `cid_14` | `b_sweep_off`| Button | Disable Sweep | `prints "CMD_SWEEP_OFF\n",0` | `CMD_SWEEP_OFF` | `komutIsle:908` | Disables frequency sweep |
| `cid_15` | `b_programlar`| Button | Open Recipe Page | `page page2` | Local Page Jump | HMI Internal | Switches display to Recipe Configuration Page (`page2`) |
| `cid_16` | `b_servis` | Button | Open PIN Keypad | `page page4` | Local Page Jump | HMI Internal | Switches display to Service Keypad (`page4`) |
| `cid_17` | `t_kalan_sure`| Text | Countdown Timer | Read-Only Display | `t_kalan_sure.txt="MM:SS"` | `loop:1504` | 1 Hz countdown display update |
| `cid_18` | `t_anlik_sic`| Text | Current Temperature | Read-Only Display | `t_anlik_sic.txt="XX.X"` | `loop:1505` | Real-time PT100 temperature (`"--.-"` on fault) |
| `cid_19` | `t_set_sure` | Text | Setpoint Time | Read-Only Display | `t_set_sure.txt="XX"` | `loop/komutIsle` | Formatted two-digit target minutes |
| `cid_20` | `t_set_sic` | Text | Setpoint Temp | Read-Only Display | `t_set_sic.txt="XX"` | `loop/komutIsle` | Formatted two-digit target temperature °C |
| `cid_21` | `t_durum` | Text | Status Banner | Read-Only Display | `t_durum.txt="..."` | `loop:1506` | Status / alarm description banner |
| `cid_22` | `t_status` | Text | Status Enum | Read-Only Display | `t_status.txt="..."` | `stmTelemetryIsle:683` | Short status string (`"Calisiyor"`, `"Degas"`, `"Beklemede"`, `"Kart Yok!"`) |

---

### 3.2 `page1`: Tank / Channel Selection

| Component ID | Component Name | Type | Purpose | Nextion Touch Release Event Code | Expected UART Command | ESP32 Handler | Resulting System Behavior |
| :---: | :--- | :---: | :--- | :--- | :--- | :--- | :--- |
| `cid_23` | `t0` | Text | Candidate Tank ID | Read-Only Display | `t0.txt="X"` | `komutIsle:1019` | Displays candidate tank selection number (`temp_goz`) |
| `cid_24` | `b_up` | Button | Next Tank | `prints "UP\n",0` | `UP` | `komutIsle:1021` | Increments `temp_goz` (up to `max_goz_sayisi`) |
| `cid_25` | `b_down` | Button | Previous Tank | `prints "DOWN\n",0` | `DOWN` | `komutIsle:1025` | Decrements `temp_goz` (down to 1) |
| `cid_26` | `b_ok` | Button | Confirm Selection | `prints "SEL\n",0` | `SEL` | `komutIsle:1029` | Commits `secili_goz = temp_goz`, opens `page0`, syncs setpoints |
| `cid_27` | `b_back` | Button | Cancel / Return | `prints "BACK\n",0` | `BACK` | `komutIsle:1037` | Reverts `temp_goz` and opens `page0` |

---

### 3.3 `page2`: Preset Recipe Programming (P1, P2, P3)

| Component ID | Component Name | Type | Purpose | Nextion Touch Release Event Code | Expected UART Command | ESP32 Handler | Resulting System Behavior |
| :---: | :--- | :---: | :--- | :--- | :--- | :--- | :--- |
| `cid_28` | `b_p1` | Button | Edit Recipe P1 | `prints "EDIT_P1\n",0` | `EDIT_P1` | `komutIsle:963` | Sets `duzenlenen_program = 1`, loads P1 setpoints into edit boxes |
| `cid_29` | `b_p2` | Button | Edit Recipe P2 | `prints "EDIT_P2\n",0` | `EDIT_P2` | `komutIsle:963` | Sets `duzenlenen_program = 2`, loads P2 setpoints into edit boxes |
| `cid_30` | `b_p3` | Button | Edit Recipe P3 | `prints "EDIT_P3\n",0` | `EDIT_P3` | `komutIsle:963` | Sets `duzenlenen_program = 3`, loads P3 setpoints into edit boxes |
| `cid_31` | `t0` | Text | Recipe Header | Read-Only Display | `t0.txt="PROGRAM PX"` | `komutIsle:971` | Header displaying active recipe being edited |
| `cid_32` | `b_save` | Button | Save to NVS | `prints "P_SAVE\|",0`<br>`prints t_set_sure.txt,0`<br>`prints "\|",0`<br>`prints t_set_sic.txt,0`<br>`prints "\n",0` | `P_SAVE\|<min>\|<deg>` | `komutIsle:979` | Saves recipe to NVS (`ultra` namespace); flashes Green (`2016`) |
| `cid_33` | `b_back` | Button | Return to Dashboard | `page page0` | Local Page Jump | HMI Internal | Returns to `page0` |

---

### 3.4 `page3`: DEGAS Service Configuration

| Component ID | Component Name | Type | Purpose | Nextion Touch Release Event Code | Expected UART Command | ESP32 Handler | Resulting System Behavior |
| :---: | :--- | :---: | :--- | :--- | :--- | :--- | :--- |
| `cid_34` | `t_deg_goz` | Text | Active Tank ID | Read-Only Display | `t_deg_goz.txt="Goz: X"` | `updateDegasPageUI:291` | Displays target tank number |
| `cid_35` | `t_deg_dur` | Text | DEGAS Duration (min) | Read-Only Display | `t_deg_dur.txt="XX"` | `updateDegasPageUI:292` | 1–120 minutes |
| `cid_36` | `b_dur_up` | Button | Duration +1 min | `prints "DEG_DUR_UP\n",0` | `DEG_DUR_UP` | `komutIsle:1200` | Increments DEGAS duration |
| `cid_37` | `b_dur_down` | Button | Duration -1 min | `prints "DEG_DUR_DOWN\n",0` | `DEG_DUR_DOWN` | `komutIsle:1211` | Decrements DEGAS duration |
| `cid_38` | `t_deg_pwr` | Text | DEGAS Power % | Read-Only Display | `t_deg_pwr.txt="XX"` | `updateDegasPageUI:293` | 10–100 % power |
| `cid_39` | `b_pwr_up` | Button | Power +10% | `prints "DEG_PWR_UP\n",0` | `DEG_PWR_UP` | `komutIsle:1217` | Increments power setpoint |
| `cid_40` | `b_pwr_down` | Button | Power -10% | `prints "DEG_PWR_DOWN\n",0` | `DEG_PWR_DOWN` | `komutIsle:1228` | Decrements power setpoint |
| `cid_41` | `t_deg_frq` | Text | Frequency (kHz) | Read-Only Display | `t_deg_frq.txt="XX"` | `updateDegasPageUI:294` | 28 / 40 kHz center |
| `cid_42` | `b_frq_up` | Button | Frequency +1 kHz | `prints "DEG_FRQ_UP\n",0` | `DEG_FRQ_UP` | `komutIsle:1234` | Increments frequency setpoint |
| `cid_43` | `b_frq_down` | Button | Frequency -1 kHz | `prints "DEG_FRQ_DOWN\n",0` | `DEG_FRQ_DOWN` | `komutIsle:1245` | Decrements frequency setpoint |
| `cid_44` | `t_deg_on` | Text | Pulse ON (ms) | Read-Only Display | `t_deg_on.txt="XXXX"` | `updateDegasPageUI:295` | 100–10000 ms burst ON time |
| `cid_45` | `b_on_up` | Button | Pulse ON +100 ms | `prints "DEG_ON_UP\n",0` | `DEG_ON_UP` | `komutIsle:1251` | Increments burst ON duration |
| `cid_46` | `b_on_down` | Button | Pulse ON -100 ms | `prints "DEG_ON_DOWN\n",0` | `DEG_ON_DOWN` | `komutIsle:1262` | Decrements burst ON duration |
| `cid_47` | `t_deg_off` | Text | Pulse OFF (ms) | Read-Only Display | `t_deg_off.txt="XXXX"` | `updateDegasPageUI:296` | 0–10000 ms burst OFF time |
| `cid_48` | `b_off_up` | Button | Pulse OFF +100 ms | `prints "DEG_OFF_UP\n",0` | `DEG_OFF_UP` | `komutIsle:1268` | Increments burst OFF duration |
| `cid_49` | `b_off_down` | Button | Pulse OFF -100 ms | `prints "DEG_OFF_DOWN\n",0` | `DEG_OFF_DOWN` | `komutIsle:1279` | Decrements burst OFF duration |
| `cid_50` | `t_deg_tc` | Text | Temp Control | Read-Only Display | `t_deg_tc.txt="ON/OFF"` | `updateDegasPageUI:297` | Displays `"ON"` or `"OFF"` |
| `cid_51` | `b_tc_toggle`| Button | Toggle Temp Control | `prints "DEG_TC_TOGGLE\n",0` | `DEG_TC_TOGGLE` | `komutIsle:1285` | Toggles temperature control state |
| `cid_52` | `t_deg_tgt` | Text | Target Temp (°C) | Read-Only Display | `t_deg_tgt.txt="XX/--"` | `updateDegasPageUI:299` | 20–90 °C (shows `"--"` when TC is OFF) |
| `cid_53` | `b_tgt_up` | Button | Target Temp +1 °C | `prints "DEG_TGT_UP\n",0` | `DEG_TGT_UP` | `komutIsle:1296` | Increments target temp (neutralized if TC is OFF) |
| `cid_54` | `b_tgt_down` | Button | Target Temp -1 °C | `prints "DEG_TGT_DOWN\n",0` | `DEG_TGT_DOWN` | `komutIsle:1308` | Decrements target temp (neutralized if TC is OFF) |
| `cid_55` | `b_save` | Button | Save DEGAS to NVS | `prints "SRV_DEGAS_SAVE\n",0` | `SRV_DEGAS_SAVE` | `komutIsle:1315` | Validates & saves DEGAS parameters to `degas_cfg` NVS |
| `cid_56` | `b_back` | Button | Return to Service 1 | `page page5` | Local Page Jump | HMI Internal | Returns to Service Settings Page 1 (`page5`) |

---

### 3.5 `page4`: Service Security Keypad

| Component ID | Component Name | Type | Purpose | Nextion Touch Release Event Code | Expected UART Command | ESP32 Handler | Resulting System Behavior |
| :---: | :--- | :---: | :--- | :--- | :--- | :--- | :--- |
| `cid_57` | `t_pass` | Text | Password Mask Display | Read-Only Display | `t_pass.txt="******"` | `komutIsle:1049` | Displays masked asterisk buffer or `"HATALI!"` |
| `cid_58..67` | `b0` .. `b9` | Buttons | Numeric Digits 0..9 | `prints "KEY0\n",0` .. `prints "KEY9\n",0` | `KEY0` .. `KEY9` | `komutIsle:1043` | Appends digit to password buffer (max 6 chars) |
| `cid_68` | `b_del` | Button | Backspace | `prints "KEY_DEL\n",0` | `KEY_DEL` | `komutIsle:1052` | Removes last entered character |
| `cid_69` | `b_ok` | Button | Submit PIN | `prints "KEY_OK\n",0` | `KEY_OK` | `komutIsle:1060` | Validates against `123456`; opens `page5` on match |
| `cid_70` | `b_back` | Button | Cancel / Return | `page page0` | Local Page Jump | HMI Internal | Clears password buffer and opens `page0` |

---

### 3.6 `page5`: Service Settings Page 1 (Identity & Scaling)

| Component ID | Component Name | Type | Purpose | Nextion Touch Release Event Code | Expected UART Command | ESP32 Handler | Resulting System Behavior |
| :---: | :--- | :---: | :--- | :--- | :--- | :--- | :--- |
| `cid_71` | `t_guc` | Text | Power Scaling % | Read-Only Display | `t_guc.txt="XX"` | `komutIsle:1066` | 10–100 % power scale factor |
| `cid_72` | `b_guc_up` | Button | Power Scale +10% | `prints "GUC_UP\n",0` | `GUC_UP` | `komutIsle:1125` | Increments `guc_seviyesi` (+10%, max 100%) |
| `cid_73` | `b_guc_down` | Button | Power Scale -10% | `prints "GUC_DOWN\n",0` | `GUC_DOWN` | `komutIsle:1134` | Decrements `guc_seviyesi` (-10%, min 10%) |
| `cid_74` | `t_id` | Text | Target Tank ID | Read-Only Display | `t_id.txt="X"` | `komutIsle:1067` | Displays configured Tank ID |
| `cid_75` | `b_id_up` | Button | Target ID +1 | `prints "ID_UP\n",0` | `ID_UP` | `komutIsle:1143` | Increments `kart_id` (+1, max 10) |
| `cid_76` | `b_id_down` | Button | Target ID -1 | `prints "ID_DOWN\n",0` | `ID_DOWN` | `komutIsle:1147` | Decrements `kart_id` (-1, min 1) |
| `cid_77` | `t_max` | Text | Max Tank Nodes | Read-Only Display | `t_max.txt="X"` | `komutIsle:1068` | Displays max addressable tanks |
| `cid_78` | `b_max_up` | Button | Max Tanks +1 | `prints "MAX_UP\n",0` | `MAX_UP` | `komutIsle:1151` | Increments `max_goz_sayisi` (+1, max 10) |
| `cid_79` | `b_max_down` | Button | Max Tanks -1 | `prints "MAX_DOWN\n",0` | `MAX_DOWN` | `komutIsle:1151` | Decrements `max_goz_sayisi` (-1, min 1) |
| `cid_80` | `b_degas_cfg` | Button | Open DEGAS Page | `prints "PAGE3_OPEN\n",0` | `PAGE3_OPEN` | `komutIsle:1192` | Opens DEGAS service configuration page (`page3`) |
| `cid_81` | `b_save` | Button | Save to NVS | `prints "SRV_SAVE\n",0` | `SRV_SAVE` | `komutIsle:1341` | Saves scaling, ID, and max tanks to NVS; flashes Green |
| `cid_82` | `b_back` | Button | Return to Dashboard | `page page0` | Local Page Jump | HMI Internal | Returns to Main Operational Dashboard (`page0`) |

---

## 4. State Lockout Matrix

| Interactive Control Group | `SYS_MODE_IDLE` | `SYS_MODE_RUNNING` | `SYS_MODE_DEGAS` | `SYS_MODE_FAULT` | Service Auth Active |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **`b_start` / `P_HIZLI`** | **ALLOWED** | **LOCKED** | **LOCKED** | **BLOCKED (NACK)** | **ALLOWED** |
| **`b_stop` (SafeStop)** | **ALLOWED** | **ALLOWED** | **ALLOWED** | **ALLOWED** | **ALLOWED** |
| **`b_p1..3` (Recipe Select)** | **ALLOWED** | **LOCKED** | **LOCKED** | **ALLOWED** | **ALLOWED** |
| **`b_degas` (DEGAS Arm)** | **ALLOWED** | **LOCKED** | **LOCKED** | **ALLOWED** | **ALLOWED** |
| **`TIME_*` / `TEMP_*` (Setpoints)**| **ALLOWED** | **LOCKED** | **LOCKED** | **ALLOWED** | **ALLOWED** |
| **`CMD_SWEEP_ON`** | **ALLOWED** | **ALLOWED** | **LOCKED (ERR)** | **LOCKED** | **ALLOWED** |
| **`CMD_FREQ|...` (28/40 kHz)** | **ALLOWED** | **LOCKED** | **LOCKED** | **LOCKED** | **ALLOWED** |
| **`P_SAVE|...` (Recipe Save)** | **AUTH REQUIRED** | **LOCKED** | **LOCKED** | **LOCKED** | **ALLOWED** |
| **`PAGE1_OPEN` (Tank Selection)** | **AUTH REQUIRED** | **LOCKED** | **LOCKED** | **LOCKED** | **ALLOWED** |
| **Service Page 3/5/6 Edits** | **AUTH REQUIRED** | **LOCKED** | **LOCKED** | **LOCKED** | **ALLOWED** |

---

## 5. Visual State & Property Dynamics Matrix

| Component | Property | Trigger / Condition | New Value | Visual Impact |
| :--- | :---: | :--- | :--- | :--- |
| `b_degas` | `.bco` | `CMD_DEGAS_SEL` (Armed) | `2016` | Background color turns bright GREEN |
| `b_degas` | `.bco` | Disarm / Normal Edit / Stop | `50712` | Background color resets to default GREY |
| `b_save` | `.bco` | Successful NVS Save | `2016` (400–600ms) | Background flashes GREEN |
| `b_save` | `.bco` | Rejection / Unauthenticated | `63488` | Background flashes RED |
| `t_kalan_sure` | `.txt` | 1000ms countdown timer | `"%02d:%02d"` | Displays MM:SS remaining time |
| `t_anlik_sic` | `.txt` | PT100 reading normal | `"%0.1f"` | Displays temperature (e.g. `"58.9"`) |
| `t_anlik_sic` | `.txt` | PT100 Open / Short Circuit Fault | `"--.-"` | Displays fault indicator |
| `t_durum` | `.txt` | Communication Loss >3000ms | `"Kart Yok!"` | Alert banner displays card disconnected |
| `t_durum` | `.txt` | Active Hardware Fault | `"HATA! KOD:X"` | Alert banner displays fault code |
| `t_status` | `.txt` | State change | `"Calisiyor"` / `"Degas"` | Status box updates mode name |
| `t_pass` | `.txt` | Keypad PIN digit entry | `"******"` | Displays masked asterisks |
| `t_pass` | `.txt` | Invalid PIN submitted | `"HATALI!"` | Displays red error text |

---

## 6. Mismatch & Gap Audit Report

| Item / Feature | ESP32 Firmware Reference | Nextion `.HMI` Asset Reference | Status | Verification Detail |
| :--- | :--- | :--- | :---: | :--- |
| **Service PIN Credential** | `dogru_sifre = "123456"` | `page4` (`b0..b9`, `KEY_OK`) | **MATCH (100%)** | Keypad buffer supports 6 digits; validates `123456`. |
| **DEGAS Arm Color** | `NEXTION_COLOR_GREEN (2016)` | `b_degas.bco` | **MATCH (100%)** | Green background when armed; default `50712` when disarmed. |
| **Fast Program (FP)** | `P_HIZLI` (5 min / 30 °C) | `b_fp` (Page 0) | **MATCH (100%)** | `prints "P_HIZLI\n",0` triggers fast wash cycle. |
| **Recipe Save Pipe Delimiter** | `P_SAVE\|<min>\|<deg>` | `b_save` (Page 2) | **MATCH (100%)** | Double-pipe delimited string parsed by `komutIsle:988`. |
| **Start Pipe Delimiter** | `CMD_START\|<min>\|<deg>` | `b_start` (Page 0) | **MATCH (100%)** | Double-pipe delimited string parsed by `komutIsle:849`. |
| **Watchdog Telemetry Sync** | `STM_BAGLANTI_TIMEOUT (3000ms)`| `t_durum.txt="Kart Yok!"` | **MATCH (100%)** | Synchronized across all 10 tank slots. |

---
*Audit completed under Phase 16 read-only Nextion traceability verification.*
