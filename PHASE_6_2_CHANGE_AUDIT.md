# EAGLEULTRASONİK — PHASE 6.2 CHANGE AUDIT

## 1. Overview of Code Modifications
This document records all code changes made during Phase 6.2 to resolve blocking bugs and establish full hardware observability.

---

## 2. Itemized Change Log

### CHANGE 1 — ISR Deadlock Removal
- **File:** [`STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L531-L536)
- **Category:** FIRMWARE (STM32)
- **Old Behavior:** `while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_TC) == RESET)` inside `HAL_UART_TxCpltCallback()` caused an infinite loop in the ISR because HAL already cleared TC flag before executing the callback.
- **New Behavior:** Removed the redundant `while` loop; `RS485_RX_ENABLE()` is called immediately to restore receive mode.
- **Hardware Impact:** Prevents MCU ISR deadlock on RS485 transmit.
- **Firmware Impact:** Smooth RS485 direction switching without thread/ISR lockups.
- **Test Impact:** Fixes RS485 communication timeout errors.

### CHANGE 2 — PA0 ADC1 Channel Initialization
- **File:** [`STM32/Ultrasonik_G4_Master/Core/Src/main.c`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L349-L524)
- **Category:** FIRMWARE (STM32)
- **Old Behavior:** `MX_ADC1_Init()` was missing, leaving PA0 (`ADC1_IN1`) uninitialized.
- **New Behavior:** Added `hadc1` declaration, `MX_ADC1_Init()` function definition, and `MX_ADC1_Init()` call in `main()`.
- **Hardware Impact:** Enables ADC conversion on PA0 connected to X9C wiper $V_W$.
- **Firmware Impact:** Closed-loop wiper voltage feedback is operational.
- **Test Impact:** Enables PA0 ADC telemetry assertions in bench test suite.

---

## 3. Component & Pinout Audit Summary
- **MOC Component Identity:** Confirmed as **MOC3021**. No references to MOC4021 exist in project files.
- **PA0 / PA1:** PA0 = ADC1_IN1 (X9C Wiper), PA1 = OPAMP3_VINP (PT100 Temperature Sensor).
- **PC8-11:** Confirmed initialized as active-low inputs with internal pull-up resistors in `main.c` (lines 815-819).
- **PB12-14:** Confirmed initialized as push-pull outputs in `main.c` (lines 801-806).
