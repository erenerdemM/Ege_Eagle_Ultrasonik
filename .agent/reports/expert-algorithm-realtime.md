> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONiK Embedded Algorithm & Real-Time Audit Report

ID: 01
SEVERITY: CRITICAL
STATUS: CONFIRMED
FILE: STM32/Ultrasonik_G4_Master/Core/Src/ultrasonic_pwm.c
LINE: 16
FUNCTION: Global Macros
TITLE: Hardcoded 50Hz AC Half-Cycle Assumption Catastrophic for 60Hz Mains
OBSERVED BEHAVIOR: `AC_HALF_CYCLE_US` is hardcoded to 10000us (50Hz), and `TRIAC_MAX_DELAY_US` is set to 9500us.
EXPECTED BEHAVIOR: The AC line frequency should be measured dynamically, or selectable between 50/60Hz, dynamically scaling the maximum firing delay.
EVIDENCE: `#define AC_HALF_CYCLE_US 10000UL`, `#define TRIAC_MAX_DELAY_US (AC_HALF_CYCLE_US - 500UL)`
ROOT CAUSE: Hardcoded assumption of 50Hz mains power.
IMPACT: When plugged into 60Hz mains (half-cycle is 8333us), a command for low power sets the firing delay to 9500us. This exceeds the 8333us half-cycle, meaning the triac fires in the NEXT half-cycle of opposite polarity. This creates extreme DC injection, asymmetric waveforms, and will likely destroy the ultrasonic transducer and blow mains fuses.
REPRODUCTION / FAILURE SCENARIO: Operate the device on 60Hz power and set power to 5%. Firing occurs at 9.5ms after zero-cross, firing the triac well into the opposite mains phase.
RECOMMENDED FIX: Dynamically calculate `AC_HALF_CYCLE_US` by capturing a high-resolution timer in the `HAL_GPIO_EXTI_Callback` zero-cross interrupt to measure the actual interval.
VERIFICATION METHOD: Analytical calculation of phase angle delay versus 60Hz period.
CONFIDENCE: HIGH
CATEGORY: SAFETY RISK

---

ID: 02
SEVERITY: HIGH
STATUS: CONFIRMED
FILE: STM32/Ultrasonik_G4_Master/Core/Src/x9c103s.c
LINE: 93
FUNCTION: X9C103S_SetStep
TITLE: Extended Critical Sections and Software Loop Delays Block Real-Time Interrupts (BUG-HIGH-03)
OBSERVED BEHAVIOR: `__disable_irq()` is used around bit-banged delays in `X9C103S_Init` and `X9C103S_SetStep`. The function executes a software delay loop (`X9C_DelayUs`) for up to 100 steps.
EXPECTED BEHAVIOR: Interrupts should not be disabled for long periods (hundreds of microseconds). Delays should use non-blocking hardware timers or interrupts.
EVIDENCE: `__disable_irq()` is followed by a loop sending up to 100 pulses, with each pulse taking `2 * 3us = 6us`. Total delay exceeds 600us. The CPU runs at 170MHz, and `X9C_DelayUs` multiplies by 45 cycles which loosely tracks time but suffers from compiler optimizations changing the delay accuracy.
ROOT CAUSE: Using crude, blocking `volatile` loop delays inside a global IRQ lockout.
IMPACT: During the 600us+ blackout, zero-cross interrupts (`EXTI9_5_IRQn`) and triac firing timers (`TIM1_BRK_TIM15_IRQn`) will be blocked. This will cause missed triac firings or delayed firings (shifting the power phase), leading to dangerous asymmetrical transducer driving or dropped power.
REPRODUCTION / FAILURE SCENARIO: Issue `SET_FREQ:40` over UART while ultrasonic is running. The triac gate firing will jitter by up to 600us.
RECOMMENDED FIX: Use a hardware timer (e.g., TIM6) for microsecond delays. Do not disable global interrupts for the entire loop; only wrap atomic pin assignments if strictly necessary, or rewrite the driver to use a timer interrupt state machine.
VERIFICATION METHOD: Inspecting the loop bound and critical section span.
CONFIDENCE: HIGH
CATEGORY: BUG

---

ID: 03
SEVERITY: HIGH
STATUS: CONFIRMED
FILE: STM32/Ultrasonik_G4_Master/Core/Src/ultrasonic_pwm.c
LINE: 144
FUNCTION: UltrasonicPWM_Process
TITLE: Soft-Start Algorithm Executes at Unregulated Main Loop Speed
OBSERVED BEHAVIOR: `current_delay_us` is decremented by 20us (`SOFTSTART_RAMP_STEP_US`) every time `UltrasonicPWM_Process()` executes.
EXPECTED BEHAVIOR: The soft-start should increment power gradually relative to real time (e.g., adjusting once per AC cycle or on a fixed timer tick).
EVIDENCE: The decrement `current_delay_us -= SOFTSTART_RAMP_STEP_US;` occurs within the `while (1)` main superloop without any time-gating.
ROOT CAUSE: Main loop frequency dependency for a temporal algorithm.
IMPACT: Since the 170MHz MCU main loop executes thousands of times per millisecond, the entire soft-start ramp from 0 to 100% power occurs instantaneously. The transducer will suffer from massive inrush current, completely defeating the purpose of a soft-start.
REPRODUCTION / FAILURE SCENARIO: Send the `START` command. Observe the firing delay variable or triac gate on an oscilloscope. It jumps to maximum power almost instantly.
RECOMMENDED FIX: Move the soft-start decrement logic into the `HAL_GPIO_EXTI_Callback` so it executes exactly once per zero-cross, providing a predictable ramp of 20us per half-cycle (e.g., 450 cycles / 4.5 seconds to reach max power).
VERIFICATION METHOD: Code analysis of main loop flow vs temporal variables.
CONFIDENCE: HIGH
CATEGORY: BUG

