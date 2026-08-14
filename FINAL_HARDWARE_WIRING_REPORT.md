# EAGLEULTRASONİK — FINAL HARDWARE WIRING REPORT & HARDWARE ACCEPTANCE PACKAGE

**Document Version:** 3.0.0  
**Phase:** Phase 6.1 / Phase 5.2 — Final Hardware Wiring Master Authority  
**Date:** 2026-08-11  
**Status:** **READY FOR HARDWARE CONNECTION (Pending User Physical Breadboard Wiring & Photo Check)**

---

## 1. Executive Summary & Verification Matrix

All 7 specialized subagents (Datasheet/Electrical, STM32 Firmware, ESP32 Firmware, RS485 Electrical, Hardware Authority, System Safety, Test & Verification) have completed cross-verification. 

- Total System Wires Defined: **32 Master Physical Wires (W01 .. W32)**
- Automated Software Tests Passed: **48 PASSED, 18 SKIPPED (0 Failures)**
- Electrical Safety: Voltage divider on ESP32 GPIO18 ($10\text{k}\Omega / 18\text{k}\Omega \implies 3.21\text{V}$), 5V rail isolation, common signal GND bus, PA0 ADC 3.3V VH clamping verified.

---

## 2. Subagent Verification Summary

| Subagent Role | Target Domain | Status | Key Verdict |
| :--- | :--- | :---: | :--- |
| **Subagent 1** | Datasheet & Electrical Auditor | **PASS** | ESP32 GPIO18 10k/18k divider (3.21V max), STM32 PB11 5V FT safety, X9C VH=3.3V clamping verified. |
| **Subagent 2** | STM32 Firmware Auditor | **PASS** | PB10 (TX), PB11 (RX), PB1 (DE/RE), PA0 (ADC), PB12-14 (X9C), PB15 (Heater), PC6 (Triac), PC7 (ZC), PC8-11 (DIP SW) verified. |
| **Subagent 3** | ESP32 Firmware Auditor | **PASS** | GPIO8 (TX), GPIO18 (RX via divider), GPIO5 (DE), GPIO17 (HMI TX), GPIO16 (HMI RX), GPIO4 (ZC_SIM) verified. |
| **Subagent 4** | RS485 Electrical Auditor | **PASS** | MAX485 #1 and #2 5V supplies, A/B twisted pair, 120Ω termination resistors, common GND reference verified. |
| **Subagent 5** | Hardware Authority Auditor | **PASS** | Verified against `hardware_wiring_FINAL_AUTHORITY.md`. PC7 CN5-2/CN10-19 single pin mapping confirmed. |
| **Subagent 6** | System Safety Auditor | **PASS** | Isolated 5V rails, common signal GND bus, 1k series protection resistors on ZC_SIM and X9C VW verified. |
| **Subagent 7** | Test & Verification Engineer | **PASS** | 15-point pre-power-on checklist and 13-point power-on testing procedure formulated. Pytest 48/48 PASS. |

---

## 3. Physical Wiring Stage Sequence

1. **Stage 1 (Signal Ground Bus):** Connect Nucleo GND, ESP32 GND, Nextion GND, MAX485 #1 GND, MAX485 #2 GND, X9C VSS/VL GND to central Common Logic GND Bus.
2. **Stage 2 (Isolated Power Rails):** Connect Nucleo 5V to X9C VCC and MAX485 #1 VCC. Connect Nucleo 3.3V rail to X9C Pin 3 (VH). Connect ESP32 5V to MAX485 #2 VCC. Connect Nextion 5V to external supply. **DO NOT CONNECT 5V RAILS TOGETHER!**
3. **Stage 3 (STM32 Connections):** Connect PB10 $\to$ MAX485 #1 DI, PB11 $\leftarrow$ MAX485 #1 RO, PB1 $\to$ MAX485 #1 DE/RE. Connect PB12 $\to$ X9C CS, PB13 $\to$ X9C U/D, PB14 $\to$ X9C INC. Connect X9C VW $\to$ 1k $\to$ PA0. Connect PC8-PC11 $\to$ DIP SW 1-4 outputs.
4. **Stage 4 (ESP32 Connections):** Connect GPIO8 $\to$ MAX485 #2 DI, GPIO18 $\leftarrow$ 10k/18k divider output $\leftarrow$ MAX485 #2 RO, GPIO5 $\to$ MAX485 #2 DE/RE. Connect GPIO17 $\to$ Nextion RX, GPIO16 $\leftarrow$ Nextion TX. Connect GPIO4 $\to$ 1k $\to$ STM32 PC7.
5. **Stage 5 (RS485 Differential Bus):** Connect MAX485 #1 Line A $\leftrightarrow$ MAX485 #2 Line A with 120Ω resistor at both ends. Connect MAX485 #1 Line B $\leftrightarrow$ MAX485 #2 Line B with 120Ω resistor at both ends.
6. **Stage 6 (Pre-Power Checklist):** Complete 15-point multimeter checklist in `FINAL_PRE_POWER_CHECKLIST.md`.

---

## 4. Master Document Index

1. [`FINAL_SYSTEM_WIRING_TABLE.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/FINAL_SYSTEM_WIRING_TABLE.md)
2. [`FINAL_SYSTEM_WIRING_TABLE.csv`](file:///c:/Users/ern0e/EAGLEULTRASONiK/FINAL_SYSTEM_WIRING_TABLE.csv)
3. [`FINAL_POWER_DISTRIBUTION.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/FINAL_POWER_DISTRIBUTION.md)
4. [`FINAL_RS485_WIRING.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/FINAL_RS485_WIRING.md)
5. [`FINAL_PRE_POWER_CHECKLIST.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/FINAL_PRE_POWER_CHECKLIST.md)
6. [`RS485_FINAL_ELECTRICAL_SCHEMATIC.svg`](file:///c:/Users/ern0e/EAGLEULTRASONiK/RS485_FINAL_ELECTRICAL_SCHEMATIC.svg)

---

## 5. HUMAN GATES & ACTION REQUIRED

> [!CAUTION]
> **DO NOT APPLY POWER YET — PHYSICAL BREADBOARD WIRING IN PROGRESS**  
> Complete the physical breadboard wiring according to [`FINAL_SYSTEM_WIRING_TABLE.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/FINAL_SYSTEM_WIRING_TABLE.md).  
> Perform the 15-point multimeter power-off checklist in [`FINAL_PRE_POWER_CHECKLIST.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/FINAL_PRE_POWER_CHECKLIST.md) and report measurement results (or photo) before powering on.

---

## 6. Final Verdict

```text
================================================================================
FINAL VERDICT: READY FOR PHYSICAL HARDWARE CONNECTION
(Firmware, Drivers, Schematics, Pinout Matrix, and Safety Audits PASSED 100%;
Physical Breadboard Wiring and Multimeter Power-Off Verification Pending)
================================================================================
```
