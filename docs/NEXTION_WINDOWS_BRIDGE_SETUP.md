# NEXTION SIMULATOR WINDOWS BRIDGE SETUP & DIRECT RENDERING REPORT
**Project:** EAGLEULTRASONİK  
**Document Revision:** 2.0.0  
**Status:** `WINDOWS NEXTION BRIDGE — READY`

---

## 1. Executive Summary

The complete bidirectional communication and graphical rendering bridge between the Raspberry Pi 5 hardware testbed and the Windows Nextion Editor Simulator has been successfully established and verified:

```
+----------------------------------------------------------------------------------------------------+
|                                    WINDOWS HOST WORKSTATION                                        |
|                                                                                                    |
|   +----------------------------------------------------+                                           |
|   |            Nextion Editor Simulator                |                                           |
|   |         (User MCU Input Mode: COM3 @ 115200)       |                                           |
|   +----------------------------------------------------+                                           |
|                             ↕ (Serial Byte Stream)                                                 |
|   +----------------------------------------------------+                                           |
|   |        com0com Virtual Null-Modem Pair             |                                           |
|   |            COM3 (CNCA0) ◄══► COM8 (CNCB0)          |                                           |
|   +----------------------------------------------------+                                           |
|                             ↕                                                                      |
|   +----------------------------------------------------+                                           |
|   |       Windows TCP-to-COM Redirector Daemon         |                                           |
|   |       (scripts/windows_com_tcp_bridge.py)          |                                           |
|   +----------------------------------------------------+                                           |
+-----------------------------↕----------------------------------------------------------------------+
                              │ TCP Port 8888 (Tailscale Network Link: 100.99.150.99)
                              ▼
+----------------------------------------------------------------------------------------------------+
|                                    RASPBERRY PI 5 HUB (100.99.150.99)                              |
|   - scripts/rpi_hmi_bridge.py (Listening on TCP 0.0.0.0:8888)                                      |
|   - Target Device: /dev/ttyACM0 @ 115200 Baud (ESP32-S3 USB CDC)                                   |
+----------------------------------------------------------------------------------------------------+
```

---

## 2. Phase-by-Phase Verification Results

### Phase 1: Raspberry Pi HMI Bridge
- **Script Path:** [`scripts/rpi_hmi_bridge.py`](file:///c:/Users/ern0e/EAGLEULTRASONiK/scripts/rpi_hmi_bridge.py)
- **Target Device:** `/dev/ttyACM0` (ESP32-S3 USB CDC with `NEXTION_SIM_MIRROR == 1`)
- **Baud Rate:** `115200 Baud`
- **TCP Listener:** `0.0.0.0:8888`
- **Verification Command:** `netstat -tuln | grep 8888` $\rightarrow$ `LISTEN` (Verified active).

### Phase 2: Windows $\longleftrightarrow$ Raspberry Pi Network Reachability
- **Test Command:** `Test-NetConnection 100.99.150.99 -Port 8888`
- **Result:** `TcpTestSucceeded: True` (Direct low-latency Tailscale peer connection).

### Phase 3: Virtual COM Pair & Windows Bridge Redirector
- **Driver:** `com0com v3.0.0.0` (Signed 64-bit kernel driver).
- **Virtual Port Pair:**
  - `CNCA0` $\longleftrightarrow$ **`COM3`** (Assigned to Nextion Simulator).
  - `CNCB0` $\longleftrightarrow$ **`COM8`** (Assigned to TCP redirector).
- **Windows Bridge Daemon:** [`scripts/windows_com_tcp_bridge.py`](file:///c:/Users/ern0e/EAGLEULTRASONiK/scripts/windows_com_tcp_bridge.py)
- **Invocation Command:**
  ```powershell
  python scripts/windows_com_tcp_bridge.py COM8 100.99.150.99 8888 115200
  ```
- **Status:** Connected and running as a background daemon.

### Phase 4: Raw Bidirectional Loopback Test
- **Test:** Injected `DIAG\n` into `COM3` from Windows.
- **Trace:** `COM3` $\to$ `COM8` $\to$ Windows Bridge $\to$ TCP `100.99.150.99:8888` $\to$ Pi Bridge $\to$ `/dev/ttyACM0` $\to$ ESP32.
- **Result:** ESP32 diagnostic confirmation and live STM32 `STAT` telemetry streamed back into `COM3`. **PASSED.**

### Phase 5: Nextion Editor Simulator Configuration
1. Open `EKRAN/arayuz.HMI` in Nextion Editor.
2. Click **Debug** (F8).
3. In the bottom control pane under **User MCU Input**:
   - Mode: **ComPort**
   - Port: **`COM3`**
   - Baud Rate: **`115200`**
   - Click **Start**.

### Phase 6: Direct Simulator Rendering Test (Bridge Isolation)
Direct Nextion bytecode instructions were transmitted across the established TCP bridge to verify on-screen GUI element mutation without triggering embedded logic:

| Test Frame | Payload | Hex String (with `\xFF\xFF\xFF`) | Visual GUI Result |
| :--- | :--- | :--- | :--- |
| **Duration Setpoint** | `t_set_sure.txt="15"` | `74 5f 73 65 74 5f 73 75 72 65 2e 74 78 74 3d 22 31 35 22 ff ff ff` | `t_set_sure` updated to **`15`** |
| **Temperature Setpoint** | `t_set_sic.txt="40"` | `74 5f 73 65 74 5f 73 69 63 2e 74 78 74 3d 22 34 30 22 ff ff ff` | `t_set_sic` updated to **`40`** |
| **Sweep Button Color** | `b_swe.bco=2016` | `62 5f 73 77 65 2e 62 63 6f 3d 32 30 31 36 ff ff ff` | `b_swe` button turned **Green** |

---

## 3. Final Classification

`WINDOWS NEXTION BRIDGE — READY`
