# RULE 06 — TESTING, HIL & REGRESSION STANDARDS

## Purpose
Ensure all code edits pass rigorous automated test suites, HIL UART integration checks, and mock display tests without regression.

## Scope
Applies to test files (`test_hil_uart.py`, `test_hmi_mock.py`, `test_rs485_mock.py`) and test execution policies.

## MUST
1. Follow **pytest** standards for all Python test scripts (`test_*.py`).
2. Run relevant unit and mock test scripts after completing firmware edits:
   - For UART/RS485 changes: `pytest test_hil_uart.py test_rs485_mock.py`
   - For HMI UI changes: `pytest test_hmi_mock.py`
3. Ensure 100% pass rate on all pre-existing tests before declaring a task completed.

## MUST NOT
1. DO NOT comment out, disable, or delete failing test assertions to mask code bugs.
2. DO NOT fabricate mock test results; test commands MUST actually run via `run_command` tool.
3. DO NOT modify test scripts (`test_*.py`) to fit broken firmware logic (firmware MUST fit the test specification).

## Verification
- Clean test log output with 0 failures (`pytest` exit code 0).

## Escalation
If a test failure reveals a fundamental flaw in the hardware specification or protocol baseline, escalate to `system-architect` and `qa-test-engineer`.
