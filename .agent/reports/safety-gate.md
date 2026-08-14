> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# Phase 4 Safety Gate Analysis

## 1. BUG-CRIT-01 (BENCH_DEV_MODE_ID)
- **Trace**: `main.c:53`, `MY_TANK_ID`, DIP SW, Flash ID, UART address.
- **Answer**: Yes. If the exact same firmware is flashed to 10 STM32 boards, they ALL boot with `MY_TANK_ID = 1` because `#define BENCH_DEV_MODE_ID 1` skips both Flash and DIP reads. This causes immediate UART collisions on the shared bus.

## 2. SEC-001 (IWDG / HEATER SAFETY)
- **Trace**: MCU lockup -> main loop stops -> heater control stops.
- **Answer**: Yes. If the MCU locks up due to EMI or faults, the hardware watchdog (IWDG) is unconfigured. The GPIO state (PB15) is held latching the heater relay ON, potentially leading to boil-off and thermal runaway.

## 3. BUG-NEW-01 (X9C103S INTERRUPT BLACKOUT)
- **Trace**: `X9C103S_SetStep()`, 170MHz, UART 115200.
- **Answer**: A blackout of ~600us occurs inside `__disable_irq()`. For UART at 115200 baud (86.8us/byte), any message longer than 2 bytes (173us) will overflow the USART3 RX FIFO, causing an Overrun Error (ORE). Zero-cross EXTI edges will also be missed, causing phase timing jitter.

## 4. ALG-001 (TRIAC / 50Hz / 60Hz)
- **Trace**: `ultrasonic_pwm.c` timer EXTI.
- **Answer**: Hardcoded for 50Hz (10000us half-cycle). On a 60Hz mains (8333us half-cycle), delays >8333us will be reset by the next zero-cross EXTI *before* firing. The triac will never fire in low-power modes (0-15%). 

## 5. ESP32-BUG-01 (ESP32 BOOT STATE)
- **Trace**: `stm_son_veri_zamani[]`, `isKartBagli()`
- **Answer**: Yes. Since `stm_son_veri_zamani` initializes to 0, `millis() - 0 < 3000` evaluates to true for the first 3 seconds. The master considers offline tanks as connected and START commands can be successfully issued to ghost tanks.

## 6. SEC-003 (T0:SET_ID)
- **Trace**: `esp32_uart.c`, `ProcessLine()`, Flash erase block.
- **Answer**: A broadcast `T0:SET_ID` is accepted in RUNNING mode. This triggers a 20-40ms Flash erase block, freezing zero-cross interrupts and corrupting power control states during active ultrasonics operation.

## 7. BUG-HIGH-01 (UART BUFFER)
- **Trace**: `esp32_uart.c`, `rx_line`, `HAL_UART_RxCpltCallback`.
- **Answer**: The single `rx_line` buffer architecture ignores incoming bytes while `line_ready` is 1. Burst commands are overwritten or silently dropped.

## 8. BUG-HIGH-02 (ESP32 BLOCKING DELAY)
- **Trace**: `ekran_kontrol.ino`, `delay(400)`, `delay(600)`.
- **Answer**: Blocking the ESP32 main loop for 1000ms stalls UART reception, causing buffer overflows and dropped telemetry from the 10 STM32 slaves.

## 9. SEC-004 (STM32 COMMUNICATION TIMEOUT)
- **Trace**: STM32 `ProcessTimer_Process`, UART RX lack of timeout.
- **Answer**: The STM32 slave does not implement a heartbeat timeout. If the cable is disconnected during RUNNING, the STM32 keeps the heater and triac ON until the local `remaining_seconds` reaches zero (which could be up to 100 minutes).

## 10. BUG-MED-01 (PT100 FILTER)
- **Trace**: `pt100_adc.c`, single sample float conversion.
- **Answer**: The raw ADC is passed directly to the relay deadband check. Even with hysteresis, lack of digital filtering means significant industrial electrical noise easily exceeds the deadband, causing rapid relay chatter.
