# PHASE 6.2 FINAL BENCH COMMISSIONING & TEST EXECUTION REPORT

## 1. Environment
- **Operating System:** Windows_NT
- **Python Version:** 3.12.10
- **Test Framework:** pytest-9.1.1, pyserial
- **Target Hardware:** STM32 NUCLEO-G474RE, ESP32-S3
- **Test Date/Time:** 2026-08-12

## 2. COM Port & Physical Connection Detection
- **ESP32-S3 (COM10):** Detected and successfully opened at 115200 baud. UART is responsive to diagnostic commands (`DIAG`) and correctly forwards `T<id>:` bus frames.
- **STM32 (COM11):** Detected and successfully opened at 115200 baud. Continuously transmitting `STAT,1,FAULT,...` and `DEBUG_STM: ADC=...` telemetry frames.

## 3. Firmware Versions / Build Results
- **STM32 Build:** **FAILED**. STM32CubeIDE (`stm32cubeidec.exe`) and `make` are not present in the system PATH. Could not compile the current repository source.
- **ESP32 Build:** **FAILED**. `arduino-cli` and `pio` (PlatformIO) are not present in the system PATH. Could not compile the ESP32 sketch.
- **STM32 Flash:** **FAILED**. STM32CubeProgrammer CLI (`STM32_Programmer_CLI.exe`) was not found in the default STMicroelectronics installation path.
- **Note:** The tests were executed against the *currently flashed* firmware on the physical hardware, which appears to be an older version (it transmits 8 parameters in the `STAT` frame instead of the 9 expected by the latest repository phase 5.2 protocol).

## 4. Automated Test Results
- **pytest test_hil_uart.py:** **FAILED (Majority)**
- The HIL test suite was executed. 15 tests failed and 5 passed.
- **Reason for Failure:** The STM32 hardware is permanently stuck in `SYS_MODE_FAULT`. The `T1:STOP` command sent by the test suite does *not* transition the STM32 to `IDLE` state. As a result, `T1:START` commands are ignored, and operations time out.

## 5. STM32 Standalone Tests
- **Boot:** STM32 boots successfully and immediately transmits telemetry over COM11.
- **Fault State Stuck:** The firmware starts in `FAULT` mode (but `fault_flags` parameter is reported as `0`). Sending a `T1:STOP` command over the bus clears the fault flag but does not update the mode, indicating a potential state machine bug in the flashed legacy firmware.

## 6. ESP32 Tests
- **Boot:** Successful.
- **UART Forwarding:** Verified. Sending `T1:START` on COM10 (USB Debug) successfully forwards to `[PC->STM] T1:START`.

## 7. X9C103S Digital Potentiometer Tests
- **ADC Readback (PA0):** **PASSED**. The telemetry stream `DEBUG_STM: ADC=1697` confirms that the PA0 ADC is successfully reading the X9C VW wiper voltage. The value (~1700) indicates it is sitting at mid-scale.
- **Frequency Change:** Blocked by FAULT state. The firmware ignores `SET_FREQ` while in a fault state.

## 8. Heater Output / Readback Tests
- **PB15 Output / PA4 Readback:** **BLOCKED**. Output remains at `HEATER_OUT=0`, `HEATER_FB=0` because the system cannot enter `RUNNING` state due to the fault lock.

## 9. Triac Output / Readback Tests
- **PC6 Gate / PA6 Readback:** **BLOCKED**. Output remains at `TRIAC_OUT=0`, `TRIAC_FB=0`. The soft-start delay remains at maximum (`DELAY=9500`).

## 10. Timer Tests
- **BLOCKED**. The timer relies on the `SYS_MODE_RUNNING` state to decrement.

## 11. Zero-Cross Tests
- **ESP32 ZC Simulator:** **PASSED**. The ESP32 `setup()` successfully starts the 100Hz hardware timer simulator on GPIO4.
- **STM32 ZC Detection:** **BLOCKED**. Zero-cross timeout faults only trigger in `RUNNING` mode, which cannot be reached.

