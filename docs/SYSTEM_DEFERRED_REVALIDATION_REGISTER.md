# EAGLEULTRASONİK — SYSTEM DEFERRED REVALIDATION REGISTER

---

## 1. Executive Summary

This register serves as the authoritative tracking document for all system functions, test cases, and physical hardware qualifications formally classified as **DEFERRED** pending physical sensor, load, or transducer hardware availability.

### Deferred Categories Summary:
* **DR-001:** PT100 Sensor & AC Heater Load Revalidation
* **DR-002:** Ultrasonic Acoustic Transducer & Power Card Frequency Sweep Validation
* **DR-003:** Liquid Tank Cavitation Degas Bubble Removal Validation

---

## 2. Deferred Revalidation Register

### Item DR-001: PT100 Sensor & AC Heater Load Revalidation
* **Affected Functions:** `STM-PT100-ADC`, `STM-HEATER-RELAY`
* **Affected Test Case:** `test_17_physical_loopback_readback` in `test_hil_uart.py`
* **Current Status:** **DEFERRED — REQUIRED PT100 HARDWARE UNAVAILABLE**
* **Reason:** Physical PT100 temperature sensor probe and AC heater/SSR load are currently unavailable on the physical test bench. The test was executed, but its result cannot be accepted as functional PASS/FAIL evidence until actual physical PT100 and heater hardware are attached.
* **Required Future Hardware:**
  1. Physical PT100 RTD temperature sensor probe connected to OPAMP3 PA1 input.
  2. AC Triac / SSR heater relay connected to PB0 GPIO output.
  3. Representative thermal load / heating element.
* **Required Future Acceptance Criteria:**
  - **Check A (Below Target):** Measured temperature < target $\implies$ `heater_out` set to HIGH ($1$).
  - **Check B (At/Above Target):** Measured temperature $\ge$ target / hysteresis threshold $\implies$ `heater_out` set to LOW ($0$).
  - **Check C (Hysteresis Window):** Verify relay toggles cleanly within the $\pm 1.0^\circ\text{C}$ hysteresis band.
  - **Check D (Forced-Off Disarm):** Verify `heater_out` is unconditionally forced LOW ($0$) in `SYS_MODE_IDLE`, `SYS_MODE_FAULT`, and `SafeStop` disarm states.
  - **Check E (DEGAS Temperature Path):** If DEGAS temperature control is enabled, verify `degas_target_temp_c` regulation path.

---

### Item DR-002: Ultrasonic Acoustic Transducer & Power Card Sweep Validation
* **Affected Functions:** `SWP-FREQ-SWEEP`, `STM-TIM15-PWM`, `STM-X9C103S`
* **Current Status:** **DEFERRED — LEVEL 4 PHYSICAL HARDWARE UNAVAILABLE** (Level 3 X9C pot voltage step ladder trace is **PASSED**).
* **Reason:** Physical high-voltage AC ultrasonic driver power card, 28kHz/40kHz acoustic transducer motor, hydrophone, and acoustic tank are currently unavailable.
* **Required Future Hardware:**
  1. Ultrasonic power card / AC driver stage.
  2. 28kHz / 40kHz acoustic transducer motor element.
  3. Hydrophone / acoustic power sensor meter.
* **Required Future Acceptance Criteria:**
  - Verify acoustic power output uniformity across 25kHz–43kHz sweep span.
  - Verify acoustic cavitation energy distribution during 400ms sweep period.
  - Verify transducer resonance tracking under varying tank liquid loads.

---

### Item DR-003: Liquid Tank Cavitation Degas Bubble Removal Validation
* **Affected Functions:** `DEG-PULSE-DEGAS`, `STM-TIM15-PWM`
* **Current Status:** **DEFERRED — LEVEL 4 PHYSICAL HARDWARE UNAVAILABLE** (Level 3 gated PWM burst trace is **PASSED**).
* **Reason:** Physical liquid tank system, ultrasonic transducer, and dissolved oxygen (DO) PPM sensor meter are currently unavailable.
* **Required Future Hardware:**
  1. Physical liquid tank filled with representative wash solution.
  2. Ultrasonic transducer and power driver card.
  3. Dissolved oxygen (DO) PPM sensor meter.
* **Required Future Acceptance Criteria:**
  - Verify DO PPM reduction rate during 15-minute DEGAS cycle (1000ms ON / 500ms OFF gated bursts).
  - Verify absence of standing acoustic wave interference in liquid tank.
  - Verify liquid degassing performance across 28kHz baseline.

---

## 3. Revalidation Execution Policy

When physical hardware assets become available:
1. Connect physical PT100 probe, heater element, ultrasonic transducer, and liquid tank to the controller bench.
2. Perform physical revalidation tests defined under `DR-001`, `DR-002`, and `DR-003`.
3. Update [`docs/SYSTEM_E2E_EXECUTION_REPORT.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/docs/SYSTEM_E2E_EXECUTION_REPORT.md) and [`docs/SYSTEM_FUNCTION_TRACEABILITY_MATRIX.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/docs/SYSTEM_FUNCTION_TRACEABILITY_MATRIX.md) with physical hardware PASS/FAIL evidence.
