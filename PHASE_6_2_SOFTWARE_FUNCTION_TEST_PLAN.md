# PHASE_6_2_SOFTWARE_FUNCTION_TEST_PLAN

| Test ID | Function | Hardware | Input | Expected Output | Pass Criteria |
|---|---|---|---|---|---|
| TEST-01 | Power/GND | Power Supply | Apply 24V DC | 5V and 3.3V rails active | Voltage within ±5% |
| TEST-02 | STM32 boot | STM32 | Reset / Power On | STM32 enters main loop | Status LED blinks |
| TEST-03 | ESP32 boot | ESP32 | Reset / Power On | ESP32 FreeRTOS tasks start | Debug UART output OK |
| TEST-04 | X9C VW ADC | STM32/ESP32 ADC | Change X9C wiper | ADC value changes accordingly | Accurate ADC read |
| TEST-05 | RS485 STM32->ESP32 | RS485 Bus | Send test packet from STM32 | ESP32 receives valid packet | CRC matches, data correct |
| TEST-06 | RS485 ESP32->STM32 | RS485 Bus | Send test packet from ESP32 | STM32 receives valid packet | CRC matches, data correct |
| TEST-07 | RS485 DE/RE | RS485 Transceiver | Toggle DE/RE pins | Half-duplex direction switches | TX/RX works cleanly |
| TEST-08 | Nextion TX | HMI UART | Send command to display | Nextion screen updates | UI reflects sent data |
| TEST-09 | Nextion RX | HMI UART | Press button on screen | Controller receives UART event | Correct button ID parsed |
| TEST-10 | DIP Switch | DIP Switch GPIO | Toggle switches | GPIO state changes detected | Software reads correct ID |
| TEST-11 | Zero Cross EXTI | Zero Cross Circuit | AC main sync pulse | EXTI interrupt fires | Frequency calculated correctly |
| TEST-12 | Heater output loopback | Heater Control | Turn on heater PWM/GPIO | Loopback confirms state | Output state matches command |
| TEST-13 | TRIAC output loopback | TRIAC Control | Turn on TRIAC | Loopback confirms state | Triac fires correctly |
| TEST-14 | Timer start | Software Timer | Start countdown | Timer begins decrementing | Initial value set correctly |
| TEST-15 | Timer expiration | Software Timer | Wait for timer to reach 0 | Timer callback/event fires | Time elapsed is accurate |
| TEST-16 | Timer -> TRIAC OFF | System Logic | Timer expires | TRIAC drive disabled | TRIAC pin goes low |
| TEST-17 | Timer -> Heater OFF | System Logic | Timer expires | Heater drive disabled | Heater pin goes low |
| TEST-18 | Feedback confirmation | System Logic | Actuator ON | Feedback signal verifies ON | Feedback matches intended state |
| TEST-19 | State Machine | Core Logic | Various transitions | System moves through states | Correct state machine flow |
| TEST-20 | Fault handling | Core Logic | Inject fault (e.g., sensor loss) | System enters safe state | Outputs disabled, error logged |
