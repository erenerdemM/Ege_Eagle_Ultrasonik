# PHASE 6.2 - HARDWARE TEST LOOPBACKS

This document details the physical loopback requirements for testing the STM32G474RE outputs in hardware in-the-loop (HIL) environments without connecting actual high-voltage loads.

## Loopback Requirements

To verify the correct operation of control outputs (SSR, TRIAC, PWM) and their corresponding ADC feedback channels, the following physical resistor loopbacks must be installed on the Nucleo board headers:

### 1. Heater / SSR Output Loopback
*   **Source (Output):** PB15 (CN10-26) - SSR Heater Control Signal
*   **Path:** Through a 1kΩ current-limiting resistor (`R-HEATER-FB`)
*   **Destination (Input):** PA4 (CN7-32) - Heater Current ADC Feedback
*   **Purpose:** Simulates a load current feedback proportional to the SSR output state.

### 2. Triac Output Loopback
*   **Source (Output):** PC6 (CN10-4) - TRIAC Gate Control Signal
*   **Path:** Through a 1kΩ current-limiting resistor (`R-TRIAC-FB`)
*   **Destination (Input):** PA6 (CN10-13) - TRIAC Current ADC Feedback
*   **Purpose:** Simulates a load current feedback proportional to the TRIAC output state.

### 3. Timer Output Loopback
*   **Source (Output):** TIM15 Soft-start PWM on PC6 (CN10-4)
*   **Path:** Through a 1kΩ current-limiting resistor (`R-TRIAC-FB`)
*   **Destination (Input):** PA6 (CN10-13) - PWM ADC Feedback (Shared with TRIAC feedback)
*   **Purpose:** Allows testing of the TIM15 PWM generation by measuring the average voltage or duty cycle at the PA6 ADC input.

## Loopback Matrix Table

| Control Output | Nucleo Pin (Header) | Series Resistor | ADC Feedback Input | Nucleo Pin (Header) | Status |
| :--- | :--- | :--- | :--- | :--- | :--- |
| Heater / SSR | PB15 (CN10-26) | 1kΩ (`R-HEATER-FB`) | Heater Current | PA4 (CN7-32) | Loopback Required |
| TRIAC Gate | PC6 (CN10-4) | 1kΩ (`R-TRIAC-FB`) | TRIAC Current | PA6 (CN10-13) | Loopback Required |
| TIM15 PWM | PC6 (CN10-4) | 1kΩ (`R-TRIAC-FB`) | PWM Monitor | PA6 (CN10-13) | Loopback Required |
| Zero Cross Sim | ESP32 GPIO4 | 1kΩ (`R-ZC-SIM`) | Zero Cross Input | STM32 PC7 (CN5-2/CN10-19) | Loopback Required |
| RS485 TX/RX | PA9 / PA10 | N/A | N/A | N/A | No Loopback Required (Mocked in SW) |
| PT100 OPAMP3 | PB0 | N/A | N/A | N/A | No Loopback Required (Simulated) |
| X9C CS | STM32 PB12 | 1kΩ (`R_L3`) | DIP Switch 1 | STM32 PB4 | Loopback Required |
| X9C U/D | STM32 PB13 | 1kΩ (`R_L4`) | DIP Switch 2 | STM32 PB5 | Loopback Required |
| X9C INC | STM32 PB14 | 1kΩ (`R_L5`) | DIP Switch 3 | STM32 PB6 | Loopback Required |

ADDITIONAL PHYSICAL LOOPBACKS: NONE
