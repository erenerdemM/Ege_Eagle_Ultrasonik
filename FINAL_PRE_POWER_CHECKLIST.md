# EAGLEULTRASONİK — PRE-POWER-ON CHECKLIST & PHYSICAL TESTING PROCEDURE

**Document Version:** 1.0.0  
**Date:** 2026-08-11  

---

## 1. Power-Off Multimeter Checklist (MUST COMPLETE BEFORE APPLYING POWER)

- [ ] **1. GND Continuity Test:** Multimeter set to Diode/Continuity. Measure resistance between Nucleo GND, ESP32 GND, Nextion GND, MAX485 #1 GND, MAX485 #2 GND, and X9C VSS/VL GND.  
  *Expected:* $R < 0.5\Omega$ (Continuous beep). **STOP if open circuit!**

- [ ] **2. 5V-to-GND Short Test:** Measure resistance between Common GND Bus and Nucleo 5V, ESP32 5V, Nextion 5V rails.  
  *Expected:* $R > 100\Omega$ (No short). **STOP if continuous beep!**

- [ ] **3. 3.3V-to-GND Short Test:** Measure resistance between Common GND Bus and Nucleo 3.3V rail.  
  *Expected:* $R > 100\Omega$ (No short). **STOP if continuous beep!**

- [ ] **4. 5V Rail Isolation Test:** Measure resistance between Nucleo 5V rail and ESP32 5V rail, and between Nucleo 5V rail and Nextion 5V rail.  
  *Expected:* Open circuit ($R = \infty$, No beep). **STOP if shorted!**

- [ ] **5. ESP32 GPIO18 Voltage Divider Test:** Measure resistance from MAX485 #2 RO pin (Pin 1) to ESP32 GPIO18 ($R_{\text{top}} = 10\text{k}\Omega \pm 5\%$), and from ESP32 GPIO18 to Common GND Bus ($R_{\text{bottom}} = 18\text{k}\Omega \pm 5\%$). Total resistance MAX485 RO to GND = $28\text{k}\Omega \pm 5\%$.  
  *Expected:* Exact 10k/18k divider values verified.

- [ ] **6. RS485 Bus Impedance Test:** Measure resistance between Differential Line A and Differential Line B with both 120Ω termination resistors installed.  
  *Expected:* $R_{\text{bus}} \approx 60\Omega \pm 5\%$ ($120\Omega \parallel 120\Omega$).

- [ ] **7. Line A / B to GND Isolation:** Measure resistance from Line A to GND Bus and Line B to GND Bus.  
  *Expected:* High impedance ($R > 1\text{k}\Omega$).

- [ ] **8. X9C VH Pin Terminal Check:** Measure voltage/pin path from Nucleo 3.3V rail to X9C Pin 3 (VH).  
  *Expected:* Directly wired to 3.3V (**MUST NOT BE CONNECTED TO 5V!**).

- [ ] **9. ZC_SIM Pin Series Resistor Test:** Verify 1kΩ series resistor installed between ESP32 GPIO4 and STM32 PC7 (CN5-2/CN10-19).

- [ ] **10. X9C Wiper Series Resistor Test:** Verify 1kΩ series resistor installed between X9C Pin 5 (VW) and STM32 PA0 ADC pin.

- [ ] **11. DE/RE Control Pins Check:** Verify STM32 PB1 wired to MAX485 #1 DE+/RE, and ESP32 GPIO5 wired to MAX485 #2 DE+/RE.

- [ ] **12. UART TX/RX Crossover Check:** Verify STM32 PB10 (TX) -> MAX485 #1 DI, STM32 PB11 (RX) <- MAX485 #1 RO. ESP32 GPIO8 (TX) -> MAX485 #2 DI, ESP32 GPIO18 (RX) <- MAX485 #2 RO divider output.

- [ ] **13. DIP Switch ID Wiring Check:** Verify DIP_SW1-4 wired to PC8-PC11 with common return connected to Common GND Bus.

- [ ] **14. Nextion HMI Crossover Check:** Verify ESP32 GPIO17 (TXD2) -> Nextion RX (Yellow wire), ESP32 GPIO16 (RXD2) <- Nextion TX (Blue wire).

- [ ] **15. 220V AC Isolation Verification:** Confirm 0 physical 220V AC mains wires are connected to the tabletop bench setup.

---

## 2. Power-On Step-by-Step Testing Procedure

1. **Step 1:** Plug in Nucleo USB cable (USB #1) ONLY. Measure Nucleo 5V ($4.75\text{V} \dots 5.25\text{V}$) and Nucleo 3.3V ($3.25\text{V} \dots 3.35\text{V}$).
2. **Step 2:** Measure X9C Pin 3 (VH) voltage. **MUST READ EXACTLY 3.3V DC**. (Disconnect power immediately if 5V is read!).
3. **Step 3:** Plug in ESP32 USB cable (USB #2). Measure ESP32 5V rail ($4.75\text{V} \dots 5.25\text{V}$).
4. **Step 4:** Measure Common GND Offset between Nucleo GND and ESP32 GND. **MUST READ 0.0 mV DC**.
5. **Step 5:** Measure DE/RE idle voltage on STM32 PB1 and ESP32 GPIO5. **MUST READ 0.0V DC** (RX Mode idle).
6. **Step 6:** Measure MAX485 #2 RO voltage while idle ($5.0\text{V}$ HIGH) and voltage divider output at ESP32 GPIO18 (**MUST READ 3.21V DC**).
7. **Step 7:** Run `python -m pytest -v` integration suite.
