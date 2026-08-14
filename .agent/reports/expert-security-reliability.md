> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONiK Security & Reliability Audit Report

## 1. Missing Independent Watchdog (IWDG) enables thermal runaway upon main loop hang
ID: SEC-01
SEVERITY: CRITICAL
STATUS: CONFIRMED
CATEGORY: SAFETY RISK
FILE: C:\Users\ern0e\EAGLEULTRASONiK\STM32\Ultrasonik_G4_Master\Core\Src\main.c
LINE: 180
FUNCTION: main
TITLE: Missing Independent Watchdog (IWDG) enables thermal runaway upon main loop hang
OBSERVED BEHAVIOR: The firmware does not initialize or refresh any hardware watchdog timer (IWDG/WWDG).
EXPECTED BEHAVIOR: A hardware watchdog should be configured to reset the MCU if the main loop stalls or hangs.
EVIDENCE: The `main.c` file and `stm32g4xx_hal_conf.h` lack any IWDG initialization or `HAL_IWDG_Refresh()` calls.
ROOT CAUSE: Watchdog peripheral was omitted from the STM32CubeMX configuration and main loop.
IMPACT: If the `while(1)` main loop hangs (e.g., due to an EMI glitch, memory corruption, or infinite loop), the heater relay will remain latched in its current state. Furthermore, high-priority EXTI and TIM15 interrupts will continue firing the ultrasonic triac independently. This leads to continuous, uncontrolled heating and ultrasonic power delivery, risking dry-boiling, thermal runaway, and transducer damage.
REPRODUCTION / FAILURE SCENARIO: 
1. System enters SYS_MODE_RUNNING with heater ON. 
2. A strong EMI burst halts the MCU main loop but leaves peripherals and high-priority interrupts active. 
3. The heater and ultrasonic drivers continue running indefinitely because the `ProcessTimer_Process()` is no longer evaluated to stop them.
RECOMMENDED FIX: Enable IWDG in STM32CubeMX with a timeout of ~1-2 seconds. Call `HAL_IWDG_Refresh()` inside the main superloop.
VERIFICATION METHOD: Manually insert a `while(1);` delay in the main loop while running and verify the system automatically resets and cuts power to the heater and triac.
CONFIDENCE: HIGH

## 2. BENCH_DEV_MODE_ID hardcodes bus ID to 1, causing multi-drop collisions
ID: SEC-02
SEVERITY: CRITICAL
STATUS: CONFIRMED
CATEGORY: BUG
FILE: C:\Users\ern0e\EAGLEULTRASONiK\STM32\Ultrasonik_G4_Master\Core\Src\main.c
LINE: 53
FUNCTION: main
TITLE: BENCH_DEV_MODE_ID hardcodes bus ID to 1, causing multi-drop collisions
OBSERVED BEHAVIOR: `BENCH_DEV_MODE_ID` is set to 1. During boot, `MY_TANK_ID` is forced to this value, overriding Flash and DIP switch settings.
EXPECTED BEHAVIOR: `BENCH_DEV_MODE_ID` should be 0 in production to allow each slave to read its unique Tank ID from Flash or DIP switches.
EVIDENCE: Line 53 in `main.c` reads `#define BENCH_DEV_MODE_ID 1`. Lines 204-208 force `MY_TANK_ID = BENCH_DEV_MODE_ID;` when this macro is > 0.
ROOT CAUSE: A debug/bench macro was left enabled in the codebase.
IMPACT: If flashed to multiple production units on the same RS485/UART bus, all nodes will operate with Tank ID 1. They will simultaneously respond to `T1:` commands and concurrently transmit `STAT,...` telemetry every 500ms, causing severe bus contention, data corruption, and total loss of communication.
REPRODUCTION / FAILURE SCENARIO: Flash this firmware to two boards connected to the same UART bus. Both will continuously garble the TX line.
RECOMMENDED FIX: Change `#define BENCH_DEV_MODE_ID 1` to `#define BENCH_DEV_MODE_ID 0`.
VERIFICATION METHOD: Read `MY_TANK_ID` on multiple boards and ensure they reflect their physical DIP switch values.
CONFIDENCE: HIGH

