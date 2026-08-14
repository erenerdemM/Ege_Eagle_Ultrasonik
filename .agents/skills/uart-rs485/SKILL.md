---
name: uart-rs485
description: RS485 multi-drop ASCII UART protocol framing, address parsing, telemetry matrix generation, and checksum validation.
---

# SKILL — RS485 MULTI-DROP UART PROTOCOL IMPLEMENTATION

## Purpose
Guide modifications to RS485 ASCII command packet parsing, telemetry generation, multi-drop addressing, and clamping rules.

## When to Use (Trigger)
- Changing multi-drop ASCII protocols, adding telemetry fields, or fixing UART framing bugs.

## Required Context Files
1. `STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c`
2. `STM32/Ultrasonik_G4_Master/Core/Inc/esp32_uart.h`
3. `esp32/ekran_kontrol/ekran_kontrol.ino`
4. `.agents/rules/05-communication.md`

## Procedure
1. Verify command target address format `T<ID>:<COMMAND>` (e.g. `T1:SET_PWR=75`).
2. Verify telemetry output string formatting:
   `STAT,ID=<id>,PWR=<pwr>,TEMP=<temp>,SET_TEMP=<stemp>,TIMER=<time>,RELAY=<r>,ERR=<err>`
3. Enforce value bounds (PWR 0-100%, TEMP 0-100°C, TIMER 0-999m).
4. Apply CRLF (`\r\n`) line termination check and 64-byte frame length buffer cap.

## Verification
- Run `pytest test_hil_uart.py test_rs485_mock.py` using `run_command`.

## Exit Criteria
- ASCII framing matches protocol matrix; multi-drop address collision avoided; tests pass.
