# PHASE 6.2 PIN CONNECTOR CROSS REFERENCE

## 1. STM32G474RE Pins
| GPIO | Function | Direction | Board | Connector | Physical Pin Number | Resistor |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **PA0** | ADC1_IN1 (X9C Wiper) | Input | NUCLEO-G474RE | Morpho CN7 | 28 | 1kΩ |
| **PA1** | OPAMP3_VINP/ADC2_IN2 (PT100) | Input | NUCLEO-G474RE | Morpho CN7 | 30 | None |
| **PA4** | GPIO Input (Heater Relay FB) | Input | NUCLEO-G474RE | Morpho CN7 | 32 | None |
| **PA6** | GPIO Input (Triac Gate FB) | Input | NUCLEO-G474RE | Morpho CN10 | 13 | None |
| **PB1** | GPIO Output (MAX485 DE/RE) | Output | NUCLEO-G474RE | Morpho CN7 | 24 | None |
| **PB10** | USART3_TX | Output | NUCLEO-G474RE | Morpho CN10 | 25 | 1kΩ |
| **PB11** | USART3_RX | Input | NUCLEO-G474RE | Morpho CN10 | 18 | 1kΩ |
| **PB12** | GPIO Output (X9C CS) | Output | NUCLEO-G474RE | Morpho CN10 | 16 | 0Ω/1kΩ (Loopback) |
| **PB13** | GPIO Output (X9C U/D) | Output | NUCLEO-G474RE | Morpho CN10 | 30 | 0Ω/1kΩ (Loopback) |
| **PB14** | GPIO Output (X9C INC) | Output | NUCLEO-G474RE | Morpho CN10 | 28 | 0Ω/1kΩ (Loopback) |
| **PB15** | GPIO Output (Heater Relay) | Output | NUCLEO-G474RE | Morpho CN10 | 26 | 0Ω/1kΩ (Loopback) |
| **PC6** | TIM15 PWM (Triac Gate) | Output | NUCLEO-G474RE | Morpho CN10 | 4 | 0Ω/1kΩ (Loopback) |
| **PC7** | EXTI7 (Zero-Cross Input) | Input | NUCLEO-G474RE | Morpho CN5/CN10 | CN5-2 / CN10-19 | 1kΩ |
| **PC8** | GPIO Input (DIP Switch 1) | Input | NUCLEO-G474RE | Morpho CN10 | 2 | None (Pull-up) |
| **PC9** | GPIO Input (DIP Switch 2) | Input | NUCLEO-G474RE | Morpho CN10 | 1 | None (Pull-up) |
| **PC10** | GPIO Input (DIP Switch 3) | Input | NUCLEO-G474RE | Morpho CN10 | 3 | None (Pull-up) |
| **PC11** | GPIO Input (DIP Switch 4) | Input | NUCLEO-G474RE | Morpho CN10 | 5 | None (Pull-up) |

## 2. ESP32-S3 Pins
| GPIO | Function | Direction | Board | Connector | Physical Pin Number | Resistor |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **GPIO4** | ZC_SIM_PIN | Output | ESP32-S3 | GPIO Header | N/A | 1kΩ |
| **GPIO5** | RS485_DE_PIN | Output | ESP32-S3 | GPIO Header | N/A | None |
| **GPIO8** | STM_TXD (UART1_TX) | Output | ESP32-S3 | GPIO Header | N/A | 1kΩ |
| **GPIO16** | RXD2 (Nextion RX) | Input | ESP32-S3 | GPIO Header | N/A | 1kΩ |
| **GPIO17** | TXD2 (Nextion TX) | Output | ESP32-S3 | GPIO Header | N/A | 1kΩ |
| **GPIO18** | STM_RXD (UART1_RX) | Input | ESP32-S3 | GPIO Header | N/A | 1kΩ |
