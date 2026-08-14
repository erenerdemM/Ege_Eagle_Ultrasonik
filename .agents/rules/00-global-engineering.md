# RULE 00 — GLOBAL ENGINEERING STANDARDS

## Purpose
Ensure all C, C++, and Python codebase modifications adhere to deterministic execution, strict memory safety, non-blocking design, and zero dynamic memory leaks.

## Scope
Applies to all source code files across `STM32/`, `esp32/`, `EKRAN/`, and `test_*.py`.

## MUST
1. Use defensive NULL checking and explicit array bounds checks before dereferencing pointers or accessing buffer indices.
2. Maintain non-blocking state loops. All hardware operations must use timeouts or non-blocking timer checks.
3. Keep line-level documentation accurate and preserve pre-existing comments.
4. Ensure deterministic variable initializations for all C structs and C++ class members.

## MUST NOT
1. DO NOT perform dynamic memory allocation (`malloc`, `free`, `new`, `delete`) inside Interrupt Service Routines (ISRs) or high-frequency control loops.
2. DO NOT use blocking infinite loops (`while(1)` without timeout check or hardware watchdog refresh).
3. DO NOT swallow exceptions or return dummy fallback values during runtime failures.
4. DO NOT create hidden global mutable variables without explicit atomic or ISR mutex protection.

## Verification
- Code must compile cleanly without memory or pointer alignment warnings.
- Run `code-reviewer` agent or diff check prior to merging changes.

## Escalation
If a requested logic change requires breaking thread safety or introducing dynamic memory in ISRs, HALT execution and request architectural escalation to `system-architect`.
