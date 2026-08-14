# EAGLEULTRASONİK — PHASE 6.2 OUTPUT GPIO AUDIT REPORT

## 1. OUTPUT GPIO SCAN & CATEGORIZATION

| Output GPIO | Function | Physical Target | Can be physically tested? | Required Loopback? | Test Method | Category |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **STM32 PB15** | Heater / SSR Relay | PA4 (Heater Feedback) | Yes | **Yes (PB15 $\to$ 1k$\Omega$ $\to$ PA4)** | Loopback Reading on PA4 ADC | **A) Physical Loopback Required** |
| **STM32 PC6** | TRIAC / TIM15 PWM | PA6 (Triac Feedback) | Yes | **Yes (PC6 $\to$ 1k$\Omega$ $\to$ PA6)** | Loopback Reading on PA6 ADC | **A) Physical Loopback Required** |
| **ESP32 GPIO4** | ZC Simulation (100Hz)| STM32 PC7 (EXTI7) | Yes | **Yes (GPIO4 $\to$ 1k$\Omega$ $\to$ PC7)**| Trigger Interrupt on PC7 | **A) Physical Loopback Required** |
| **STM32 PB12** | X9C Chip Select (CS) | X9C Pin 7 (CS) | Yes | **No (Tested via Wiper VW)** | VW Output Readback via PA0 ADC | **B) Direct Component Test** |
| **STM32 PB13** | X9C Up/Down (U/D) | X9C Pin 2 (U/D) | Yes | **No (Tested via Wiper VW)** | VW Output Readback via PA0 ADC | **B) Direct Component Test** |
| **STM32 PB14** | X9C Increment (INC) | X9C Pin 1 (INC) | Yes | **No (Tested via Wiper VW)** | VW Output Readback via PA0 ADC | **B) Direct Component Test** |
| **STM32 PB10** | USART3 TX Data | MAX485 #1 Pin 4 (DI) | Yes | **No** | RS485 Serial Communication | **B) Direct Component Test** |
| **STM32 PB1** | RS485 DE/RE Enable | MAX485 #1 Pins 2 & 3 | Yes | **No** | RS485 Transceiver Enable | **B) Direct Component Test** |
| **ESP32 GPIO8** | UART1 TX Data | MAX485 #2 Pin 4 (DI) | Yes | **No** | RS485 Serial Communication | **B) Direct Component Test** |
| **ESP32 GPIO5** | RS485 DE Enable | MAX485 #2 Pins 2 & 3 | Yes | **No** | RS485 Transceiver Enable | **B) Direct Component Test** |
| **ESP32 GPIO17**| UART2 HMI TX Data | Nextion Yellow Cable | Yes | **No** | Nextion Serial Communication | **B) Direct Component Test** |

---

## 2. CATEGORIES LEGEND

* **A) Physical Loopback Required**: Requires physical wiring to another GPIO to verify state output during bench test.
* **B) Direct Component Test**: Verified through direct interaction with connected physical component (e.g., UART communication, X9C wiper ADC).
* **C) Software Test Only**: State is checked purely via internal MCU registers (no physical external test).
* **D) Physical Test Not Required**: Not applicable for active physical validation.

---

## 3. CONFIRMATION OF KEY PHYSICAL LOOPBACKS

The following 3 physical loopbacks are confirmed as defined in the master wiring authority:
1. **Heater Loopback:** STM32 PB15 $\to$ 1k$\Omega$ Resistor $\to$ STM32 PA4 (CN7 Pin 32)
2. **TRIAC/PWM Loopback:** STM32 PC6 $\to$ 1k$\Omega$ Resistor $\to$ STM32 PA6 (CN10 Pin 13)
3. **Zero Cross Sim Loopback:** ESP32 GPIO4 $\to$ 1k$\Omega$ Resistor $\to$ STM32 PC7 (CN5-2 / CN10 Pin 19)

**ADDITIONAL PHYSICAL LOOPBACKS REQUIRED:** NONE

---

## 4. CONCLUSION

```text
OUTPUT AUDIT = PASS
```
