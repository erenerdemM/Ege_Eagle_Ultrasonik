> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONiK STM32 Firmware Audit Report
**Date:** 2026-08-10
**Target:** `STM32/Ultrasonik_G4_Master/`
**Status:** Audit Completed

## 1. Verification of Previous Findings

ID: BUG-CRIT-01
SEVERITY: CRITICAL
STATUS: CONFIRMED
FILE: STM32/Ultrasonik_G4_Master/Core/Src/main.c
LINE: 53
FUNCTION: main
TITLE: BENCH_DEV_MODE_ID macro is set to 1 in code
OBSERVED BEHAVIOR: `#define BENCH_DEV_MODE_ID 1` is hardcoded, which overrides `MY_TANK_ID` to 1.
EXPECTED BEHAVIOR: `BENCH_DEV_MODE_ID` should be 0 for production builds so the ID is read from Flash or DIP switches.
EVIDENCE: `main.c`, line 53: `#define BENCH_DEV_MODE_ID 1 // Set to 0 to disable dev mode`
ROOT CAUSE: Bench development flag was not cleared to 0 prior to commit.
IMPACT: All STM32 boards deployed in field will boot as Tank ID 1, causing immediate bus collision and collapse of multi-drop addressing.
REPRODUCTION / FAILURE SCENARIO: Connect 2 STM32 slave boards to shared UART bus. Both will transmit STAT,1 telemetry.
RECOMMENDED FIX: Set `#define BENCH_DEV_MODE_ID 0`.
VERIFICATION METHOD: Verify DIP switch reading correctly reflects on bus telemetry after setting to 0.
CONFIDENCE: HIGH
CATEGORY: BUG

ID: BUG-HIGH-01
SEVERITY: HIGH
STATUS: CONFIRMED
FILE: STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c
LINE: 250
FUNCTION: HAL_UART_RxCpltCallback
TITLE: Incoming UART commands dropped when line_ready flag is set
OBSERVED BEHAVIOR: The UART rx ISR drops bytes if `line_ready` is set, meaning the main loop hasn't yet processed the single `rx_line` buffer.
EXPECTED BEHAVIOR: UART should have a circular FIFO queue for incoming lines.
EVIDENCE: `esp32_uart.c`, line 250: `if (!line_ready) { ... } /* else incoming bytes are dropped */`
ROOT CAUSE: Lack of circular FIFO queue for incoming UART messages.
IMPACT: Rapidly sent commands from ESP32 master can be silently dropped.
REPRODUCTION / FAILURE SCENARIO: Send 3 commands from ESP32 within 5ms interval; observe dropped commands.
RECOMMENDED FIX: Implement a ring buffer (FIFO) for UART RX lines.
VERIFICATION METHOD: Verify burst transmission of 5 commands in HIL test.
CONFIDENCE: HIGH
CATEGORY: RELIABILITY RISK

ID: BUG-HIGH-03
SEVERITY: HIGH
STATUS: CONFIRMED
FILE: STM32/Ultrasonik_G4_Master/Core/Src/x9c103s.c
LINE: 18
FUNCTION: X9C_DelayUs
TITLE: Uncalibrated software NOP loop for microsecond timing
OBSERVED BEHAVIOR: Microsecond delays for potentiometer bit-banging are implemented using a fixed count `for` loop executing an empty asm statement.
EXPECTED BEHAVIOR: Time delays should rely on hardware timers or DWT cycle counters.
EVIDENCE: `x9c103s.c`, line 21: `for (volatile uint32_t i = 0U; i < count; i++) { __asm__ volatile(""); }`
ROOT CAUSE: Using CPU loop multiplication rather than hardware timer or DWT cycle counter.
IMPACT: Delay durations vary with compiler optimization flags, risking missed wiper steps on digital pot.
REPRODUCTION / FAILURE SCENARIO: Compile with -O3 and measure INC pin pulse widths on oscilloscope.
RECOMMENDED FIX: Use DWT->CYCCNT for microsecond delay timing.
VERIFICATION METHOD: Measure pulse width on oscilloscope across optimization levels.
CONFIDENCE: HIGH
CATEGORY: RELIABILITY RISK

ID: BUG-MED-01
SEVERITY: MEDIUM
STATUS: CONFIRMED
FILE: STM32/Ultrasonik_G4_Master/Core/Src/pt100_adc.c
LINE: 34
FUNCTION: PT100_ADC_Process
TITLE: Lack of digital filtering on raw ADC temperature samples
OBSERVED BEHAVIOR: ADC values are read once and immediately used to calculate temperature without filtering.
EXPECTED BEHAVIOR: Temperature readings should be smoothed using a moving average or median filter.
EVIDENCE: `adc_raw = HAL_ADC_GetValue(&hadc2);` is directly evaluated and mapped to `g_system_state.current_temp_c`.
ROOT CAUSE: Missing software noise filter layer.
IMPACT: Electrical noise from triac or power supplies can cause temperature spikes and spurious fault trips.
REPRODUCTION / FAILURE SCENARIO: Inject noise into ADC input and observe current_temp_c fluctuations.
RECOMMENDED FIX: Implement an 8-sample moving average filter.
VERIFICATION METHOD: Verify stable temperature readings under noisy conditions.
CONFIDENCE: HIGH
CATEGORY: SAFETY RISK

