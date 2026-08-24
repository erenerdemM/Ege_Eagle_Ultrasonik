# REMOTE NEXTION SIMULATOR ARCHITECTURE & INTEGRATION SPECIFICATION
**Project:** EAGLEULTRASONİK  
**Document Revision:** 1.0.0  
**Target:** Remote Nextion Editor Simulator (Windows PC) $\leftrightarrow$ Raspberry Pi 5 $\leftrightarrow$ ESP32-S3 Master $\leftrightarrow$ STM32G474RE Slave Node  
**Status:** `INVESTIGATION & TOPOLOGY VERIFIED`

---

## 1. Current Physical Topology Discovery

The current physical desktop testbed is configured with the following active node topography:

```
+----------------------------------------------------------------------------------------------------+
|                                    WINDOWS HOST PC (DEVELOPMENT)                                  |
|   - Nextion Editor v1.65.1+ (GUI Simulator)                                                        |
|   - Virtual Serial Bridge / TCP Client (e.g., com2tcp / pyserial client / raw TCP bridge)         |
|   - IP Address: Connected to local LAN / Tailscale VPN (Pi IP: 100.99.150.99)                     |
+----------------------------------------------------------------------------------------------------+
                                                  │
                                                  │  Ethernet / Tailscale SSH & TCP (Port 22, Port 8888)
                                                  ▼
+----------------------------------------------------------------------------------------------------+
|                                    RASPBERRY PI 5 (CENTRAL TEST HUB)                               |
|   - Host IP: 100.99.150.99 | OS: Linux Debian (aarch64)                                           |
|   - Active USB Ports:                                                                             |
|       * /dev/ttyACM0 (QinHeng USB Single Serial VID:1A86 PID:55D3) -> ESP32-S3 USB Debug & CDC    |
|       * /dev/ttyACM1 (STMicroelectronics STLINK-V3 VID:0483 PID:374E) -> STM32 Nucleo VCP & SWD   |
|   - Direct Pi GPIO -> ESP32 HMI UART (GPIO16/17): NOT PHYSICALLY CONNECTED (Requires USB-UART)    |
+----------------------------------------------------------------------------------------------------+
                         │                                                 │
      USB CDC / UART     │                                                 │ ST-LINK VCP / SWD
      (/dev/ttyACM0)     │                                                 │ (/dev/ttyACM1)
                         ▼                                                 ▼
+------------------------------------+                      +----------------------------------------+
|       ESP32-S3 MASTER NODE         |                      |          STM32G474RE SLAVE NODE        |
| - UART0 (USB Debug): 115200 Baud   |                      | - LPUART1 (ST-LINK VCP): 115200 Baud   |
| - UART1 (RS485): GPIO18/8 (115200) |                      | - USART3 (RS485 Bus): PB10/PB11        |
| - UART2 (HMI): GPIO16/17 (9600)    |                      | - TIM15 Soft-Start PWM (20-40 kHz)     |
| - DE/RE Pin: GPIO5                 |                      | - OPAMP3 PT100 Signal Conditioning     |
| - ZC Sim: GPIO4 (100Hz Square Wave)|                      | - PC7 (CN5-2): Zero-Cross EXTI7 Input  |
+------------------------------------+                      +----------------------------------------+
                  │                                                             ▲
                  │             RS485 MULTI-DROP ASCII UART BUS                 │
                  └─────────────────────────────────────────────────────────────┘
                     Half-Duplex Differential Bus (115200 Baud, 8N1)
```

---

## 2. ESP32 HMI UART Trace & Source Analysis

Analysis of `esp32/ekran_kontrol/ekran_kontrol.ino` confirms the following hardware and software configuration for HMI communication:

### A. ESP32 UART Peripheral Instances
1. **`Serial2` (Nextion Physical HMI UART)**:
   - **Instance:** `HardwareSerial Serial2` (ESP32-S3 UART2 Controller)
   - **RX Pin:** `GPIO16` (`#define RXD2 16`)
   - **TX Pin:** `GPIO17` (`#define TXD2 17`)
   - **Baud Rate:** `9600 Baud` (`SERIAL_8N1`)
   - **Initialization Code:**
     ```cpp
     Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
     ```
   - **Protocol Framing:** Nextion standard 3-byte terminator `0xFF 0xFF 0xFF` on TX, line-terminated ASCII or raw event frames on RX.
   - **Consumer:** Polled non-blockingly in `loop()` via `hatOku(Serial2, gelenMesaj, satirNextion)` $\to$ processed by `komutIsle(satirNextion)`.

2. **`Serial` (ESP32 USB Debug & HIL Test Injection Channel)**:
   - **Instance:** `HardwareSerial Serial` (USB CDC / UART0 via on-board CH343/CH9102 chip at `/dev/ttyACM0` on Pi)
   - **Baud Rate:** `115200 Baud` (`SERIAL_8N1`)
   - **Initialization Code:**
     ```cpp
     Serial.begin(115200);
     ```
   - **Input Channel Behavior:** Polled in `loop()` via `hatOku(Serial, usbMesaj, satirUsb)`. Non-bus commands are passed directly into `komutIsle(satirUsb)`.

3. **`Serial1` (STM32 RS485 Bus)**:
   - **Instance:** `HardwareSerial Serial1` (ESP32-S3 UART1 Controller)
   - **RX Pin:** `GPIO18` (`#define STM_RXD 18`)
   - **TX Pin:** `GPIO8` (`#define STM_TXD 8`)
   - **Direction Control (DE/RE):** `GPIO5` (`#define RS485_DE_PIN 5`)
   - **Baud Rate:** `115200 Baud` (`SERIAL_8N1`)

### B. HMI TX / RX Data Flow
```
HMI Touch Event (Simulator / Nextion)
   │
   ▼
[HMI UART RX (ESP32 GPIO16)]  ──► hatOku(Serial2, ...) ──► komutIsle() ──► State Mutation & RS485 Frame TX (Serial1)
                                                                                  │
                                                                                  ▼
[HMI UART TX (ESP32 GPIO17)]  ◄── nextionGonder() ◄── 1000ms UI Refresh / State Change (t_durum, t_kalan_sure, bco)
```

---

## 3. Raspberry Pi Serial Inventory

Execution of serial enumeration on Raspberry Pi 5 (`python list_serial_devices.py` / `udevadm`):

| Device Path | Device Symlink | USB Vendor | USB Model | Baud Rate | Assigned Purpose |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `/dev/ttyACM0` | `usb-1a86_USB_Single_Serial_5C4C166947-if00` | `1a86` (QinHeng) | `USB_Single_Serial` | 115200 | ESP32-S3 USB Debug Console & Direct Command Injection |
| `/dev/ttyACM1` | `usb-STMicroelectronics_STLINK-V3_...-if02` | `0483` (STMicro) | `STLINK-V3` | 115200 | STM32 Nucleo VCP Telemetry & OpenOCD SWD Debugger |
| `/dev/ttyAMA10`| `/dev/serial0` | Broadcom MMIO | PL011 UART | 115200 | Dedicated Pi 5 Linux Kernel Debug Console Header |

> [!IMPORTANT]
> **HARDWARE DISCOVERY FINDING:**  
> There is currently **NO USB-to-UART adapter** plugged into the Raspberry Pi that is wired to ESP32 pins **GPIO16 (RXD2)** and **GPIO17 (TXD2)**.  
> The physical Nextion UART (`Serial2`) is currently open/unwired on the breadboard or wired to a terminal header.

---

## 4. Simulator Limitation & Capabilities Analysis

The official Nextion Editor Debug Simulator (v1.65.1+) provides the following interfaces:

