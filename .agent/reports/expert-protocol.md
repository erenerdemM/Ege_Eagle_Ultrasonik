> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# Communication Protocol Audit Report
**EAGLEULTRASONiK - UART Communication between STM32 and ESP32**

## 1. ESP32 UART RX Buffer Overflow (OOM)
ID: PROTO-001
SEVERITY: HIGH
STATUS: CONFIRMED
FILE: esp32/ekran_kontrol/ekran_kontrol.ino
LINE: 602
FUNCTION: hatOku
TITLE: Unbounded String buffer in ESP32 UART receive loop
OBSERVED BEHAVIOR: The `tampon += c;` statement appends received characters indefinitely if no `\n` is encountered.
EXPECTED BEHAVIOR: The receive buffer should have a maximum length limit to prevent heap exhaustion.
EVIDENCE: `if (c != 0xFF && c != '\r') tampon += c;` with no length check on the `tampon` String.
ROOT CAUSE: Lack of length limit on string concatenation in the serial read function.
IMPACT: If the STM32 or environmental EMI generates a continuous stream of characters without a newline, the ESP32 will consume all heap memory and crash (WDT reset or allocation failure).
REPRODUCTION / FAILURE SCENARIO: Send a stream of 100KB of text without `\n` to the ESP32 UART1 RX pin.
RECOMMENDED FIX: Add a maximum length check (e.g., `if (tampon.length() < 128) tampon += c; else tampon = "";`).
VERIFICATION METHOD: Code review.
CONFIDENCE: HIGH
CATEGORIZATION: RELIABILITY RISK

## 2. STM32 UART RX Buffer Wrap Vulnerability
ID: PROTO-002
SEVERITY: MEDIUM
STATUS: CONFIRMED
FILE: STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c
LINE: 268
FUNCTION: HAL_UART_RxCpltCallback
TITLE: RX buffer overflow reset splits long commands
OBSERVED BEHAVIOR: If `rx_index >= RX_LINE_MAX - 1`, `rx_index` is reset to 0, and subsequent bytes are written to the start of the buffer. If a `\n` arrives later, the suffix of the long line is processed as a complete command.
EXPECTED BEHAVIOR: Excess bytes should be discarded until a `\n` is received, without capturing the suffix as a new line.
EVIDENCE:
```c
    else
    {
      /* line too long, discard it and resync on the next '\n' */
      rx_index = 0;
    }
```
ROOT CAUSE: Resetting `rx_index` to 0 immediately starts recording the next bytes as a valid line.
IMPACT: The suffix of a long, noisy string might accidentally form a valid command if the noise happens to spell out a valid command prefix.
REPRODUCTION / FAILURE SCENARIO: Send 64 'X' characters followed by "T1:START\n". `rx_index` resets after 63 'X's. The remaining "XT1:START\n" is captured. While this specific one won't parse due to 'X', a suffix that happens to begin with 'T' could parse as a valid command.
RECOMMENDED FIX: Introduce a state flag (e.g., `ignore_until_nl`) that gets set when the buffer overflows and clears when `\n` is received.
VERIFICATION METHOD: Code review.
CONFIDENCE: HIGH
CATEGORIZATION: BUG

## 3. Lack of Data Integrity Check (CRC/Checksum)
ID: PROTO-003
SEVERITY: MEDIUM
STATUS: CONFIRMED
FILE: STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c
LINE: N/A
FUNCTION: ProcessLine
TITLE: Absence of CRC/Checksum in UART frames
OBSERVED BEHAVIOR: UART frames (both commands and telemetry) are plain ASCII strings without any CRC or checksum.
EXPECTED BEHAVIOR: In industrial environments, UART communication should include a CRC/checksum to detect bit flips caused by EMI.
EVIDENCE: `esp32_uart.c` parses `T<id>:<cmd>:<val>`. `ekran_kontrol.ino` sends and parses raw ASCII. The Python test suite `test_hil_uart.py` confirms no checksum is used.
ROOT CAUSE: Protocol design choice. The protocol uses human-readable ASCII with no cryptographic or arithmetic integrity mechanism.
IMPACT: EMI/noise could flip bits in the ASCII payload, changing a value like `SET_POWER:10` to `SET_POWER:90` (if numeric chars are formed), leading to unintended and potentially hazardous operation parameters.
REPRODUCTION / FAILURE SCENARIO: EMI causes the UART RX line to toggle during transmission, flipping a bit in the ASCII payload.
RECOMMENDED FIX: Add a simple CRC8 or LRC to the end of frames, e.g., `T1:SET_POWER:50*<CRC>\n`.
VERIFICATION METHOD: Protocol analysis.
CONFIDENCE: HIGH
CATEGORIZATION: RELIABILITY RISK

