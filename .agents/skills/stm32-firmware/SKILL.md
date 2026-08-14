---
name: stm32-firmware
description: STM32G474RE HAL peripheral configuration, TIM15 PWM phase control, OPAMP3 PT100 ADC signal processing, and MISRA C development procedures.
---

# SKILL — STM32 FIRMWARE DEVELOPMENT & HARDWARE HAL

## Purpose
Guide modifications to STM32G474RE HAL firmware drivers, PWM generation, PT100 ADC window filtering, and relay timing logic.

## When to Use (Trigger)
- Any task involving `STM32/` directory files.
- Adjusting ultrasonic PWM soft-start, triac phase control, PT100 ADC calculations, or UART ISRs on STM32.

## Required Context Files
1. `STM32/Ultrasonik_G4_Master/Core/Inc/main.h`
2. Target C module in `STM32/Ultrasonik_G4_Master/Core/Src/` (e.g. `ultrasonic_pwm.c`, `pt100_adc.c`, `esp32_uart.c`, `heater_relay.c`)
3. `.agents/rules/03-stm32.md`

## Procedure
1. Inspect target `.h` and `.c` files using `view_file` with precise line range boundaries.
2. Verify pin assignments against `hardware_wiring_FINAL_AUTHORITY.md` if changing GPIO or peripheral clocks.
3. Write MISRA C compliant code using standard STM32 HAL library calls (`HAL_TIM_`, `HAL_ADC_`, `HAL_UART_`).
4. Ensure duty cycle clamping (0-100%) and PT100 moving average filtering.
5. Use `replace_file_content` to apply changes.

## Verification
- Code must compile cleanly without pointer or ISR warnings.
- Run `pytest test_hil_uart.py` to verify telemetry frames.

## Exit Criteria
- Code edited; HAL APIs verified; test suite passed.