1. **User MCU Input Mode (COM Port Mode)**:
   - Nextion Editor Simulator can bind to any native or virtual Windows COM port (e.g. `COM1` .. `COM32`) at selectable baud rates (e.g., `9600`, `115200`).
   - When active, all button press events (`Touch Press` / `Touch Release`) defined in `arayuz.HMI` send their compiled payload (e.g. `print "P1_SEL\n"`) out through the COM port.
   - Any Nextion instruction received from the COM port terminated by `0xFF 0xFF 0xFF` (e.g., `t0.txt="PROGRAM P1"`, `b_swe.bco=2016`, `t_kalan_sure.txt="14:32"`) is parsed and visually rendered on the simulated display screen in real-time.
2. **Simulator Limitations**:
   - The Nextion Editor Simulator does **NOT** have a native raw TCP client built into its GUI; it strictly requires a **Windows COM Port** (Real physical COM port or Virtual COM Port via `com0com` / `pyserial`).
   - Therefore, a **TCP-to-Virtual-COM redirector** on Windows is required to bridge the network socket from Raspberry Pi into the Nextion Editor Simulator.

---

## 5. Evaluation of Architectural Options

| Option | Architecture Concept | Required Hardware | Required Software | Complexity | Preserves Real ESP32 HMI UART? | Reliability | Risk |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **OPTION A** | Nextion Sim $\to$ Windows Virtual COM $\to$ TCP Bridge $\to$ RPi $\to$ RPi GPIO UART $\to$ ESP32 GPIO16/17 | Level Shifter / Jumper Wires | `com0com`, `socat` / Python TCP bridge | Medium | **YES** | High | Low (Requires Pi 5 GPIO overlay enable) |
| **OPTION B** | Nextion Sim $\to$ Windows Virtual COM $\to$ TCP Bridge $\to$ RPi $\to$ **USB-UART Dongle** $\to$ ESP32 GPIO16/17 | **3.3V USB-UART Dongle (CP2102/CH340/FTDI)** | `com0com`, `pyserial` redirector on Pi | **Low** | **YES (100% Exact Physical Path)** | **Highest** | **Zero Risk** |
| **OPTION C** | Nextion Sim $\to$ Windows COM $\to$ SSH Tunnel $\to$ RPi Serial Port | USB-UART Dongle | SSH Client + Port Forwarding | Medium | **YES** | High | Low (SSH overhead) |
| **OPTION D** | Nextion Sim $\to$ TCP $\to$ RPi $\to$ ESP32 USB `/dev/ttyACM0` (CDC Injection) | None (Existing USB Cable) | Python Bridge on Pi | Very Low | **NO** (Bypasses GPIO16/17 `Serial2`) | Medium | High (Cannot visualize Nextion response without firmware mirroring) |
| **OPTION E** | Direct Windows PC USB-UART Cable $\to$ ESP32 GPIO16/17 | Long USB Cable to Desk | Nextion Simulator binds direct to PC COM | Very Low | **YES** | Highest | Zero (Requires PC physically near breadboard) |

---

## 6. Selected Architecture: OPTION B (Physical USB-UART Bridge) with OPTION D Fallback

### Primary Target: OPTION B (Full Bidirectional Physical UART Loop)

```
[Windows PC]
   Nextion Editor Simulator
          ↕  (COM10 / Virtual COM)
   com2tcp / pyserial redirector
          ↕  (TCP Port 8888 via Tailscale: 100.99.150.99)
[Raspberry Pi 5]
   tcp_serial_bridge.py (Listens on 8888)
          ↕  (/dev/ttyUSB0 @ 9600 Baud)
   3.3V USB-UART Adapter (CP2102 / CH340)
          ↕  (TXD -> 1kΩ -> GPIO16, RXD <- 1kΩ <- GPIO17, Common GND)
[ESP32-S3 Master]
   Serial2 (GPIO16/17 @ 9600 Baud)
          ↕
   Full State Machine & Safety Logic
          ↕  (Serial1 RS485 @ 115200 Baud)
[STM32G474RE Slave Node]
   TIM15 Soft-Start PWM + OPAMP3 PT100 ADC + Heater Relay
```