---

ID: 04
SEVERITY: MEDIUM
STATUS: CONFIRMED
FILE: STM32/Ultrasonik_G4_Master/Core/Src/pt100_adc.c
LINE: 67
FUNCTION: PT100_ADC_Process
TITLE: Lack of Digital Filtering on PT100 ADC causes Relay Chatter (BUG-MED-01)
OBSERVED BEHAVIOR: The `current_temp_c` is derived from a single raw ADC conversion without any filtering or averaging.
EXPECTED BEHAVIOR: Analog inputs, especially in noisy high-power ultrasonic environments, must be low-pass filtered.
EVIDENCE: `adc_raw = HAL_ADC_GetValue(&hadc2);` followed by direct conversion `g_system_state.current_temp_c = ((float)adc_raw * PT100_CAL_SLOPE) + PT100_CAL_OFFSET;`.
ROOT CAUSE: Omitting filtering algorithms in software for a direct ADC read.
IMPACT: Electrical noise from the 28kHz/40kHz transducer switching will couple into the PT100 line. The temperature reading will fluctuate rapidly, which will cause the bang-bang heater relay (`heater_relay.c`) to chatter on and off, destroying the relay contacts.
REPRODUCTION / FAILURE SCENARIO: Turn on the ultrasonic transducer and observe the `temp_x10` output in the UART status telegram. Noise will cause large transient spikes.
RECOMMENDED FIX: Implement an Exponential Moving Average (EMA) (e.g., `filtered_adc = (filtered_adc * 15 + new_adc) / 16`) or a ring buffer median filter before converting to float temperature.
VERIFICATION METHOD: Source code inspection of ADC data flow.
CONFIDENCE: HIGH
CATEGORY: BUG

---

ID: 05
SEVERITY: LOW
STATUS: CONFIRMED
FILE: STM32/Ultrasonik_G4_Master/Core/Src/ultrasonic_pwm.c
LINE: 43
FUNCTION: PowerPctToDelayUs
TITLE: Linear Phase Delay Formula Does Not Yield Linear RMS Power
OBSERVED BEHAVIOR: The target firing delay is calculated using linear interpolation: `TRIAC_MAX_DELAY_US - ((span * power_pct) / 100u)`.
EXPECTED BEHAVIOR: Triac phase-angle control delivers true RMS power according to an integrated sine-squared function. To achieve linear 0-100% power, the delay mapping must be non-linear (inverse arccosine relation).
EVIDENCE: The `PowerPctToDelayUs` function applies strict linear algebra to the delay time span.
ROOT CAUSE: Simplistic mathematical modeling of AC phase control.
IMPACT: At 50% power setting, the actual ultrasonic power delivered will not be 50%. The power curve follows an S-shape, limiting the user's ability to precisely dial in cavitation intensity.
REPRODUCTION / FAILURE SCENARIO: Set power to 50% on the HMI. Measure AC true RMS power output to the transducers. It will deviate from 50% of maximum.
RECOMMENDED FIX: Implement a lookup table (LUT) that translates a 0-100% linear power request into the correct firing delay microsecond value that matches the AC power integral.
VERIFICATION METHOD: Mathematical verification of AC phase angle integrals.
CONFIDENCE: HIGH
CATEGORY: PERFORMANCE ISSUE

---

ID: 06
SEVERITY: LOW
STATUS: CONFIRMED
FILE: STM32/Ultrasonik_G4_Master/Core/Src/pt100_adc.c
LINE: 43
FUNCTION: PT100_ADC_Process
TITLE: Synchronous ADC Polling Blocks Main Superloop
OBSERVED BEHAVIOR: `HAL_ADC_PollForConversion(&hadc2, 10)` is used to wait for the ADC reading inside the main loop.
EXPECTED BEHAVIOR: ADC conversions should be interrupt-driven, use DMA, or at least be guaranteed to be extremely fast.
EVIDENCE: Calling `HAL_ADC_PollForConversion` blocks the main execution path. While ADC conversion on STM32G4 is fast (~1us), the use of blocking HAL calls in a high-frequency real-time superloop is an anti-pattern.
ROOT CAUSE: Using blocking synchronous peripheral API calls.
IMPACT: While currently low impact due to the fast native ADC speed, any hardware failure or misconfiguration of the ADC clock could result in a 10ms block (the timeout value), breaking UART and PWM soft-start processing.
REPRODUCTION / FAILURE SCENARIO: If the ADC gets stuck, the superloop stalls for 10ms every pass.
RECOMMENDED FIX: Configure the ADC to trigger via a hardware timer (e.g., TIM6) and use DMA or End-of-Conversion (EOC) interrupts to update the raw value asynchronously.
VERIFICATION METHOD: Code analysis.
CONFIDENCE: HIGH
CATEGORY: CODE QUALITY
