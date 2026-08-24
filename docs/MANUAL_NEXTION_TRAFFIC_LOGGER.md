# PASSIVE BIDIRECTIONAL NEXTION HIL TRAFFIC LOGGER
**Project:** EAGLEULTRASONİK  
**Document Revision:** 1.0.0  
**Status:** `PASSIVE BIDIRECTIONAL LOGGER — READY`

---

## 1. Executive Summary & Architecture

The passive traffic logger runs transparently on the Windows host between the Virtual Null-Modem pair (`COM8`) and the Raspberry Pi TCP Socket (`100.99.150.99:8888`), recording all bidirectional communication in raw HEX format with automatic event classification:

```
+----------------------------------------------------------------------------------------------------+
|                                    WINDOWS WORKSTATION                                             |
|                                                                                                    |
|   +----------------------------------------------------+                                           |
|   |            Nextion Editor Simulator                |                                           |
|   |         (User MCU Input Mode: COM3 @ 115200)       |                                           |
|   +----------------------------------------------------+                                           |
|                             ↕ (Raw Serial Stream)                                                  |
|   +----------------------------------------------------+                                           |
|   |        com0com Virtual Null-Modem Pair             |                                           |
|   |            COM3 (CNCA0) ◄══► COM8 (CNCB0)          |                                           |
|   +----------------------------------------------------+                                           |
|                             ↕                                                                      |
|   +----------------------------------------------------+                                           |
|   |   PASSIVE RAW TRAFFIC LOGGER & FORWARDER           | ──► [scratch/manual_nextion_traffic_*.log] |
|   |   (scratch/passive_traffic_logger.py)              | ──► [scratch/manual_nextion_traffic_*_sum]|
|   +----------------------------------------------------+                                           |
+-----------------------------↕----------------------------------------------------------------------+
                              │ TCP Port 8888 (Tailscale Link: 100.99.150.99)
                              ▼
+----------------------------------------------------------------------------------------------------+
|                                    RASPBERRY PI 5 TESTBED (100.99.150.99)                          |
|   - scripts/rpi_hmi_bridge.py (Listening on TCP 0.0.0.0:8888)                                      |
|   - Target Device: /dev/ttyACM0 @ 115200 Baud (ESP32-S3 USB CDC)                                   |
|   - RS485 Half-Duplex Differential Bus: ESP32-S3 (GPIO8/18) ◄══► STM32G474RE Node 1 (PB10/11)      |
+----------------------------------------------------------------------------------------------------+
```

---

## 2. Event Log Format & Classification Rules

Every captured event is appended to `scratch/manual_nextion_traffic_YYYYMMDD_HHMMSS.log` in authoritative raw HEX with timestamping:

```text
[13:34:52.125431] (t_mono=1423.854910)
DIRECTION=SIM->ESP32
TRANSPORT=USB_CDC
HEX=50 31 5F 53 45 4C 0A
ASCII=P1_SEL
TYPE=HMI_COMMAND

[13:34:52.146200] (t_mono=1423.875679)
DIRECTION=ESP32->SIM
TRANSPORT=USB_CDC_MIRROR
HEX=74 5F 73 65 74 5F 73 75 72 65 2E 74 78 74 3D 22 31 35 22 FF FF FF
ASCII=t_set_sure.txt="15"
TYPE=NEXTION_FRAME

[13:34:52.151890] (t_mono=1423.881369)
DIRECTION=ESP32->STM32
TRANSPORT=RS485
HEX=5B 45 53 50 2D 3E 53 54 4D 5D 20 54 31 3A 53 45 54 5F 54 49 4D 45 3A 31 35 0A
ASCII=[ESP->STM] T1:SET_TIME:15
TYPE=RS485_COMMAND

[13:34:52.204500] (t_mono=1423.933979)
DIRECTION=STM32->ESP32
TRANSPORT=RS485
HEX=5B 53 54 4D 2D 3E 45 53 50 5D 20 53 54 41 54 2C 31 2C 49 44 4C 45 2C 30 2C 36 30 38 2C 30 2C 30 2C 32 38 2C 30 2C 32 2C 30 0A
ASCII=[STM->ESP] STAT,1,IDLE,0,608,0,0,28,0,2,0
TYPE=RS485_TELEMETRY
```

### Event Classification Matrix

| Classification Type | Detection Criteria | Primary Direction |
| :--- | :--- | :--- |
| **`HMI_COMMAND`** | Touch events sent from Nextion Simulator (`P1_SEL`, `b_swe`, `b_deg`, `CMD_START`, etc.) | `SIM->ESP32` |
| **`NEXTION_FRAME`** | Byte streams containing `0xFF 0xFF 0xFF` terminators (`t_set_sure.txt="..."`, `b_swe.bco=...`) | `ESP32->SIM` |
| **`RS485_COMMAND`** | Addressed master command lines (`[ESP->STM] T<g>:<CMD>`) | `ESP32->STM32` |
| **`RS485_TELEMETRY`**| Slave telemetry records (`[STM->ESP] STAT,<g>,...`) | `STM32->ESP32` |
| **`DEBUG`** | ESP32 watchdog and internal diagnostic strings (`DEBUG_ESP32: ...`, `--> ...`) | `ESP32->LOG` |
| **`ACK`** | Affirmative acknowledgments (`CK:`, `ACK`) | Both |
| **`NACK` / `ERR`** | Bus errors, negative acknowledgments (`NK:`, `HATA:`, `ERR`) | Both |
| **`UNKNOWN`** | Unrecognized formatting or binary payloads | Both |

---

## 3. Usage & Operational Workflow

### A. Starting the Logger (Passive Mode)
The logger is currently active in the background. To manually start or restart:
```powershell
python scratch/passive_traffic_logger.py COM8 100.99.150.99 8888 115200
```

### B. Operating Nextion Editor Simulator
1. In Nextion Editor, open `EKRAN/arayuz.HMI`.
2. Click **Debug** (F8).
3. Under **User MCU Input**, select **ComPort**, choose **`COM3`**, set Baud **`115200`**, and click **Start**.
4. Freely click buttons (`b_p1`, `b_swe`, `b_deg`, `b_start`, `b_stop`, `b_goz`, `b_up`, `b_down`, `b_ok`, `b_set`, `PAGE2_OPEN`, etc.).
5. Every single button press, ESP32 response, RS485 command, and live telemetry frame will be captured in real-time.

### C. Stopping the Logger & Generating Summary
When manual interaction is finished, stop the logger (via `Ctrl+C` or terminating the process). The logger will safely close serial and socket descriptors and generate:
- Full Raw Log: `scratch/manual_nextion_traffic_YYYYMMDD_HHMMSS.log`
- Cumulative Summary: `scratch/manual_nextion_traffic_YYYYMMDD_HHMMSS_summary.txt`

---

## 4. Final Classification

`PASSIVE BIDIRECTIONAL LOGGER — READY`
