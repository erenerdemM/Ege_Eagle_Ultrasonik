---
name: nextion-hmi
description: Nextion HMI display instruction set, serial protocol parsing, dual-buffer UI state sync, and TFT asset integration.
---

# SKILL — NEXTION HMI DISPLAY INTEGRATION & UI PROTOCOL

## Purpose
Guide Nextion HMI screen communication, page transitions, dual-buffer state synchronizations, and button touch event handlers.

## When to Use (Trigger)
- Modifying HMI display screens (`EKRAN/`), serial parser in `esp32/ekran_kontrol/`, or HMI telemetry feedback.

## Required Context Files
1. `esp32/ekran_kontrol/ekran_kontrol.ino`
2. `EKRAN/` directory files (`arayuz.HMI`, `arayuz.tft`)
3. `.agents/rules/04-esp32.md`

## Procedure
1. Inspect serial command parser for Nextion 0xFF 0xFF 0xFF frame termination.
2. Update double-buffer state values (`power`, `temperature`, `timer`, `status_icon`) on page refresh.
3. Ensure Nextion UI updates occur on state change rather than continuous polling.
4. Execute `pytest test_hmi_mock.py` to verify HMI serial message parsing.

## Verification
- Run `pytest test_hmi_mock.py`.

## Exit Criteria
- Nextion HMI command parsing verified; UI state sync confirmed; mock tests pass.