ID: BUG-MED-02
SEVERITY: MEDIUM
STATUS: CONFIRMED
FILE: STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c
LINE: 80
FUNCTION: ProcessLine
TITLE: Missing CRC or Checksum in ASCII UART protocol
OBSERVED BEHAVIOR: String parsing relies on pure ASCII format with no checksum validation byte.
EXPECTED BEHAVIOR: Telemetry and commands should include a checksum (like CRC8 or XOR checksum) to reject corrupted lines.
EVIDENCE: The whole of `ProcessLine()` checks only the T<id>: prefix and commands using `strncmp()`.
ROOT CAUSE: Protocol designed as plain text ASCII for simplicity.
IMPACT: Line noise corrupted bytes could be parsed as valid erroneous commands.
REPRODUCTION / FAILURE SCENARIO: Inject bit errors into UART line and check if bad commands execute.
RECOMMENDED FIX: Append 2-character Hex XOR checksum to frames.
VERIFICATION METHOD: Verify corrupted frames are rejected.
CONFIDENCE: HIGH
CATEGORY: RELIABILITY RISK

ID: BUG-LOW-01
SEVERITY: LOW
STATUS: CONFIRMED
FILE: STM32/Ultrasonik_G4_Master/Core/Src/pt100_adc.c
LINE: 43
FUNCTION: PT100_ADC_Process
TITLE: Blocking ADC polling call in superloop
OBSERVED BEHAVIOR: The ADC process function polls for conversion completion with a 10ms timeout.
EXPECTED BEHAVIOR: ADC reads should be non-blocking using Interrupts (IT) or DMA.
EVIDENCE: `HAL_ADC_PollForConversion(&hadc2, 10)` in the main loop path.
ROOT CAUSE: Polled ADC design instead of interrupt or DMA driven.
IMPACT: Adds unnecessary microseconds delay to main loop iteration. If ADC faults, it blocks 10ms per iteration.
REPRODUCTION / FAILURE SCENARIO: Measure superloop cycle time during ADC hardware fault.
RECOMMENDED FIX: Convert ADC reading to interrupt driven or timer triggered.
VERIFICATION METHOD: Measure superloop cycle time.
CONFIDENCE: HIGH
CATEGORY: PERFORMANCE ISSUE

## 2. New Firmware Audit Findings

ID: BUG-NEW-01
SEVERITY: HIGH
STATUS: CONFIRMED
FILE: STM32/Ultrasonik_G4_Master/Core/Src/x9c103s.c
LINE: 94
FUNCTION: X9C103S_SetStep
TITLE: Critical System Interrupts Disabled for Extended Durations During Potentiometer Updates
OBSERVED BEHAVIOR: `__disable_irq()` is called before bit-banging the INC/CS pins. This global interrupt mask is held across software loops of `X9C_DelayUs`, lasting up to 600 microseconds.
EXPECTED BEHAVIOR: Software bit-banging delays should not block all interrupts. Hardware timers or SPI should be used, or at minimum only lower-priority IRQs should be masked if absolutely needed.
EVIDENCE: `__disable_irq(); ... for(...) { ... X9C_DelayUs(3U); ... } ... __set_PRIMASK(primask);`
ROOT CAUSE: Fear of zero-cross or timer interrupt preemption disrupting the bit-bang timing led to heavy-handed `__disable_irq()` usage.
IMPACT: High-priority interrupts like `USART3` (ESP32 command reception) and `EXTI9_5` (Mains zero-cross) are delayed or missed. A 600us UART blackout easily causes RX overrun on STM32G4 UART. A delayed zero-cross shifts the triac firing angle, causing instantaneous power spikes.
REPRODUCTION / FAILURE SCENARIO: Send `SET_FREQ:40` to trigger a 50-step wiper change. Simultaneously blast a `SET_TEMP:50` command on UART and monitor the triac output. UART will drop the temp command, and the current AC half-cycle will stutter.
RECOMMENDED FIX: Use a hardware timer to generate the `INC` pulses via PWM, or convert the bit-bang to a state machine triggered by a timer interrupt, eliminating `__disable_irq()` completely.
VERIFICATION METHOD: Toggle a debug GPIO around `__disable_irq()` and capture the pulse width on an oscilloscope to prove maximum lockout duration.
CONFIDENCE: HIGH
CATEGORY: RELIABILITY RISK
