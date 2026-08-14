# RULE 02 — SCOPE CONTROL & BOUNDARY ENFORCEMENT

## Purpose
Enforce strict file writing boundaries for each specialized agent role to prevent unintended side effects across subsystem boundaries.

## Scope
Applies to all subagents invoked during task execution.

## MUST
1. Require agents to operate strictly within their assigned folder scopes:
   - `stm32-specialist`: Write permitted ONLY within `STM32/`.
   - `esp32-hmi-specialist`: Write permitted ONLY within `esp32/` and `EKRAN/`.
   - `communication-specialist`: Write permitted ONLY in UART/RS485 communication module files.
   - `qa-test-engineer`: Write permitted ONLY in `test_*.py`.
   - `system-architect`: Write permitted ONLY in Markdown documentation files (`*.md`).
   - `hardware-engineer` & `code-reviewer`: READ-ONLY across entire repository.
2. Verify affected file list prior to invoking any write or edit tool.

## MUST NOT
1. DO NOT allow subagents to edit files outside their declared tool scope.
2. DO NOT modify test scripts (`test_*.py`) when implementing firmware features (firmware implementation belongs to specialists; test suite management belongs to `qa-test-engineer`).
3. DO NOT touch historical audit archives (`.agent/reports/`, `.agent/findings/`).

## Verification
- Pre-write scope verification check before calling `replace_file_content` or `write_to_file`.

## Escalation
If a task requires cross-subsystem edits (e.g., both STM32 and ESP32), Master Agent MUST route the task through `system-architect` and invoke each specialist separately within their respective scopes.