## 3. Unrestricted T0: Broadcast Address enables unauthorized/accidental mass activation
ID: SEC-03
SEVERITY: HIGH
STATUS: CONFIRMED
CATEGORY: SAFETY RISK
FILE: C:\Users\ern0e\EAGLEULTRASONiK\STM32\Ultrasonik_G4_Master\Core\Src\esp32_uart.c
LINE: 100
FUNCTION: ProcessLine
TITLE: Unrestricted T0: Broadcast Address enables unauthorized/accidental mass activation
OBSERVED BEHAVIOR: The UART parser accepts any command prefixed with `T0:` and executes it on all connected slaves, including `START`, `SET_TEMP`, and `SET_POWER`.
EXPECTED BEHAVIOR: Broadcast commands should be restricted to harmless or specific configuration commands (like `SET_ID` or `STOP`). Dangerous state-changing commands like `START` should require the specific node ID.
EVIDENCE: Lines 98-103 allow `tank_id == 0` to bypass the `MY_TANK_ID` check, immediately passing the payload to the parser which processes `START` and `SET_TEMP` without further address validation.
ROOT CAUSE: Lack of command filtering for the broadcast address.
IMPACT: A malformed packet, logic bug in the ESP32, or malicious packet injection on the UART bus sending `T0:START` will simultaneously turn on the heaters and ultrasonic generators of all up to 10 baths on the bus. This could trip mains breakers due to massive inrush current, or cause a major safety incident.
REPRODUCTION / FAILURE SCENARIO: Inject `T0:START\nT0:SET_TEMP:90\n` onto the UART bus. All 10 connected baths will immediately start heating to 90C.
RECOMMENDED FIX: Inside `ProcessLine()`, explicitly reject `START`, `SET_TEMP`, `SET_POWER`, and `SET_TIME` if `tank_id == 0`. Only allow `SET_ID` and `STOP` for broadcast.
VERIFICATION METHOD: Send `T0:START` on the bus and verify no node transitions to `SYS_MODE_RUNNING`.
CONFIDENCE: HIGH

## 4. Lack of HMI communication timeout allows runaway processes upon link loss
ID: SEC-04
SEVERITY: HIGH
STATUS: CONFIRMED
CATEGORY: RELIABILITY RISK
FILE: C:\Users\ern0e\EAGLEULTRASONiK\STM32\Ultrasonik_G4_Master\Core\Src\esp32_uart.c
LINE: 64
FUNCTION: ESP32_UART_Process
TITLE: Lack of HMI communication timeout allows runaway processes upon link loss
OBSERVED BEHAVIOR: The STM32 firmware does not monitor the UART link for continuous liveness (heartbeat). Once started, it runs until the `process_timer` expires (up to 100 minutes) or an internal sensor fault occurs.
EXPECTED BEHAVIOR: The system should fail-safe and enter `SYS_MODE_IDLE` or `SYS_MODE_FAULT` if it loses communication with the HMI/ESP32 master for a sustained period (e.g., 5-10 seconds) while running.
EVIDENCE: No timeout logic exists in `esp32_uart.c` or `main.c` to reset the system state if `rx_byte` reception stops.
ROOT CAUSE: Omission of a fail-safe communication timeout mechanism.
IMPACT: If the ESP32 crashes, the HMI screen breaks, or the UART cable is severed while a bath is actively heating and cavitating, the operator loses all ability to control or stop the unit via the interface. The bath will continue running blindly for up to 100 minutes, posing an unattended operational hazard.
REPRODUCTION / FAILURE SCENARIO: Start a process with 100 minutes duration. Disconnect the RX/TX pins. Observe that the STM32 continues driving the triac and relay indefinitely.
RECOMMENDED FIX: Implement a timestamp variable updated on every valid parsed UART packet. In the main loop, check if `HAL_GetTick() - last_valid_rx_tick > 5000`. If true and mode is `SYS_MODE_RUNNING`, force mode to `SYS_MODE_IDLE` or set a communication fault.
VERIFICATION METHOD: Disconnect UART during operation and verify the system automatically shuts off after 5 seconds.
CONFIDENCE: HIGH

