---
name: esp32-freertos
description: ESP32-S3 FreeRTOS multi-threading, task prioritization, queue management, NVS persistent recipe storage, and watchdog handling.
---

# SKILL — ESP32 FREERTOS & EMBEDDED C++ DEVELOPMENT

## Purpose
Guide modifications to ESP32-S3 FreeRTOS tasks, queue management, NVS recipe saves, and slave connection watchdog routines.

## When to Use (Trigger)
- Any task modifying files in `esp32/` directory.
- Adding FreeRTOS tasks, tuning queue depths, adjusting NVS recipe parameters, or zero-cross timer logic.

## Required Context Files
1. `esp32/ekran_kontrol/ekran_kontrol.ino`
2. `.agents/rules/04-esp32.md`

## Procedure
1. Inspect FreeRTOS task definitions and queue setup using `view_file`.
2. Ensure task priority matrix balances Nextion HMI RX parsing and RS485 slave telemetry handling.
3. Protect shared resources with FreeRTOS mutexes.
4. Verify non-volatile NVS flash key length (<=15 chars) and payload limits.
5. Apply non-blocking `vTaskDelay()` in task loops.

## Verification
- Run `pytest test_hmi_mock.py` and `pytest test_rs485_mock.py`.

## Exit Criteria
- FreeRTOS thread safety verified; NVS bounds validated; mock test suites green.
