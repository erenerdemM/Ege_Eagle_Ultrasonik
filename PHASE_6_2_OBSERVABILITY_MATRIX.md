# EAGLEULTRASONİK — PHASE 6.2 OBSERVABILITY MATRIX

| Function | Source MCU | Pin | Physical Connection | Trigger / Command | Expected Signal | Measurement Point | Expected Value | Status |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :-: |
| **DIP SW ID** | STM32 | PC8-11 | DIP SW 1-4 to GND | DIP Switch Toggle | Active-LOW TTL | PC8-11 Voltmetre | LOW: 0.0V / HIGH: 3.3V | **PASS** |
| **X9C CS** | STM32 | PB12 | PB12 -> 1k -> PB4 | `X9C103S_SetStep` | Lojik CS Pulse | PB12 Logic Probe | Pulse: 3.3V -> 0V -> 3.3V | **PASS** |
| **X9C U/D** | STM32 | PB13 | PB13 -> 1k -> PB5 | `X9C103S_SetStep` | Lojik Direction | PB13 Voltmetre | UP: 3.3V / DOWN: 0.0V | **PASS** |
| **X9C INC** | STM32 | PB14 | PB14 -> 1k -> PB6 | `X9C103S_SetStep` | Lojik Clock Pulse | PB14 Logic Probe | 3.3V pulses (min 1us) | **PASS** |
| **X9C VW** | X9C | Pin 5 | VW -> 1k -> PA0 | Potentiometer Step | Analog DC Voltage | PA0 Voltmetre | $0.0\text{V} \dots 3.3\text{V DC}$ | **PASS** |
| **PA0 ADC** | STM32 | PA0 | ADC1_IN1 Channel | `HAL_ADC_GetValue` | 12-bit Digital Code | Debug / Telemetry | $0 \dots 4095$ counts | **PASS** |
| **Heater PB15** | STM32 | PB15 | PB15 -> 1k -> PA4 | `HeaterRelay_Set` | Digital Output HIGH | PB15 Voltmetre | OFF: 0.0V / ON: 3.3V | **PASS** |
| **Heater Feedback**| STM32 | PA4 | PA4 Input Pin | PB15 Activation | Logic High Level | PA4 Readback Code | 1 (Active) | **PASS** |
| **Triac PC6** | STM32 | PC6 | PC6 -> 1k -> PA6 | `UltrasonicPWM_Set` | Soft-Start Pulse | PC6 Scope Probe | Soft-Start PWM ($20\text{k-}40\text{kHz}$) | **PASS** |
| **Triac Feedback** | STM32 | PA6 | PA6 Input Pin | PC6 Activation | Logic Pulse Read | PA6 Readback Code | Pulse detected | **PASS** |
| **TIM1 PWM** | STM32 | PA8 | TIM1_CH1 Output | `HAL_TIM_PWM_Start` | Hardware PWM | PA8 Scope Probe | Hardware Ultrasonic PWM | **PASS** |
| **TIM15 OPM** | STM32 | Internal | TIM15 Interrupt | EXTI7 Zero-Cross | One-Pulse Delay | Interrupt Counter | Firing delay $500 \dots 9500\mu\text{s}$ | **PASS** |
| **ESP32 ZC Sim** | ESP32 | GPIO4 | GPIO4 -> 1k -> PC7 | `esp_timer` 100Hz | 100Hz Square Wave | GPIO4 Frequency Meter | **100.0 Hz** ($10\text{ ms}$ period) | **PASS** |
| **STM32 PC7 EXTI** | STM32 | PC7 | PC7 EXTI7 Input | GPIO4 100Hz Input | Rising Edge Interrupt| EXTI7 Interrupt Count| Increments 100 times/sec | **PASS** |
| **RS485 TX** | STM32 | PB10 | PB10 -> MAX485 #1 DI| `ESP32_UART_SendStatus`| UART TX Data Stream | PB10 Logic Probe | 115200 8N1 serial stream | **PASS** |
| **RS485 RX** | STM32 | PB11 | MAX485 #1 RO -> PB11| ESP32 Master Command| UART RX Data Stream | PB11 Logic Probe | 115200 8N1 serial stream | **PASS** |
| **RS485 DE/RE #1** | STM32 | PB1 | PB1 -> MAX485 #1 DE | Transmit Blocking | Direction Control | PB1 Voltmetre | Idle: 0V / Transmit: 3.3V | **PASS** |
| **RS485 DE/RE #2** | ESP32 | GPIO5 | GPIO5 -> MAX485 #2 DE| `rs485Transmit` | Direction Control | GPIO5 Voltmetre | Idle: 0V / Transmit: 3.3V | **PASS** |
| **RS485 Bus A/B** | Transceiver| Pins 6,7 | Line A/B Twisted Pair| ASCII Protocol Msg | Differential Voltaj | Line A - Line B Scope | $V_A - V_B > 200\text{mV}$ | **PASS** |
| **Nextion TX** | ESP32 | GPIO17 | GPIO17 -> Nextion RX| `nextionGonder` | HMI Serial Data | GPIO17 Logic Probe | 9600 8N1 + `0xFF*3` | **PASS** |
| **Nextion RX** | ESP32 | GPIO16 | Nextion TX -> GPIO16| User Touch Input | HMI Command Frame | GPIO16 Logic Probe | Touch event bytes | **PASS** |
| **PT100 ADC** | STM32 | PA1 | OPAMP3_VINP Channel| `PT100_ADC_Process` | Analog Sensor Voltage| Telemetry `temp_x10` | Temperature x 10 ($250 = 25.0^\circ\text{C}$) | **PASS** |
| **Watchdog** | ESP32 | Internal | FreeRTOS Timer | 3000ms Silence | Watchdog Trip | `stm_bagli` Flag | Timeout clears connection flag | **PASS** |
