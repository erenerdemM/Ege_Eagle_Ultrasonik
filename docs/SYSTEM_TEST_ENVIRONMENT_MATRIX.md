# EAGLEULTRASONİK — SYSTEM TEST ENVIRONMENT MATRIX

---

## 1. Overview

This document specifies the exact physical hardware, software mocks, signal injection resources, interfaces, and prerequisites comprising the test environment for the EAGLEULTRASONiK project.

---

## 2. Hardware Availability Matrix

| Component / Subsystem | Physical Hardware Available? | Test Environment Status | Alternate / Injection Method |
| :--- | :--- | :--- | :--- |
| **STM32 Slave MCU** | **YES** | Available (STM32G474RE Nucleo Board) | Direct hardware execution |
| **ESP32 Master Node** | **YES** | Available (ESP32-S3 Dev Board) | Direct hardware execution |
| **Nextion HMI Display** | **YES** | Available (Nextion 4.3" USART2 Display) | Direct hardware execution / `test_hmi_mock.py` |
| **X9C103S Digital Pot**| **YES** | Available (X9C103S IC Module) | Wiper resistance / voltage readback |
| **RS485 Bus Transceiver**| **YES** | Available (MAX485 / ST485 Hardware Module) | Multi-drop physical serial bus |
| **Raspberry Pi Host** | **YES** | Available (Raspberry Pi Test Host) | Pytest test execution host |
| **Ultrasonic Transducer**| **NO** | **UNAVAILABLE** | Oscilloscope TIM15 PWM duty cycle trace |
| **Ultrasonic Power Card**| **NO** | **UNAVAILABLE** | TIM15 CH1/CH1N PWM signal readback |
| **Physical PT100 Probe**| **NO** | **UNAVAILABLE** | Precision DC Voltage Injection on PA1 OPAMP3 |
| **Heater Relay / Load** | **NO** | **UNAVAILABLE** | Multimeter / Scope PB0 GPIO High/Low trace |
| **AC Zero-Cross Line** | **NO** | **UNAVAILABLE** | Signal generator / 100Hz `esp_timer` simulator |
| **Liquid Tank System** | **NO** | **UNAVAILABLE** | Gated PWM burst modulation trace |

---

## 3. Host Connections, Interfaces & Ports

| Subsystem | Physical Port / Pin | Interface Protocol | Baudrate / Clock | Connected Target |
| :--- | :--- | :--- | :--- | :--- |
| **Raspberry Pi / Host** | `/dev/ttyACM0` or `COM3` | USB-UART (ST-LINK VCP) | 115200 Baud, 8N1 | STM32 USART3 / Debug |
| **Raspberry Pi / Host** | `/dev/ttyUSB0` or `COM4` | USB-UART (CP2102) | 115200 Baud, 8N1 | ESP32 UART0 Debug |
| **ESP32 Master** | GPIO16 (RX), GPIO17 (TX) | Serial UART2 | 115200 Baud, 8N1 | Nextion Display RX/TX |
| **ESP32 Master** | GPIO4 (TX), GPIO5 (RX), GPIO18 (DE) | Half-Duplex RS485 | 115200 Baud, 8N1 | RS485 Bus MAX485 |
| **STM32 Slave** | PB10 (TX), PB11 (RX), PB1 (DE) | Half-Duplex RS485 | 115200 Baud, 8N1 | RS485 Bus MAX485 |
| **STM32 PWM Output** | PB14 (TIM15_CH1), PB15 (TIM15_CH1N) | Complimentary PWM | 20kHz – 40kHz | Oscilloscope Channel 1 & 2 |
| **STM32 Digital Pot** | PA8 (CS), PA9 (U/D), PA10 (INC) | Bit-Bang GPIO Pulses | Microsecond Pulses | X9C103S IC Module |
| **STM32 PT100 Input** | PA1 (OPAMP3_INP), PB0 (OPAMP3_OUT) | Analog Voltage | 0.0V – 3.3V | DC Voltage Injector / DAC |
| **STM32 Zero-Cross** | PB12 (EXTI12) | Edge Interrupt | 100Hz Square Wave | Signal Generator / `esp_timer` |
| **STM32 Heater Output**| PB0 (Heater Relay) | Push-Pull GPIO | 0V / 3.3V Logic | Multimeter / LED Indicator |

---

## 4. Loopback & Injection Resources

| Verification Target | Injection / Loopback Setup | Equipment Used | Measurement Metric |
| :--- | :--- | :--- | :--- |
| **TIM15 PWM Soft-Start**| Connect PB14 to Oscilloscope Ch 1 | Rigol / Siglent DSO | Duty cycle ramping 0% ➔ 100% in 500ms |
| **X9C103S Pot Frequency**| Connect X9C wiper PA4 to Multimeter | Digital Multimeter / DSO | Resistance ($1.05\text{ V} \dots 1.58\text{ V}$ ladder) |
| **PT100 Temperature ADC**| Connect PA1 to Variable DC Source | Precision DC Supply / DAC | Analog voltage 1.25V ➔ 50.0°C linear readback |
| **Zero-Cross Firing** | Connect PB12 to 100Hz Signal Generator| Function Generator | PB0 Triac gate firing delay (0–10ms) |
| **RS485 Direction DE/RE**| Connect PB1 / GPIO5 to Scope Ch 2 | DSO Channel 2 | DE HIGH duration matching TX packet length |
| **Heater Hysteresis** | Vary PA1 ADC voltage around target | Multimeter on PB0 | PB0 toggles HIGH at target - 1.0°C, LOW at target |

---

## 5. Software Mocks & Simulation Engines

| Mock Engine File | Purpose | Simulated Components | Pass Criteria |
| :--- | :--- | :--- | :--- |
| `test_hil_uart.py` | Real UART HIL Integration | Real STM32 Nucleo & RS485 bus | 20/20 Pytest cases PASSED |
| `test_hmi_mock.py` | Nextion HMI Protocol Mock | Nextion serial parser, dual-buffer, NVS | 22/22 Pytest cases PASSED |
| `test_rs485_mock.py` | Multi-Drop Bus Collision Mock| Multi-tank addressing ($T1 \dots T10$), CRC errors| 26/26 Pytest cases PASSED |
| `id_full_lifecycle_test.py` | Provisioning Lifecycle Test | Provisioning state machine, Flash commit | Full lifecycle PASSED |
| `heater_triac_bench_test.c`| Triac Bench Test Harness | Zero-cross EXTI & Triac pulse timing | Zero-cross sync verified |

---

## 6. Environment Sanity Check Procedure

Before initiating any automated or manual test execution sequence, perform the following 5-point environment sanity check:

1. **Power Rail Verification:** Measure 3.3V DC and 5.0V DC test points on STM32 Nucleo and ESP32 boards using a multimeter. Ensure voltage is within $\pm 5\%$ tolerance.
2. **Serial Port Enumeration:** Verify USB-UART devices enumerated on host:
   ```bash
   python list_serial_devices.py
   ```
   Confirm `/dev/ttyACM0` (ST-LINK) and `/dev/ttyUSB0` (ESP32) are active.
3. **Clean GCC Build Verification:** Execute STM32 build script:
   ```bash
   bash tools/build_stm32.sh
   ```
   Confirm `Ultrasonik_G4_Master.elf` builds cleanly with 0 errors.
4. **OpenOCD ST-LINK Connection Test:** Verify ST-LINK can connect and halt STM32 MCU:
   ```bash
   openocd -f interface/stlink.cfg -f target/stm32g4x.cfg -c "init; halt; exit"
   ```
5. **Pytest Environment Check:** Verify pytest runner can discover all 3 test suites:
   ```bash
   pytest --collect-only test_hil_uart.py test_hmi_mock.py test_rs485_mock.py
   ```
   Confirm 68 total test cases collected.
