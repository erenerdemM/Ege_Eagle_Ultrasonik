# EAGLEULTRASONİK — PHASE 6.2 RESISTOR TWO-TERMINAL NETLIST

```text
RESISTOR TWO-TERMINAL NETLIST
STATUS: VERIFIED & FROZEN
```

---

## 1. EXPLICIT TWO-TERMINAL RESISTOR MASTER TABLE

Projede kullanılan 9 adet direncin her birinin iki terminali, voltaj seviyeleri ve kullanım amaçları aşağıdaki gibidir:

| Resistor ID | Value | Terminal A (Node 1) | Voltage A | Terminal B (Node 2) | Voltage B | Purpose / Electrical Function | Mode |
| :--- | :-: | :--- | :-: | :--- | :-: | :--- | :-: |
| **R-TERM1** | **120$\Omega$** | MAX485 #1 Pin 6 (A) | $2.5\dots3.5\text{V}$ | MAX485 #1 Pin 7 (B) | $1.5\dots2.5\text{V}$ | RS485 Bus Node 1 Line Termination ($120\Omega$) | BENCH & PROD |
| **R-TERM2** | **120$\Omega$** | MAX485 #2 Pin 6 (A) | $2.5\dots3.5\text{V}$ | MAX485 #2 Pin 7 (B) | $1.5\dots2.5\text{V}$ | RS485 Bus Node 2 Line Termination ($120\Omega$) | BENCH & PROD |
| **R-DIV-UPPER**| **10k$\Omega$** | MAX485 #2 Pin 1 (RO) | 5.0V CMOS | ESP32 GPIO18 Pin | 3.214V Max | 5V $\to$ 3.21V Voltage Divider Upper Resistor | BENCH & PROD |
| **R-DIV-LOWER**| **18k$\Omega$** | ESP32 GPIO18 Pin | 3.214V Max | Common Signal GND Bus | 0.0V DC | 5V $\to$ 3.21V Voltage Divider Lower Resistor | BENCH & PROD |
| **R-ZC-SIM** | **1k$\Omega$** | ESP32 GPIO4 Pin | 3.3V Logic | STM32 PC7 (CN5-2) | 3.3V Logic | 100Hz Zero-Cross Signal Protection Resistor | **BENCH ONLY** |
| **R-X9C-VW** | **1k$\Omega$** | X9C103S Pin 5 (VW) | $0.0\dots3.3\text{V}$| STM32 PA0 (CN7-28) | $0.0\dots3.3\text{V}$| Wiper Output Current Limiter / PA0 ADC Protection| BENCH & PROD |
| **R-HEATER-FB**| **1k$\Omega$** | STM32 PB15 (CN10-26) | 3.3V Logic | STM32 PA4 (CN7-32) | 3.3V Logic | Heater Output Logic Readback Loopback | **BENCH ONLY** |
| **R-TRIAC-FB** | **1k$\Omega$** | STM32 PC6 (CN10-4) | 3.3V Logic | STM32 PA6 (CN10-13) | 3.3V Logic | Triac TIM15 PWM Waveform Readback Loopback | **BENCH ONLY** |
| **R-MOC-LED** | **150$\Omega$** | STM32 PC6 (CN10-4) | 3.3V Logic | MOC3021 Pin 1 (Anode) | 1.15V Forward | Optocoupler LED Current Limiting ($14.3\text{mA}$) | **PRODUCTION ONLY**|

---

## 2. VOLTAGE DIVIDER FORMULA & PROOF

```text
VOLTAGE DIVIDER SCHEMATIC:
MAX485 #2 Pin 1 (RO, 5.0V CMOS) ───► [10kΩ R-DIV-UPPER] ───┬───► ESP32 GPIO18
                                                           │
                                             [18kΩ R-DIV-LOWER]
                                                           │
                                                           ▼
                                                 Common Signal GND Bus (0V)

FORMULA:
V_GPIO18 = V_RO * [ R_DIV_LOWER / (R_DIV_UPPER + R_DIV_LOWER) ]

CALCULATION:
V_GPIO18 = 5.0V * [ 18,000 / (10,000 + 18,000) ]
V_GPIO18 = 5.0V * [ 18 / 28 ]
V_GPIO18 = 3.21428 V DC

VERIFICATION:
3.214V <= 3.30V (ESP32 Maximum Safe Input Voltage Limit). PASS.
```
