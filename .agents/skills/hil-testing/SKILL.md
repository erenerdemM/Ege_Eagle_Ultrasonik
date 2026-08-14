---
name: hil-testing
description: Execution guide for HIL pytest integration suites (test_hil_uart.py), Nextion mock display tests, and RS485 bus collision tests.
---

# SKILL — HIL TESTING & AUTOMATED REGRESSION SUITES

## Purpose
Execute automated HIL integration tests and mock display/bus test suites using `pytest` to guarantee zero software regression.

## When to Use (Trigger)
- Executing NORMAL, LARGE, or CRITICAL task verification.
- Validating UART packet telemetry, Nextion UI serial parsing, or multi-drop bus timing.

## Required Context Files
1. `test_hil_uart.py`
2. `test_hmi_mock.py`
3. `test_rs485_mock.py`
4. `.agents/rules/06-testing.md`

## Procedure
1. Determine appropriate test suite based on modified component:
   - For UART/RS485: `pytest test_hil_uart.py test_rs485_mock.py -v`
   - For HMI: `pytest test_hmi_mock.py -v`
   - Full regression: `pytest test_hil_uart.py test_hmi_mock.py test_rs485_mock.py -v`
2. Run command via `run_command` tool.
3. Inspect output logs for test execution status and assertions.
4. Verify 100% test pass rate (exit code 0).

## Verification
- Pytest output log showing `PASSED` for all test cases.

## Exit Criteria
- Test suites executed; zero failures reported; regression suite green.