---

## 7. Required Hardware, Cables & Pinout Specification

> [!CAUTION]
> **ADDITIONAL USB-UART CONNECTION REQUIRED**  
> To test the exact physical `Serial2` (GPIO16/17) hardware path without the physical display, a 3.3V USB-to-UART adapter must be plugged into any available USB 2.0/3.0 port on the Raspberry Pi 5.

### Physical Wiring Table for USB-UART Dongle $\leftrightarrow$ ESP32-S3:

| Dongle Pin | Sinyal Adı | Seri Koruma Direnci | ESP32-S3 Pin | ESP32 Sinyal | Gerilim Düzeyi | Notlar |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **USB-UART TXD** | Master Transmit | **1k$\Omega$ Seri Direnç** | **GPIO16** | **RXD2 (UART2 RX)** | **3.3V LVTTL** | **KESİNLİKLE 5V DEĞİL!** |
| **USB-UART RXD** | Master Receive | **1k$\Omega$ Seri Direnç** | **GPIO17** | **TXD2 (UART2 TX)** | **3.3V LVTTL** | 3.3V Mantıksal Seviye |
| **USB-UART GND** | Sinyal Toprağı | **0$\Omega$ (Doğrudan)** | **GND Pin** | **Common GND Bus** | **0V Referans** | Ortak Breadboard GND |
| **USB-UART 5V/VCC** | Besleme Ucu | **BAĞLANMAYACAK** | **BOŞTA** | **BOŞTA** | N/A | **AÇIKTA BIRAKIN** (Ters akım yasağı) |

---

## 8. Required Bridge Software Implementation

### A. Raspberry Pi Side Bridge Daemon (`scripts/rpi_hmi_bridge.py`)
A lightweight, non-blocking bidirectional Python bridge that listens on TCP port `8888` and forwards raw serial frames to/from the USB-UART adapter:

```python
#!/usr/bin/env python3
"""
rpi_hmi_bridge.py - TCP Socket to Serial Port Bridge for Nextion HMI Simulator
Listens on 0.0.0.0:8888 and bridges bidirectionally to /dev/ttyUSB0 (or /dev/ttyACM0)
"""
import socket
import select
import serial
import sys
import time

SERIAL_PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"
BAUD_RATE = int(sys.argv[2]) if len(sys.argv) > 2 else 9600
TCP_PORT = 8888

def run_bridge():
    print(f"[BRIDGE] Opening serial port {SERIAL_PORT} @ {BAUD_RATE} baud...")
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.05)
    
    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_sock.bind(("0.0.0.0", TCP_PORT))
    server_sock.listen(1)
    print(f"[BRIDGE] TCP Server listening on port {TCP_PORT}. Waiting for Windows Nextion Simulator...")

    while True:
        client_sock, client_addr = server_sock.accept()
        print(f"[BRIDGE] Connection accepted from {client_addr}")
        client_sock.setblocking(False)

        try:
            while True:
                # Read from Serial -> Send to TCP
                if ser.in_waiting > 0:
                    data = ser.read(ser.in_waiting)
                    client_sock.sendall(data)

                # Read from TCP -> Send to Serial
                r, _, _ = select.select([client_sock], [], [], 0.02)
                if r:
                    tcp_data = client_sock.recv(4096)
                    if not tcp_data:
                        print("[BRIDGE] Client disconnected.")
                        break
                    ser.write(tcp_data)
                    ser.flush()
        except (socket.error, serial.SerialException) as exc:
            print(f"[BRIDGE] Connection reset: {exc}")
        finally:
            client_sock.close()

if __name__ == "__main__":
    run_bridge()
```

