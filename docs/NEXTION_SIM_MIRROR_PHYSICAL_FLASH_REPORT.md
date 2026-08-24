# PHYSICAL ESP32-S3 FIRMWARE FLASH & NEXTION MIRROR VERIFICATION REPORT
**Project:** EAGLEULTRASONİK  
**Document Revision:** 1.0.0  
**Target:** Physical ESP32-S3 Master Node on Raspberry Pi 5 (`/dev/ttyACM0`)  
**Status:** `NEXTION MIRROR — PHYSICAL FLASH VERIFIED`

---

## 1. Executive Summary

The ESP32-S3 master node connected to the Raspberry Pi 5 testbed has been successfully compiled and reflashed with the updated firmware image containing `#define NEXTION_SIM_MIRROR 1`.

Live Hardware-In-The-Loop capture across the entire signal chain (Physical ESP32 $\longleftrightarrow$ Raspberry Pi $\longleftrightarrow$ TCP Bridge $\longleftrightarrow$ Windows `COM3`) confirmed that:
1. Physical ESP32 booted cleanly with zero reset loops.
2. Multi-drop RS485 communication with STM32 Node 1 resumed immediately with valid telemetry and watchdog health.
3. Every `nextionGonder()` frame is now mirrored live onto USB CDC `/dev/ttyACM0` and delivered byte-for-byte with `0xFF 0xFF 0xFF` terminators to Windows `COM3`.
4. Injecting `P1_SEL` triggered exact mirrored Nextion instructions: `t_set_sure.txt="15"`, `t_set_sic.txt="40"`, and `b_swe.bco=50712`.

---

## 2. Compilation & Build Metrics

- **Toolchain:** `arduino-cli` v1.5.1 on Raspberry Pi 5 (`aarch64-linux-gnu`)
- **Target FQBN:** `esp32:esp32:esp32s3` (ESP32 Arduino Core 3.3.11 / GCC 14.2.0)
- **Source File:** `esp32/ekran_kontrol/ekran_kontrol.ino` (66,214 bytes, 1,701 lines)
- **Flash Utilization:**
  - Program Storage: **`384,250 bytes (29%)`** of `1,310,720 bytes` maximum.
  - Dynamic Memory (RAM): **`23,416 bytes (7%)`** of `327,680 bytes` maximum.
- **Compiler Diagnostics:** Zero errors, zero warnings.

---

## 3. Physical Flash & Upload Verification

- **Upload Interface:** `/dev/ttyACM0` @ `921,600 Baud` via `esptool v5.3.1`
- **Target Hardware Detected:**
  - Chip: `ESP32-S3 (QFN56) (revision v0.2)`
  - MAC Address: `a4:cb:8f:d8:d0:d8`
  - Flash Size: `8MB Embedded PSRAM / 40MHz Crystal`
- **Flash Write Result:**
  - Total Data Written: `385,024 bytes (218,017 compressed)` in `3.3 seconds (929.1 kbit/s)`
  - Verification: `Hash of data verified.` $\to$ Hard reset via RTS pin successful.

---

## 4. Boot & RS485 Health Verification

Following flash reboot:
- **Watchdog State:** `DEBUG_ESP32: WDT tank=1 connected=1 age_ms=92` (Continuous healthy heartbeat).
- **STM32 Telemetry:** Real-time `STAT,1,IDLE,0,608,0,0,28,0,2,0` streaming at 100ms intervals.
- **RS485 Command Acknowledgment:** STM32 acknowledged sweep config `[STM->ESP] CK:SWEEP:OFF,CENTER_RESTORED`.

---

## 5. Live Nextion Mirror Frame Evidence (`NX-SIM-RT-001` Test)

Captured live on Windows `COM3` following `P1_SEL` command injection:
- **Total Bytes Captured:** `3,985 bytes`
- **Total `0xFF 0xFF 0xFF` Terminators:** **`88`**

```text
[NEXTION_FRAME] t_set_sure.txt="15"      | Hex: 74 5f 73 65 74 5f 73 75 72 65 2e 74 78 74 3d 22 31 35 22 ff ff ff
[NEXTION_FRAME] t_set_sic.txt="40"       | Hex: 74 5f 73 65 74 5f 73 69 63 2e 74 78 74 3d 22 34 30 22 ff ff ff
[NEXTION_FRAME] b_swe.bco=50712          | Hex: 62 5f 73 77 65 2e 62 63 6f 3d 35 30 37 31 32 ff ff ff
[NEXTION_FRAME] t_kalan_sure.txt="00:00" | Hex: ... ff ff ff
[NEXTION_FRAME] t_anlik_sic.txt="60.8"   | Hex: ... ff ff ff
[NEXTION_FRAME] b_goz.txt="Goz: 1"       | Hex: ... ff ff ff
```

---

## 6. Controlled Rollback Procedure (For Production Baseline)

When the testing phase is concluded, revert the test mirror by following these steps:
1. In `esp32/ekran_kontrol/ekran_kontrol.ino` (line 18), change:
   ```cpp
   #define NEXTION_SIM_MIRROR 0
   ```
2. Recompile and reflash the physical ESP32:
   ```bash
   arduino-cli compile --fqbn esp32:esp32:esp32s3 esp32/ekran_kontrol
   arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:esp32s3 esp32/ekran_kontrol
   ```

---

## 7. Final Classification

`NEXTION MIRROR — PHYSICAL FLASH VERIFIED`
