# EAGLEULTRASONİK — PROJECT CLEANUP CANDIDATE REGISTER

---

## 1. Executive Summary

This register formally classifies all 590 files in the repository and provides a comprehensive risk-assessed cleanup candidate inventory. 

> [!IMPORTANT]
> **READ-ONLY ASSESSMENT:** This document only identifies and classifies cleanup candidates. **Zero files have been deleted, moved, or modified.** Any future cleanup actions must be explicitly authorized by engineering.

---

## 2. Inventory Classification Totals

| Classification Code | Category Name | Description | Count | Action Policy |
| :---: | :--- | :--- | :---: | :--- |
| **A** | **AUTHORITATIVE / KEEP** | Active firmware, GUI, test suites, rules, skills, and current baseline documentation | **186** | **PRESERVE** |
| **B** | **SUPPORT / KEEP** | Active build scripts, test runners, flash utilities, and IDE project definitions | **11** | **PRESERVE** |
| **C** | **HISTORICAL / ARCHIVE** | Historical phase audit reports, superseded milestone summaries, and legacy markdown | **173** | **ARCHIVE CANDIDATE** |
| **D** | **LEGACY / DELETE** | Root duplicate source files, deprecated bench tests, and obsolete PIC code | **8** | **DELETE CANDIDATE** |
| **E** | **GENERATED** | Binary compiler outputs, pytest cache, python bytecode, and test log files | **183** | **GITIGNORE / PURGE** |
| **F** | **TEMPORARY / SCRATCH** | Dated test artifact backups, scratch scripts, and temporary zips | **29** | **PURGE / ARCHIVE** |
| **G** | **UNKNOWN / REVIEW** | Unclassified files requiring manual engineering review | **0** | **NONE** |
| **TOTAL** | — | **All Tracked & Untracked Files in Workspace** | **590** | — |

---

## 3. High-Priority Cleanup Candidate Register

### 3.1 Category D: Legacy Source & Deprecated Tests (Delete Candidates — 8 Files)

| Path | Class | Role | Reference Status | Cleanup Candidate | Risk | Recommendation |
| :--- | :---: | :--- | :--- | :---: | :---: | :--- |
| `main.c` (Root) | **D** | Root duplicate of `STM32/.../Core/Src/main.c` | UNREFERENCED | **DELETE** | **LOW** | Safe to delete; active build uses `STM32/Ultrasonik_G4_Master/Core/Src/main.c`. |
| `x9c103s.c` (Root) | **D** | Root duplicate of `STM32/.../Core/Src/x9c103s.c` | UNREFERENCED | **DELETE** | **LOW** | Safe to delete; active build uses `STM32/Ultrasonik_G4_Master/Core/Src/x9c103s.c`. |
| `heater_triac_bench_test.c` | **D** | Early standalone bench test artifact | UNREFERENCED | **DELETE** | **LOW** | Safe to delete; superseded by `test_hil_uart.py`. |
| `heater_triac_bench_test.h` | **D** | Early standalone bench test header | UNREFERENCED | **DELETE** | **LOW** | Safe to delete; superseded by `test_hil_uart.py`. |
| `id_bench_test.c` | **D** | Early standalone bench test artifact | UNREFERENCED | **DELETE** | **LOW** | Safe to delete; superseded by `id_full_lifecycle_test.py`. |
| `id_bench_test.h` | **D** | Early standalone bench test header | UNREFERENCED | **DELETE** | **LOW** | Safe to delete; superseded by `id_full_lifecycle_test.py`. |
| `dip_switch_test.py` | **D** | Deprecated test for removed DIP switches | UNREFERENCED | **DELETE** | **LOW** | Safe to delete; DIP switches were removed in Phase 5 ID lifecycle. |
| `STM32/.../Eski_PIC_Kodlari/500W_Display.mbas`| **D** | Obsolete legacy PIC microcontroller code | UNREFERENCED | **DELETE** | **LOW** | Safe to delete or move to external legacy repository. |

---

### 3.2 Category F: Temporary Scripts & Backups (Purge/Archive Candidates — 29 Files)

| Path | Class | Role | Reference Status | Cleanup Candidate | Risk | Recommendation |
| :--- | :---: | :--- | :--- | :---: | :---: | :--- |
| `TEST_ARTIFACTS_BACKUP_20260816_171329/*` (16 files) | **F** | Dated pre-refactor source & test backup snapshot | UNREFERENCED | **ARCHIVE / DELETE** | **LOW** | Safe to archive externally or delete; git history preserves state. |
| `scratch/` (12 files) | **F** | Helper scripts (`capture_stm32_boot.py`, `restore_baseline.py`, etc.) | INDIRECTLY REFERENCED | **KEEP / SCRATCH** | **MEDIUM** | Retain in `scratch/` for developer debugging; do not deploy to production. |
| `EKRAN/Arayuz.zi` | **F** | Legacy zip backup of HMI project | UNREFERENCED | **DELETE** | **LOW** | Safe to delete; `EKRAN/arayuz.HMI` is the active source of truth. |

---

### 3.3 Category E: Generated Outputs & Caches (Gitignore Candidates — 183 Files)

