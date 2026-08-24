# WINDOWS BRIDGE EMBEDDED PASSIVE TEE LOGGER REPORT
**Project:** EAGLEULTRASONİK  
**Document Revision:** 1.0.0  
**Status:** `WINDOWS BRIDGE TEE LOGGER — VERIFIED`

---

## 1. Executive Summary & Problem Resolution

### The Previous COM8 Port Ownership Conflict
In earlier iterations, `scratch/passive_traffic_logger.py` and `scripts/windows_com_tcp_bridge.py` were two separate scripts attempting to open `COM8` concurrently. On Windows, serial ports are exclusive kernel objects (`FILE_SHARE_READ` is not supported for COM devices), causing `PermissionError(13, 'Erişim engellendi')` and $0\text{-byte}$ logs.

### The Unified Solution: Embedded Passive Tee Logger
The passive raw traffic logger was integrated directly **inside** [`scripts/windows_com_tcp_bridge.py`](file:///c:/Users/ern0e/EAGLEULTRASONiK/scripts/windows_com_tcp_bridge.py).
- **Single Process Ownership:** The bridge script alone opens and owns `COM8`.
- **100% Transparent Forwarding:** Every incoming chunk is recorded to disk and immediately forwarded byte-for-byte without alteration, latency penalty, or payload transformation.
- **Zero Port Collisions:** No secondary logger processes are required.

```
+----------------------------------------------------------------------------------------------------+
|                                    WINDOWS WORKSTATION                                             |
|                                                                                                    |
|   +----------------------------------------------------+                                           |
|   |            Nextion Editor Simulator                |                                           |
|   |         (User MCU Input Mode: COM3 @ 115200)       |                                           |
|   +----------------------------------------------------+                                           |
|                             ↕ (Raw Serial Byte Stream)                                             |
|   +----------------------------------------------------+                                           |
|   |        com0com Virtual Null-Modem Pair             |                                           |
|   |            COM3 (CNCA0) ◄══► COM8 (CNCB0)          |                                           |
|   +----------------------------------------------------+                                           |
|                             ↕                                                                      |
|   +----------------------------------------------------+                                           |
|   |   WINDOWS TCP BRIDGE WITH EMBEDDED TEE LOGGER      | ──► [scratch/manual_nextion_bridge_*.log]  |
|   |   (scripts/windows_com_tcp_bridge.py)              | ──► [scratch/manual_nextion_bridge_*_sum]  |
|   +----------------------------------------------------+                                           |
+-----------------------------↕----------------------------------------------------------------------+
                              │ TCP Port 8888 (Tailscale Link: 100.99.150.99)
                              ▼
+----------------------------------------------------------------------------------------------------+
|                                    RASPBERRY PI 5 TESTBED (100.99.150.99)                          |
|   - scripts/rpi_hmi_bridge.py (Listening on TCP 0.0.0.0:8888)                                      |
|   - Target Device: /dev/ttyACM0 @ 115200 Baud (Physical ESP32-S3 with NEXTION_SIM_MIRROR == 1)      |
+----------------------------------------------------------------------------------------------------+
```

---

## 2. Exact Log Format & Classification Specification

Every raw byte transfer in both directions is captured in `scratch/manual_nextion_bridge_YYYYMMDD_HHMMSS.log`:

```text
[2026-08-18 14:28:52.125431] (t_mono=21430.125431, timing=RX_TIMESTAMP_ONLY)
DIR=HMI_TO_RPI
HEX=50 31 5F 53 45 4C 0A
ASCII=P1_SEL
BYTES=7
TYPE=HMI_COMMAND

[2026-08-18 14:28:52.146200] (t_mono=21430.146200, timing=RX_TIMESTAMP_ONLY)
DIR=RPI_TO_HMI
HEX=74 5F 73 65 74 5F 73 75 72 65 2E 74 78 74 3D 22 31 35 22 FF FF FF
ASCII=t_set_sure.txt="15"
BYTES=22
TYPE=NEXTION_FRAME

[2026-08-18 14:28:52.152890] (t_mono=21430.152890, timing=RX_TIMESTAMP_ONLY)
DIR=RPI_TO_HMI
HEX=5B 45 53 50 2D 3E 53 54 4D 5D 20 54 31 3A 53 45 54 5F 54 49 4D 45 3A 31 35 0A
ASCII=[ESP->STM] T1:SET_TIME:15
BYTES=25
TYPE=RS485_COMMAND
```

---

## 3. End-to-End Test & Verification Results

### Test A: Single-Process COM8 Ownership & Network Reachability
- `COM8` opened cleanly by `windows_com_tcp_bridge.py` without sharing violations.
- TCP socket connected to `100.99.150.99:8888` instantly.

### Test B: Direct Nextion Rendering
- Direct frame `t_set_sure.txt="15"\xFF\xFF\xFF` forwarded to `COM3`.
- Simulator GUI visual update confirmed.

### Test C: `P1_SEL` Round-Trip & Telemetry Trace
- Injected `P1_SEL\n` into `COM3`.
- Log confirmed:
  1. `DIR=HMI_TO_RPI`: `P1_SEL` (`50 31 5F 53 45 4C 0A`)
  2. `DIR=RPI_TO_HMI`: `t_set_sure.txt="15"` + `\xFF\xFF\xFF`
  3. `DIR=RPI_TO_HMI`: `t_set_sic.txt="40"` + `\xFF\xFF\xFF`
  4. `DIR=RPI_TO_HMI`: `b_swe.bco=50712` + `\xFF\xFF\xFF`
  5. `DIR=RPI_TO_HMI`: `[ESP->STM] T1:SET_TIME:15`
  6. `DIR=RPI_TO_HMI`: `[STM->ESP] STAT,1,IDLE,0,...`

---

## 4. Operational Instructions (Start / Stop)

### To Run the Bridge with Live Tee Logging:
```powershell
python scripts/windows_com_tcp_bridge.py COM8 100.99.150.99 8888 115200
```

### To Stop:
Press `Ctrl+C` in the running terminal. The bridge automatically closes sockets, flushes buffers, and writes the cumulative summary to `scratch/manual_nextion_bridge_YYYYMMDD_HHMMSS_summary.txt`.

### Old Logger Status:
- `scratch/passive_traffic_logger.py` is **deprecated** and must not be run to avoid `COM8` ownership conflicts.

---

## 5. Final Classification

`WINDOWS BRIDGE TEE LOGGER — VERIFIED`