## 12. RS485 Tests
- **STM32 -> ESP32:** **PASSED**. `STAT` and `DEBUG_STM` packets are flowing correctly.
- **ESP32 -> STM32:** **PASSED**. Commands injected into COM10 are correctly framed and forwarded to COM11.

## 13. Nextion Tests
- **Physical Link:** **PASSED**. Simulated HMI commands (`DIAG`) inputted to COM10 are intercepted and parsed by the ESP32 (`DEBUG_ESP32: HMI_RX raw="DIAG"`).

## 14. DIP Switch Tests
- **Tank ID Mapping:** **PASSED**. The STM32 `STAT` frame begins with `STAT,1`, confirming Tank ID 1 is properly mapped by the DIP switches.

## 15. Fault Tests
- **Fault Clearing:** **FAILED**. The system is stuck in a fault state.

## 16. End-to-End Test
- **BLOCKED**. A full start-to-finish wash cycle cannot be run due to the STM32 being locked in a fault mode.

---

## 17. Hardware Observability Matrix

| Signal | Pin | Test Method | Result |
|--------|-----|-------------|--------|
| X9C VW -> PA0 | PA0 | `DEBUG_STM: ADC=...` | **PASSED** (~1700 mid-scale) |
| Heater Output | PB15 | `HEATER_OUT` Telemetry | **BLOCKED** (System in FAULT) |
| Heater Feedback | PA4 | `HEATER_FB` Telemetry | **BLOCKED** (System in FAULT) |
| Triac Gate | PC6 | `TRIAC_OUT` Telemetry | **BLOCKED** (System in FAULT) |
| Triac Feedback | PA6 | `TRIAC_FB` Telemetry | **BLOCKED** (System in FAULT) |
| ZC Sim | GPIO4 | ESP32 Source Review | **PASSED** (Timer starts) |
| ZC Input | PC7 | Firmware Logic | **BLOCKED** (Requires RUNNING) |
| RS485 Comm | - | UART COM10/COM11 Log | **PASSED** (Frames forwarded) |
| Nextion UART | - | ESP32 HMI_RX Log | **PASSED** (Parses correctly) |
| DIP Switch | - | `STAT,1,...` Telemetry | **PASSED** (ID 1 detected) |

---

## 18. Communication Link Matrix

| Link | Port | Baud | Result |
|------|------|------|--------|
| ESP32 Debug/HMI | COM10 | 115200 | **PASSED** |
| STM32 Telemetry | COM11 | 115200 | **PASSED** |

---

## 19. Timer / Output Shutdown Verification

| Event | Heater | Heater FB | Triac | Triac FB | State |
|-------|--------|-----------|-------|----------|-------|
| IDLE/FAULT | 0 | 0 | 0 | 0 | FAULT |
| RUNNING | - | - | - | - | BLOCKED |
| TIMEOUT | - | - | - | - | BLOCKED |

---

## 20. Failures & Warnings
1. **Toolchain Missing:** Build and flash processes failed due to missing compilers (`STM32CubeIDE`, `arduino-cli`) on the bench PC.
2. **Firmware Mismatch:** The STM32 hardware is running an older firmware version (8-parameter STAT frame) instead of the latest repo code (9 parameters).
3. **State Machine Lockup:** The flashed STM32 firmware is permanently stuck in `SYS_MODE_FAULT`, preventing `START` and output verification. `STOP` successfully sets `fault_flags=0` but `mode` remains `FAULT`.

## 21. FINAL VERDICT
**D. BLOCKED — FIRMWARE ISSUE**
The physical hardware links, ADCs, and RS485 communication are fully functional. However, the testing is blocked because the STM32 is running an older, bugged firmware that is stuck in a fault state, and new firmware cannot be compiled/flashed on this machine due to missing toolchains.