| Path | Class | Role | Reference Status | Cleanup Candidate | Risk | Recommendation |
| :--- | :---: | :--- | :--- | :---: | :---: | :--- |
| `STM32/.../build-stm32/*` (41 files) | **E** | GCC ARM binary build artifacts (`.o`, `.elf`, `.bin`, `.hex`, `.map`) | BUILD OUTPUT | **GITIGNORE** | **LOW** | Add `STM32/**/build-stm32/` to `.gitignore`; rebuild via `tools/build_stm32.sh`. |
| `logs/*` (120 files) | **E** | Automated test execution trace logs | TEST OUTPUT | **GITIGNORE** | **LOW** | Add `logs/` to `.gitignore`. |
| `test_results.log` | **E** | Test runner output log | TEST OUTPUT | **GITIGNORE** | **LOW** | Add `*.log` to `.gitignore`. |
| `__pycache__/*` (9 files) | **E** | Python 3 bytecode cache (`.pyc`) | RUNTIME CACHE | **GITIGNORE** | **LOW** | Add `__pycache__/` to `.gitignore`. |
| `.pytest_cache/*` (5 files) | **E** | Pytest test execution cache | TEST CACHE | **GITIGNORE** | **LOW** | Add `.pytest_cache/` to `.gitignore`. |

---

### 3.4 Category C: Historical Phase Reports & Superseded Documents (Archive Candidates — 173 Files)

| Path Pattern | Count | Role | Reference Status | Cleanup Candidate | Risk | Recommendation |
| :--- | :---: | :--- | :--- | :---: | :---: | :--- |
| `.agent/reports/*` | **86** | Subagent milestone reports (Phases 1–14) | HISTORICAL AUDIT | **ARCHIVE** | **LOW** | Move to `archive/agent_reports/` during major cleanup gate. |
| Root `PHASE_6_2_*.md` | **31** | Phase 6.2 detailed wiring & bench reports | SUPERSEDED | **ARCHIVE** | **LOW** | Consolidated by `hardware_wiring_FINAL_AUTHORITY.md`. |
| Root `FINAL_*.md` | **10** | Intermediate wiring and schematic drafts | SUPERSEDED | **ARCHIVE** | **LOW** | Consolidated by `hardware_wiring_FINAL_AUTHORITY.md`. |
| Root `RS485_*.md` / `svg` | **6** | Intermediate RS485 wiring & reports | SUPERSEDED | **ARCHIVE** | **LOW** | Consolidated by `hardware_wiring_FINAL_AUTHORITY.md`. |
| Root `Manifesto_V3.md` / `SYSTEM_MANIFESTO.md` | **3** | Superseded early manifesto drafts | SUPERSEDED | **ARCHIVE** | **LOW** | Consolidated by `docs/EAGLEULTRASONIK_SYSTEM_MANIFESTO.md`. |
| Root `AGENT_OS_V2_*.md` | **4** | Agent OS v2 bootstrapping acceptance reports | HISTORICAL | **ARCHIVE** | **LOW** | Move to `archive/agent_os_reports/`. |
| Historical `docs/*.md` drafts | **33** | Superseded intermediate risk/coverage audits | HISTORICAL | **ARCHIVE** | **LOW** | Move to `archive/historical_docs/`. |

---

## 4. Highest-Risk Cleanup Candidates Assessment

| Candidate File / Group | Intended Action | Potential Risk if Accidentally Deleted | Safety Safeguard |
| :--- | :---: | :--- | :--- |
| `scratch/restore_baseline.py` | Temporary Script | Developer loses quick one-line script to reset DUT to factory defaults | Script is documented in `docs/P1_BATCH2_IMPLEMENTATION_REPORT.md` |
| `STM32/.../Ultrasonik_G4_Master.ioc` | STM32CubeMX Project | Loss of GUI peripheral configuration descriptor | File is classified as **Authoritative (A)** and **MUST NOT BE DELETED** |
| `EKRAN/arayuz.HMI` | Nextion GUI Source | Loss of graphical button layouts, fonts, and touch coordinates | File is classified as **Authoritative (A)** and **MUST NOT BE DELETED** |
| `hardware_wiring_FINAL_AUTHORITY.md` | Hardware Wiring Bible | Loss of authoritative physical pinout definitions | File is classified as **Authoritative (A)** and **MUST NOT BE DELETED** |

---

## 5. Recommended Cleanup Sequencing (For Future Execution)

When a cleanup phase is formally initiated by engineering:
1. **Step 1 (Zero-Risk Deletions):** Delete root duplicate source files (`main.c`, `x9c103s.c`, `heater_triac_bench_test.*`, `id_bench_test.*`, `dip_switch_test.py`, `500W_Display.mbas`, `EKRAN/Arayuz.zi`).
2. **Step 2 (Gitignore Build Outputs):** Add `build-stm32/`, `logs/`, `__pycache__/`, `.pytest_cache/`, and `*.log` to `.gitignore`.
3. **Step 3 (Archive Historical Reports):** Move root `PHASE_6_2_*.md`, `FINAL_*.md`, `RS485_*.md`, and `.agent/reports/` to a dedicated `archive/` folder.
4. **Step 4 (Preserve Core Authority):** Ensure all 186 Authoritative (A) and 11 Support (B) files remain untouched in their exact primary paths.

---
*Register completed under Phase 16 read-only inventory. Zero repository files were deleted, moved, or modified.*