## 5. Over-temperature condition misclassified as open sensor, lacks explicit software thermal fuse
ID: SEC-05
SEVERITY: MEDIUM
STATUS: CONFIRMED
CATEGORY: SAFETY RISK
FILE: C:\Users\ern0e\EAGLEULTRASONiK\STM32\Ultrasonik_G4_Master\Core\Src\pt100_adc.c
LINE: 53
FUNCTION: PT100_ADC_Process
TITLE: Over-temperature condition misclassified as open sensor, lacks explicit software thermal fuse
OBSERVED BEHAVIOR: Readings exceeding `ADC_RAW_VALID_MAX` (110°C) trigger a `FAULT_PT100_OPEN`. There is no explicit logic to detect an over-temperature condition (e.g., >95°C) before it breaches the hardware validity limit.
EXPECTED BEHAVIOR: The software should explicitly detect and halt the system with a specific `FAULT_OVERTEMP` flag if the temperature exceeds a safe operational maximum (e.g., 90-95°C), before relying on the out-of-bounds ADC threshold.
EVIDENCE: `ADC_RAW_VALID_MAX` is calculated for 110°C in `pt100_adc.h`. If temperature hits 110°C, the ADC raw value exceeds this max, and line 60 sets `FAULT_PT100_OPEN`.
ROOT CAUSE: The PT100 validity window acts as a de facto high-limit cutoff, conflating a genuine sensor failure with an overheating event.
IMPACT: If a triac shorts or a relay welds, the bath will boil. The software will not intervene until it reaches 110°C, at which point it incorrectly reports a disconnected sensor. While it safely transitions to `SYS_MODE_FAULT`, the delayed response and mischaracterized error make diagnostics and safe shutdown less robust.
REPRODUCTION / FAILURE SCENARIO: Apply 115°C to the PT100. The system reports `FAULT_PT100_OPEN` instead of over-temperature.
RECOMMENDED FIX: Add a new macro `MAX_SAFE_TEMP_C` (e.g., 95.0f). Add a check: `if (g_system_state.current_temp_c >= MAX_SAFE_TEMP_C) { g_system_state.fault_flags |= FAULT_OVERTEMP; g_system_state.mode = SYS_MODE_FAULT; }`.
VERIFICATION METHOD: Inject a 100°C reading and verify `FAULT_OVERTEMP` triggers.
CONFIDENCE: HIGH

## 6. Unauthenticated UART protocol allows malicious packet injection
ID: SEC-06
SEVERITY: MEDIUM
STATUS: CONFIRMED
CATEGORY: SECURITY RISK
FILE: C:\Users\ern0e\EAGLEULTRASONiK\STM32\Ultrasonik_G4_Master\Core\Src\esp32_uart.c
LINE: 107
FUNCTION: ProcessLine
TITLE: Unauthenticated UART protocol allows malicious packet injection
OBSERVED BEHAVIOR: The UART interface accepts plaintext commands (e.g., `T1:START`) without any form of authentication, checksum, or sequence validation.
EXPECTED BEHAVIOR: A control bus managing high-power industrial equipment should employ basic integrity checks (like CRC8/CRC16) or authentication tokens to prevent spoofing.
EVIDENCE: `ProcessLine` processes incoming strings exactly as read, parsing commands with `strncmp`.
ROOT CAUSE: Simple ASCII protocol design.
IMPACT: An attacker with physical access to the UART/RS485 wires can inject commands to arbitrarily start processes, disable faults (via `STOP`), or change frequencies on the fly, leading to potential equipment damage. Even without malice, line noise could theoretically form a valid string.
REPRODUCTION / FAILURE SCENARIO: Connect a USB-to-UART bridge to the bus and send `T1:START\n`. The system executes the command immediately.
RECOMMENDED FIX: At minimum, implement a CRC checksum appended to the command (e.g., `T1:START:C5`).
VERIFICATION METHOD: Send an invalid CRC packet and observe it is rejected.
CONFIDENCE: HIGH

## 7. UART RX bytes are silently dropped when line_ready is set
ID: SEC-07
SEVERITY: LOW
STATUS: CONFIRMED
CATEGORY: BUG
FILE: C:\Users\ern0e\EAGLEULTRASONiK\STM32\Ultrasonik_G4_Master\Core\Src\esp32_uart.c
LINE: 272
FUNCTION: HAL_UART_RxCpltCallback
TITLE: UART RX bytes are silently dropped when line_ready is set
OBSERVED BEHAVIOR: When `line_ready` is 1, the RX interrupt drops all incoming bytes instead of buffering them in a circular buffer.
EXPECTED BEHAVIOR: A circular buffer or double buffer should be used so that if a new command arrives while the main loop is processing the previous one, it is not lost.
EVIDENCE: Lines 272-273 note "if line_ready is still set... incoming bytes are dropped". `HAL_UART_Receive_IT` is simply re-armed for `rx_byte` but it won't be saved.
ROOT CAUSE: Single fixed buffer (`rx_line`) and blocking flag (`line_ready`).
IMPACT: If the ESP32 sends two commands back-to-back (e.g., `T1:SET_POWER:100\nT1:START\n`), the second command will be partially or entirely dropped, causing erratic behavior and lost commands.
REPRODUCTION / FAILURE SCENARIO: Send two commands rapidly. The second one will not be parsed.
RECOMMENDED FIX: Implement a ring buffer for RX data.
VERIFICATION METHOD: Send rapid consecutive commands and verify all are executed.
CONFIDENCE: HIGH
