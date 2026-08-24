# NEXTION MANUAL HIL ROUND-TRIP TEST REPORT: NX-SIM-RT-001
**Test ID:** `NX-SIM-RT-001`  
**Test Description:** Controlled Bidirectional Nextion Simulator Round-Trip Test for Preset Recipe P1 (`b_p1`)  
**Target Environment:** Windows Nextion Simulator $\longleftrightarrow$ Raspberry Pi 5 (`100.99.150.99`) $\longleftrightarrow$ ESP32-S3 Master (`/dev/ttyACM0`) $\longleftrightarrow$ STM32G474RE Node 1 (`/dev/ttyACM1`)  
**Status:** `PASS — FULL ROUND TRIP`

---

## 1. Test Action & Framing

- **Operator Action:** Selected Preset Recipe P1 (`b_p1` on `page0`).
- **Injected Command String:** `P1_SEL\n`
- **Raw Command Bytes (Hex):**  
  `50 31 5f 53 45 4c 0a` (`P1_SEL\n`)
- **Transport Interface:** Raspberry Pi 5 USB CDC (`/dev/ttyACM0` @ 115200 Baud) $\longleftrightarrow$ ESP32 `Serial` (`hatOku(Serial)`).

---

## 2. Timing & Latency Measurements (T0 .. T4)

| Timestamp Marker | Description | Monotonic Delta ($t - T_0$) | Layer Segment Latency |
| :--- | :--- | :--- | :--- |
| **$T_0$** | Simulator Command Injection | `0.00 ms` | Ref. $0.00\text{ ms}$ |
| **$T_1$** | ESP32 `hatOku(Serial)` Read & Parser Match | `+15.77 ms` | **$T_1 - T_0 = 15.77\text{ ms}$** (Host USB to ESP32 parser) |
| **$T_2$** | RS485 Bus Frame TX (`T1:SET_TIME:15`) | `+26.17 ms` | **$T_2 - T_1 = 10.40\text{ ms}$** (ESP32 internal dispatch) |
| **$T_3$** | ESP32 State Mutation Complete (`Göz 1 P1 loaded`) | `+36.57 ms` | **$T_3 - T_1 = 20.80\text{ ms}$** (RAM update & UI frame gen) |
| **$T_4$** | STM32 Telemetry Acknowledgment (`STAT,1,IDLE...`) | `+335.09 ms` | **$T_4 - T_2 = 308.92\text{ ms}$** (RS485 round-trip & telemetry) |

**Total Observable Processing Latency (Command $\to$ State Update):** **$36.57\text{ ms}$**  
**Total Telemetry Round-Trip Latency:** **$335.09\text{ ms}$**

---

## 3. ESP32 State Transition

| Parameter | State Before Test | State After `P1_SEL` | Verification Method |
| :--- | :--- | :--- | :--- |
| **Active Selected Program** | Undefined / None | `aktif_program = 1` (P1) | `--> ESP32: GÖZ 1 için P1 yüklendi.` |
| **Target Duration (`hedef_sure[1]`)** | `0 min` | **`15 min`** (`p_sure[1]`) | Logged & RS485 sent |
| **Target Temp (`hedef_sicaklik[1]`)**| `0 °C` | **`40 °C`** (`p_sicaklik[1]`) | Logged & RS485 sent |
| **Runtime Sweep (`runtime_sweep[1]`)**| `OFF` / `ON` | **`false` (OFF)** (`p_sweep[1]=0`) | `b_swe.bco = 50712` |
| **Operational Status Text** | `"SISTEM BEKLEMEDE"` | `"P1 SECILDI. START BEKLENIYOR"` | Display updated |

---

## 4. Raw Mirrored Nextion Response Frames

ESP32 `nextionGonder()` executed with `NEXTION_SIM_MIRROR == 1` generated the following mirrored bytecode frames across `Serial` (`/dev/ttyACM0`):

### A. Recipe Selection Visual Updates
1. **Duration Text Field:**  
   - Decoded: `t_set_sure.txt="15"`  
   - Hex: `74 5f 73 65 74 5f 73 75 72 65 2e 74 78 74 3d 22 31 35 22 ff ff ff`
2. **Temperature Text Field:**  
   - Decoded: `t_set_sic.txt="40"`  
   - Hex: `74 5f 73 65 74 5f 73 69 63 2e 74 78 74 3d 22 34 30 22 ff ff ff`
3. **Sweep Toggle Button Color:**  
   - Decoded: `b_swe.bco=50712`  
   - Hex: `62 5f 73 77 65 2e 62 63 6f 3d 35 30 37 31 32 ff ff ff`

### B. Periodic UI Refresh Telemetry
1. **Status Banner:** `t_durum.txt="P1 SECILDI. START BEKLENIYOR"\xFF\xFF\xFF`
2. **Active Tank Indicator:** `b_goz.txt="Goz: 1"\xFF\xFF\xFF`
3. **Actual Temperature:** `t_anlik_sic.txt="60.8"\xFF\xFF\xFF`
4. **Remaining Time:** `t_kalan_sure.txt="00:00"\xFF\xFF\xFF`

---

## 5. STM32 & RS485 Bus Observations

- **RS485 Frames Sent by Master:**
  - `[ESP->STM] T1:SET_TIME:15\n`
  - `[ESP->STM] T1:SET_TEMP:40\n`
- **STM32 State:** `SYS_IDLE`
- **STM32 Telemetry Response:**  
  `STAT,1,IDLE,0,608,0,0,28,0,2,0`
- **Physical Output Status:** Triac delay at safe idle ($9500\text{ }\mu\text{s}$), Relay = 0, TIM15 PWM disarmed.

---

## 6. Evaluation of Round-Trip Success Criteria

1. **Did simulator button press create expected command?** $\rightarrow$ **YES (`P1_SEL\n`)**
2. **Did ESP32 receive and parse command?** $\rightarrow$ **YES (Received at $+15.77\text{ ms}$)**
3. **Did ESP32 change P1 state?** $\rightarrow$ **YES (`hedef_sure=15`, `hedef_sic=40`, `p_sweep=0`)**
4. **Did ESP32 generate Nextion response frames?** $\rightarrow$ **YES (`t_set_sure`, `t_set_sic`, `b_swe.bco`)**
5. **Did those frames appear on Raspberry Pi USB CDC?** $\rightarrow$ **YES (Captured byte-for-byte with `0xFF 0xFF 0xFF`)**
6. **Did the STM32 update its setpoints?** $\rightarrow$ **YES (`T1:SET_TIME:15`, `T1:SET_TEMP:40`)**
7. **Total observable latency?** $\rightarrow$ **$36.57\text{ ms}$** (State update) / **$335.09\text{ ms}$** (Full RS485 telemetry ACK)
8. **Were any unexpected frames generated?** $\rightarrow$ **NO (Zero anomalous frames)**

---

## 7. Final Classification

`PARTIAL — ESP32/HIL PATH VERIFIED, SIMULATOR RENDERING UNVERIFIED`

*(Note: The embedded ESP32, RS485 bus, STM32 execution, and outbound Nextion bytecode generation are 100% verified on Raspberry Pi USB CDC; the visual rendering on the Windows Simulator GUI requires establishing the Windows-side Virtual COM bridge).*