## 4. Lack of Explicit ACK/NACK for Commands
ID: PROTO-004
SEVERITY: LOW
STATUS: CONFIRMED
FILE: esp32/ekran_kontrol/ekran_kontrol.ino
LINE: 170
FUNCTION: stmGonder
TITLE: Open-loop command transmission without explicit acknowledgment
OBSERVED BEHAVIOR: The ESP32 sends commands (e.g., `stmSetPower`) and assumes they are received. It does not wait for an ACK or NACK response.
EXPECTED BEHAVIOR: The master (ESP32) should expect an ACK from the STM32 to confirm receipt and application of the command, and retry if not received.
EVIDENCE: `stmGonder()` sends data via `Serial1.print()`. There is no response parsing for ACK. The ESP32 relies on periodic `STAT` telemetry to verify state, but commands themselves are fire-and-forget.
ROOT CAUSE: Open-loop protocol design.
IMPACT: If a command is lost due to noise or a UART error, the ESP32 state and STM32 state might momentarily desync until the ESP32 UI receives the next `STAT` telemetry and the user realizes the setting didn't stick.
REPRODUCTION / FAILURE SCENARIO: Send `SET_POWER:100` during a noise spike. The STM32 drops the frame. The ESP32 UI might briefly show 100 but the machine stays at the previous power level.
RECOMMENDED FIX: The STM32 should reply with `ACK:<cmd>` or `NACK:<cmd>`. The ESP32 should implement a retry queue for unacknowledged commands.
VERIFICATION METHOD: Protocol analysis.
CONFIDENCE: HIGH
CATEGORIZATION: DESIGN LIMITATION

## 5. Weak Telemetry Parsing in ESP32
ID: PROTO-005
SEVERITY: LOW
STATUS: CONFIRMED
FILE: esp32/ekran_kontrol/ekran_kontrol.ino
LINE: 248
FUNCTION: stmTelemetryIsle
TITLE: Telemetry parsing uses substring.toInt() which fails silently on malformed data
OBSERVED BEHAVIOR: `int tank_id = rest.substring(0, p1).toInt();` is used to parse fields. If the string contains non-numeric characters due to noise, `toInt()` returns 0.
EXPECTED BEHAVIOR: Malformed telemetry frames with non-numeric data in numeric fields should be completely rejected.
EVIDENCE: `String::toInt()` is used for `tank_id`, `rem_sec`, `temp_x10`, `relay`, `pwr`, `freq`, `fault`. If `pwr` is corrupted to `ab`, `pwr` becomes 0.
ROOT CAUSE: Relying on `toInt()` without validating if the substring only contains valid digit characters.
IMPACT: Corrupted telemetry might be displayed as 0 (e.g., 0.0 °C, 0% power) on the UI instead of being discarded, potentially causing user confusion.
REPRODUCTION / FAILURE SCENARIO: Inject `STAT,1,RUNNING,50,XX,1,50,28,0`. `temp_x10` becomes 0. The UI displays 0.0 °C.
RECOMMENDED FIX: Validate that the substring contains valid numeric characters before converting, or use a more robust parsing mechanism.
VERIFICATION METHOD: Code review.
CONFIDENCE: HIGH
CATEGORIZATION: CODE QUALITY
