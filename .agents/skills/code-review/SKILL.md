---
name: code-review
description: Code quality auditor for MISRA C compliance, FreeRTOS safety, pointer alignment, boundary checks, and git diff security audits.
---

# SKILL — CODE REVIEW & SAFETY COMPLIANCE AUDIT

## Purpose
Perform thorough read-only code review checking MISRA C rules, FreeRTOS safety, defensive pointer checks, git diff integrity, and safety-critical compliance.

## When to Use (Trigger)
- Final stage of LARGE or CRITICAL tasks before human approval gate.
- Audit code changes before committing.

## Required Context Files
1. `.agents/rules/00-global-engineering.md`
2. `.agents/rules/03-stm32.md`
3. `.agents/rules/04-esp32.md`
4. Git diff output (`git diff`)

## Procedure
1. Inspect git diff using `run_command` (`git diff`).
2. Audit modified lines against MISRA C guidelines (explicit types, no dynamic memory in ISRs, proper NULL pointer checks).
3. Verify FreeRTOS thread safety (mutexes on shared queues/buffers).
4. Verify non-blocking timer loops (`HAL_GetTick()`, `vTaskDelay()`).
5. Confirm no unrelated files were touched.

## Verification
- Code Reviewer report detailing zero safety violations.

## Exit Criteria
- Diff audit clean; MISRA C / FreeRTOS compliance verified; ready for Human Gate / Merge.
