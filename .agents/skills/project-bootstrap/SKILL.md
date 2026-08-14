---
name: project-bootstrap
description: Fast context initialization and state discovery for EAGLEULTRASONİK workspace using PROJECT_STATE.md and core rules.
---

# SKILL — PROJECT BOOTSTRAP & STATE INITIALIZATION

## Purpose
Initialize conversation context rapidly with zero token bloat by reading `PROJECT_STATE.md` and identifying active hardware/firmware baselines.

## When to Use (Trigger)
- Start of any new Antigravity conversation session.
- When user asks "What is the current project state?" or "Where did we leave off?".

## Required Context Files
1. `PROJECT_STATE.md` (Level 1 Context)
2. `AGENTS.md` (Role definitions entrypoint)

## Procedure
1. View `PROJECT_STATE.md` using `view_file`.
2. Extract Current Phase, Hardware Baseline, Active Tasks, and Known Issues.
3. Classify incoming user task using `07-agent-orchestration.md` task router logic (TINY, SMALL, NORMAL, LARGE, CRITICAL).
4. Select minimum required specialist agent(s) and load relevant skills on-demand.

## Verification
- Confirm Current Phase matches `PROJECT_STATE.md`.
- Ensure token consumption remains under 1,000 tokens during bootstrap phase.

## Exit Criteria
- Task complexity classified; minimum agent set identified; ready to execute.