### B. Windows Side Connection Options
1. **Virtual COM Port with `hub4com` / `com2tcp`**:
   ```cmd
   com2tcp.exe \\.\CNCB0 100.99.150.99 8888
   ```
   Nextion Editor Simulator connects to `CNCA0` at 9600 Baud.
2. **Direct Python Injection CLI (`scripts/send_hmi_cmd.py`)**:
   For scripted regression test execution directly from Windows without GUI.

---

## 9. Command & Response Flow Paths

### Command Flow (Windows $\to$ STM32)
1. User clicks `b_p1` or `b_start` on Nextion Editor Simulator on Windows PC.
2. Nextion Simulator transmits ASCII touch string: `P1_SEL\n` or `CMD_START|15|40\n`.
3. Windows Virtual COM forwards bytes across TCP socket to Pi 5 (`100.99.150.99:8888`).
4. Pi 5 `rpi_hmi_bridge.py` writes bytes to physical USB-UART adapter (`/dev/ttyUSB0`).
5. USB-UART TX drives ESP32 GPIO16 (RXD2).
6. ESP32 `hatOku(Serial2)` buffers line, invokes `komutIsle("P1_SEL")`.
7. ESP32 updates runtime state arrays and sends RS485 bus frame: `T1:SET_TIME:15\n`, `T1:SET_TEMP:40\n`.
8. STM32 USART3 receives frame, sets internal targets, and acknowledges on bus.

### Response Flow (STM32 $\to$ Windows Simulator)
1. STM32 executes control loop, samples PT100 ADC, measures elapsed seconds.
2. STM32 transmits telemetry frame on RS485 bus: `STAT,1,RUNNING,899,250,0,50,28,0\n`.
3. ESP32 `Serial1` (GPIO18) receives telemetry, invokes `stmTelemetryIsle()`.
4. ESP32 formats Nextion visual commands: `t_kalan_sure.txt="14:59"` + `0xFF 0xFF 0xFF`, `t_anlik_sic.txt="25.0"` + `0xFF 0xFF 0xFF`.
5. ESP32 `Serial2` (GPIO17) transmits raw Nextion bytes.
6. USB-UART RX receives bytes, passes to Pi 5 daemon.
7. Pi 5 daemon transmits over TCP socket to Windows PC.
8. Windows Virtual COM delivers bytes to Nextion Editor Simulator.
9. Nextion Editor Simulator updates on-screen timer, temperature, and status text in real-time.

---

## 10. Safety Considerations & Invariants

1. **Zero High-Voltage AC**: Desktop HIL testing is strictly 3.3V DC TTL.
2. **No 5V Interconnection**: Pi USB 5V, ESP32 USB 5V, and Nucleo 5V remain strictly separated.
3. **Common Ground**: All boards must share the single logic GND bus to prevent ground offset errors during 115200/9600 baud serial transmission.
4. **Current Limiting**: All inter-board MCU connections (GPIO16, GPIO17, GPIO4, GPIO8, GPIO18) must have physical **1k$\Omega$ series protection resistors**.
5. **Watchdog Interlock**: If the TCP bridge or serial link is severed for $>3000\text{ ms}$, ESP32 safety watchdog triggers, setting `durum_metni = "Kart Yok!"` and stopping all active PWM processes.

---

## 11. Final Classification & Next Steps

`REMOTE NEXTION SIMULATOR PATH — HARDWARE BRIDGE REQUIRED`

### Actionable Next Steps:
1. Connect a 3.3V USB-to-UART adapter to Raspberry Pi 5 USB port.
2. Wire Dongle TX $\to$ 1k$\Omega$ $\to$ ESP32 GPIO16, Dongle RX $\to$ 1k$\Omega$ $\to$ ESP32 GPIO17, Dongle GND $\to$ Common GND.
3. Start `rpi_hmi_bridge.py` on Raspberry Pi 5.
4. Bind Nextion Editor Simulator to the virtual COM port on Windows and execute test matrix.
