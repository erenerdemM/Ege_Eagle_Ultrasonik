# RULE 01 — SOURCE OF TRUTH (HARDWARE INTEGRITY)

## Purpose
Protect physical hardware wiring, pinouts, timer assignments, OPAMP channels, jumpers, and UART mappings defined in authoritative documents.

## Scope
Applies to `hardware_wiring_FINAL_AUTHORITY.md`, `hardware_wiring_final_audit.md`, `hardware_wiring_final_physical_package.md`, and any firmware file modifying GPIO pins or hardware peripherals.

## MUST
1. Treat `hardware_wiring_FINAL_AUTHORITY.md` as the IMMUTABLE SINGLE SOURCE OF TRUTH.
2. Verify pin mappings (e.g., STM32 PB10/PB11 USART3, PA2/PA3 LPUART1, OPAMP3 PA1/PA2/PB0) against `hardware_wiring_FINAL_AUTHORITY.md` before changing STM32 `.ioc` or peripheral driver code.
3. Compare hardware jumpers, relay connections, and PT100 wiring against the physical package specification when diagnosing hardware communication or sensor bugs.

## MUST NOT
1. DO NOT edit, auto-correct, or modify `hardware_wiring_FINAL_AUTHORITY.md` without explicit human authorization.
2. DO NOT modify firmware GPIO or peripheral pin assignments to match broken code; code MUST conform to the physical hardware authority.
3. DO NOT assume or guess physical wiring facts when discrepancies arise between source code and hardware documentation.

## Verification
- Inspect pin definitions against `hardware_wiring_FINAL_AUTHORITY.md` using `hardware-engineer` agent.

## Escalation
If a code change or bugfix reveals a genuine conflict with physical hardware wiring:
1. **HALT IMMEDIATELY.**
2. **DO NOT WRITE CODE.**
3. **GENERATE HARDWARE CONFLICT REPORT** specifying file name, line numbers, and pin mismatch.
4. **REQUEST HUMAN DECISION** before taking any further action.
