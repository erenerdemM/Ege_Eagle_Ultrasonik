# EAGLEULTRASONİK — SYSTEM ENGINEERING DECISION REGISTER

---

## 1. Executive Summary

This register formally records all open human engineering decisions, technical trade-offs, and physical hardware validation setup requirements identified during the EAGLEULTRASONiK modernization plan.

Each decision is structured with an authoritative Decision ID, Topic, Existing Evidence, Knowns, Unknowns, Available Options, Recommended Next Discussion Point, and Manifesto Blocking Status.

---

## 2. Human Engineering Decision Register

### DEC-001: Standalone Requirement Specification Files for Basic Driver Modules
* **Topic:** Whether to generate standalone requirement `.md` specification files (e.g. `docs/SYS_BOOT_REQUIREMENTS.md`, `docs/STM_X9C103S_REQUIREMENTS.md`) for 6 basic C/C++ driver modules or retain them covered under master system manifestos.
* **Existing Evidence:**  
  Audit documents [`docs/SYSTEM_FUNCTION_INVENTORY.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/docs/SYSTEM_FUNCTION_INVENTORY.md) and [`docs/SYSTEM_COVERAGE_AUDIT_REPORT.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/docs/SYSTEM_COVERAGE_AUDIT_REPORT.md) show that 6 basic driver functions (`SYS-BOOT`, `STM-X9C103S`, `STM-TIMER-DOWN`, `ESP-ZERO-SIM`) are currently covered under generic manifesto requirements. All 6 have 100% implementation in C code and 100% pass rates across automated pytest suites.
* **What is Known:**  
  The software implementations and hardware behaviors are fully operational and verified via HIL loopback and pytest suites.
* **What is Unknown:**  
  Whether future system maintenance requires modular, granular requirement `.md` files per driver file or if system-level manifesto coverage is sufficient.
* **Available Options:**  
  * **Option A:** Retain driver requirement coverage under master technical manifestos (`SYSTEM_MANIFESTO.md`).  
  * **Option B:** Author individual modular `.md` requirement specifications for each basic driver module.
* **Recommended Discussion Point:** Keep master manifesto coverage for prototype completion; generate individual driver `.md` specifications if required during post-prototype production certification.
* **Manifesto Blocking Status:** **NON-BLOCKING** (Does not prevent manifesto generation).
* **Future Validation Requirement:** N/A (Documentation decision).

---

### DEC-002: Acoustic Transducer Frequency Sweep Physical Bench Setup
* **Topic:** Technical procedure and equipment alignment for Level 4 physical acoustic frequency sweep qualification when high-voltage power cards and transducer motors become available.
* **Existing Evidence:**  
  Level 3 HIL voltage step traces [`docs/C_SWEEP_GAP008_PHYSICAL_CALIBRATION_REPORT.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/docs/C_SWEEP_GAP008_PHYSICAL_CALIBRATION_REPORT.md) demonstrate that X9C103S digital pot wiper stepping ($1.05\text{ V} \dots 1.58\text{ V}$ for 28kHz span) and 50ms triangle timing are 100% operational in firmware. Level 4 physical acoustic validation is registered under `DR-002` in [`docs/SYSTEM_DEFERRED_REVALIDATION_REGISTER.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/docs/SYSTEM_DEFERRED_REVALIDATION_REGISTER.md).
* **What is Known:**  
  Digital pot wiper steps and sweep timing in `x9c103s.c` operate cleanly.
* **What is Unknown:**  
  Acoustic transducer impedance resonance curve under full liquid load and whether voltage step calibration matches transducer acoustic bandwidth.
* **Available Options:**  
  * **Option A:** Utilize hydrophone acoustic sensor array in liquid tank to measure acoustic power distribution across 25kHz–43kHz sweep span.  
  * **Option B:** Measure high-voltage AC current/voltage phase shift on power card output using current probe.
* **Recommended Discussion Point:** Align on hydrophone vs current probe measurement protocol prior to Phase 6 physical hardware commissioning.
* **Manifesto Blocking Status:** **NON-BLOCKING** (Formally registered under `DR-002`).
* **Future Validation Requirement:** Repeat Level 4 physical sweep qualification using physical transducer and power card.

---

### DEC-003: Liquid Tank Cavitation Degas Physical Test Setup
* **Topic:** Technical procedure for Level 4 physical liquid tank cavitation and dissolved oxygen (DO) reduction verification when physical liquid tank and transducer are attached.
* **Existing Evidence:**  
  Level 3 HIL gated PWM burst traces [`docs/B_DEGAS_E2E_VERIFICATION_REPORT.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/docs/B_DEGAS_E2E_VERIFICATION_REPORT.md) demonstrate that 1000ms ON / 500ms OFF gated TIM15 PWM bursts operate cleanly with soft-start ramps. Level 4 physical liquid validation is registered under `DR-003` in [`docs/SYSTEM_DEFERRED_REVALIDATION_REGISTER.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/docs/SYSTEM_DEFERRED_REVALIDATION_REGISTER.md).
* **What is Known:**  
  Gated PWM burst timing and UI state machine in `ultrasonic_pwm.c` operate cleanly.
* **What is Unknown:**  
  Fluid-specific dissolved oxygen (DO) PPM reduction rate and whether 1000ms ON / 500ms OFF burst timing requires viscosity-specific tuning for specific wash liquids.
* **Available Options:**  
  * **Option A:** Retain frozen prototype baseline (1000ms ON / 500ms OFF) for standard aqueous cleaning solutions.  
  * **Option B:** Introduce liquid-type selection parameter in HMI Service Menu to adjust burst duty cycle for high-viscosity fluids.
* **Recommended Discussion Point:** Review DO PPM reduction measurement results during physical liquid tank commissioning before altering burst timing parameters.
* **Manifesto Blocking Status:** **NON-BLOCKING** (Formally registered under `DR-003`).
* **Future Validation Requirement:** Measure DO PPM reduction curve on physical liquid tank system during Phase 6 physical commissioning.
