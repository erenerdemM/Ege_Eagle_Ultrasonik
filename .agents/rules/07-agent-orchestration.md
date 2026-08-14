# RULE 07 — AGENT ORCHESTRATION, TASK ROUTING & HANDOFF POLICY

## Purpose
Enforce the Minimum Sufficient Agent Principle, 5-tier Task Complexity Routing (TINY, SMALL, NORMAL, LARGE, CRITICAL), cross-agent handoff protocols, and failure recovery policies.

## Scope
Applies to Master Agent (`Antigravity`) task dispatch, subagent execution workflows, and multi-agent handoffs.

## 1. TASK ROUTING MATRIX

1. **TINY** (Typo, comment, 1-line doc fix):
   - **Route:** Master Agent direct edit (`replace_file_content`).
   - **Subagents:** 0 (No subagent overhead).
   - **Context:** Level 0 (User Task) + Target file line view.

2. **SMALL** (Single function bugfix, isolated driver update):
   - **Route:** Master → 1 Specialist (`stm32-specialist` OR `esp32-hmi-specialist`).
   - **Subagents:** 1 Specialist.
   - **Context:** Level 1 (`PROJECT_STATE.md`) + Level 3 (Relevant Skill) + Target C/C++ file.

3. **NORMAL** (UART/RS485 behavior change, ADC filtering update, relay logic tweak):
   - **Route:** Master → Specialist → `qa-test-engineer`.
   - **Subagents:** 2 Subagents sequentially.
   - **Context:** Level 1 + Level 2/3 (Comms/Testing Rules & Skills) + Target files.

4. **LARGE** (Multi-node RS485 integration, protocol packet matrix change, subsystem refactor):
   - **Route:** Master → `system-architect` → Specialists (`stm32` + `esp32` + `communication`) → `qa-test-engineer` → `code-reviewer`.
   - **Subagents:** Multi-agent pipeline with orchestrated handoffs.
   - **Context:** Level 1 + Level 2/3 (All relevant rules & skills) + Target files.

5. **CRITICAL** (Safety shutdown logic, hardware pinout change, production release baseline):
   - **Route:** Master → `system-architect` → `hardware-engineer` → Specialists → `qa-test-engineer` → `code-reviewer` → **HUMAN APPROVAL GATE**.
   - **Subagents:** Full verification pipeline.
   - **Human Gate:** Mandatory human sign-off BEFORE committing code modifications.

## 2. CROSS-AGENT HANDOFF PROTOCOL

When transferring work between subagents, the sending agent MUST format its summary using the following standardized Handoff Payload:

```text
TASK: [Clear 1-sentence description of the subtask]
CONTEXT: [Key technical constraints or findings]
FILES TOUCHED: [Exact list of modified files]
CHANGES: [Summary of edits made]
TESTS: [Test execution command & pass/fail status]
RESULT: [SUCCESS / PARTIAL / FAILED]
RISKS: [Potential side-effects or regression risks]
NEXT ACTION: [Recommended next specialist or Human Gate]
```

## 3. FAILURE & RECOVERY POLICY

1. **Retry Cap:** A subagent may retry a failed modification maximum **2 times**.
2. **Escalation Path:** If a subagent fails twice, Master Agent MUST NOT re-invoke the same subagent with duplicate inputs. Master Agent MUST escalate to `system-architect` (for design analysis) or `code-reviewer` (for root cause audit).
3. **Hardware & Safety Halt:** If a failure involves a hardware pin conflict or safety logic violation, execution stops immediately, code modification is blocked, and control is handed over to **HUMAN GATE**.
4. **No Infinite Polling:** Subagents and background tasks notify Master Agent automatically via reactive messaging; polling loops are strictly prohibited.

## MUST NOT
1. DO NOT spawn subagents for TINY tasks.
2. DO NOT scan historical audit reports (`.agent/reports/`, 74 files) by default; Level 6 historical reports are BANNED unless explicitly requested.
3. DO NOT bypass the Human Gate on CRITICAL tasks or hardware authority conflicts.
