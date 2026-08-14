---
name: hardware-validation
description: Physical pinout verification, hardware authority cross-checking, jumper configuration check, and conflict detection procedure.
---

# SKILL — HARDWARE VALIDATION & PHYSICAL PINOUT AUDIT

## Purpose
Cross-check physical pinouts, OPAMP channels, timers, jumpers, and relay wiring against the immutable single source of truth (`hardware_wiring_FINAL_AUTHORITY.md`).

## When to Use (Trigger)
- Any task altering GPIO pins, timer peripheral mappings, OPAMP channels, ADC pins, or hardware jumpers.
- Whenever a hardware pin discrepancy is suspected.

## Required Context Files
1. `hardware_wiring_FINAL_AUTHORITY.md` (IMMUTABLE SINGLE SOURCE OF TRUTH)
2. Target HAL setup file (e.g. `STM32/Ultrasonik_G4_Master/Core/Src/main.c`, `main.h`)
3. `.agents/rules/01-source-of-truth.md`

## Procedure
1. Read relevant pinout tables in `hardware_wiring_FINAL_AUTHORITY.md` using `view_file`.
2. Inspect target code pin definitions (`GPIO_PIN_x`, `GPIOx`).
3. Compare physical pin names (e.g. PB10 USART3_TX, PB11 USART3_RX, PA2 LPUART1_TX, PA3 LPUART1_RX).
4. If code matches hardware authority → Proceed with task.
5. If code conflicts with hardware authority → **HALT IMMEDIATELY. DO NOT EDIT CODE. GENERATE CONFLICT REPORT & ALERT HUMAN.**

## Verification
- Hardware Engineer check confirmation.

## Exit Criteria
- Zero pin conflicts detected OR conflict report escalated to human gate.
