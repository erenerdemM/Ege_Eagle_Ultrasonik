> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONİK — Donanım ve Yazılım Haritası (Hardware & Software Map)

> **Doküman Statüsü:** Technical Master Specification  
> **Tarih:** 10 Ağustos 2026

---

## 1. STM32G474RE Pin Haritası (Pin Mapping Table)

| MCU | Pin | Peripheral | Direction | Function / Signal Name | Active State | Used By Module |
| --- | --- | --- | --- | --- | --- | --- |
| **STM32** | `PC6` | GPIO / TIM15 | Output | `TRIAC_GATE_Pin` (Triyak Tetikleme Gate) | High | `ultrasonic_pwm.c` |
| **STM32** | `PC7` | EXTI / GPIO | Input | `ZERO_CROSS_Pin` (EXTI9_5 Yükselen Kenar ZC) | High Edge | `ultrasonic_pwm.c` |
| **STM32** | `PB15`| GPIO | Output | `HEATER_RELAY_Pin` (Isıtıcı Röle Kontrol) | High | `heater_relay.c` |
| **STM32** | `PC8` | GPIO | Input | `DIP_SW1_Pin` (DIP Switch Bit 0) | Low (Pull-up) | `main.c` (`ReadDipSwitchId`) |
| **STM32** | `PC9` | GPIO | Input | `DIP_SW2_Pin` (DIP Switch Bit 1) | Low (Pull-up) | `main.c` (`ReadDipSwitchId`) |
| **STM32** | `PC10`| USART3_TX / GPIO | Output | `DIP_SW3_Pin` / USART3 TX (Multi-Drop Bus) | High | `esp32_uart.c`, `main.c` |
| **STM32** | `PC11`| USART3_RX / GPIO | Input | `DIP_SW4_Pin` / USART3 RX (Multi-Drop Bus) | High | `esp32_uart.c`, `main.c` |
| **STM32** | `PA2` | LPUART1_TX | Output | `LPUART1_TX_Pin` (ST-Link VCP COM11 Echo) | High | `main.c`, `esp32_uart.c` |
| **STM32** | `PA3` | LPUART1_RX | Input | `LPUART1_RX_Pin` (ST-Link VCP COM11 RX) | High | `main.c` |
| **STM32** | `PB12`| GPIO | Output | `X9C_CS_Pin` (Digital Pot Chip Select) | Active Low | `x9c103s.c` |
| **STM32** | `PB13`| GPIO | Output | `X9C_UD_Pin` (Digital Pot Up/Down Dir) | High=UP, Low=DOWN | `x9c103s.c` |
| **STM32** | `PB14`| GPIO | Output | `X9C_INC_Pin` (Digital Pot Increment Pulse) | Falling Edge | `x9c103s.c` |
| **STM32** | OPAMP3 Output | OPAMP3 / ADC2 | Internal | `ADC_CHANNEL_VOPAMP3_ADC2` (PT100 Amplified) | Analog | `pt100_adc.c` |

---

## 2. ESP32-S3 Pin Haritası

| MCU | Pin | Peripheral | Direction | Function / Signal Name | Active State | Used By Module |
| --- | --- | --- | --- | --- | --- | --- |
| **ESP32**| `GPIO18` | Serial1 RX | Input | `STM_RXD` (Multi-Drop Bus RX) | High | `ekran_kontrol.ino` |
| **ESP32**| `GPIO8` | Serial1 TX | Output | `STM_TXD` (Multi-Drop Bus TX) | High | `ekran_kontrol.ino` |
| **ESP32**| `GPIO16` | Serial2 RX | Input | `RXD2` (Nextion HMI RX) | High | `ekran_kontrol.ino` |
| **ESP32**| `GPIO17` | Serial2 TX | Output | `TXD2` (Nextion HMI TX) | High | `ekran_kontrol.ino` |
| **ESP32**| `GPIO4` | GPIO / `esp_timer` | Output | `ZC_SIM_PIN` (100Hz Zero-Cross Simülatör) | Toggle | `ekran_kontrol.ino` |

---

## 3. STM32 Çevrebirim (Peripheral) Haritası

| Peripheral | Instance | Configuration | Purpose | Interrupt? | Used Function |
| --- | --- | --- | --- | --- | --- |
| **ADC** | `ADC2` | 12-Bit Single Conversion, PCLK/4 | PT100 Voltaj Örnekleme | No (Polled) | `HAL_ADC_Start`, `HAL_ADC_PollForConversion`, `HAL_ADC_GetValue` |
| **OPAMP** | `OPAMP3` | PGA Mode, Gain x2, Internal Output | PT100 Voltaj Yükseltme | No | `HAL_OPAMP_Init`, `HAL_OPAMP_Start` |
| **TIM** | `TIM15` | One-Pulse Mode, Prescaler 169 (1MHz) | Triyak Firing Delay + Gate Pulse | Yes (`TIM1_BRK_TIM15_IRQn`) | `HAL_TIM_OC_Init`, `HAL_TIM_OC_Start_IT` |
| **UART** | `USART3` | 115200 8N1, FIFO Disabled | Multi-Drop Bus İletişimi | Yes (`USART3_IRQn`) | `HAL_UART_Receive_IT`, `HAL_UART_Transmit_IT` |
| **LPUART**| `LPUART1` | 115200 8N1 | ST-Link VCP (COM11) Debug Echo | No | `HAL_UART_Transmit` |
| **EXTI** | `EXTI9_5` | `PC7` Rising Edge Interrupt | 50Hz Zero-Cross Yakalama | Yes (`EXTI9_5_IRQn`) | `HAL_GPIO_EXTI_Callback` |
| **Flash** | Flash | Bank 2 Page 127 (`0x0807F800`) | `MY_TANK_ID` Kalıcı Override | No | `HAL_FLASHEx_Erase`, `HAL_FLASH_Program` |

---

## 4. Clock ve Zamanlama Analizi (Clock & Timing Architecture)

### 4.1. STM32 Clock Ağacı Yapılandırması (`SystemClock_Config`)
- **Main Oscillator:** HSI 16 MHz.
- **PLL Configuration:** PLLM = 4, PLLN = 85, PLLR = 2 $\rightarrow$ $\text{SYSCLK} = (16 / 4) \times 85 / 2 = 170 \text{ MHz}$.
- **AHB / APB Clocks:** HCLK = 170 MHz, PCLK1 = 170 MHz, PCLK2 = 170 MHz (Voltage Scale 1 Boost Mode).
- **Flash Latency:** `FLASH_LATENCY_4` (170 MHz operasyonu için gereklidir).
- **TIM15 Clock:** APB2 Timer Clock = 170 MHz. Prescaler = 169 $\rightarrow$ Timer Tick = 1 MHz (1 tick = 1 µs).

### 4.2. Zamanlama Çözünürlük Çizelgesi

| Zaman Ölçeği | Süre / Periyot | Kaynak / Algoritma | Açıklama |
| --- | --- | --- | --- |
| **Microsecond (µs)** | 1 µs | `TIM15` Prescaler | Triyak ateşleme gecikmesi çözünürlüğü |
| **Microsecond (µs)** | 100 µs | `TRIAC_PULSE_WIDTH_US` | Triyak gate pini HIGH kalma süresi |
| **Microsecond (µs)** | 20 µs | `SOFTSTART_RAMP_STEP_US` | Her ZC darbesinde firing delay düşüş adımı |
| **Microsecond (µs)** | 500 µs - 9500 µs | `PowerPctToDelayUs` | %100 - %0 güç arası triyak tetikleme penceresi |
| **Millisecond (ms)** | 10 ms (50 Hz) | AC Yarı Periyot | İki zero-cross arası süre (10.000 µs) |
| **Millisecond (ms)** | 500 ms | Superloop Heartbeat | ESP32'ye `STAT,...` telemetri yayın periyodu |
| **Millisecond (ms)** | 500 ms | `ZERO_CROSS_TIMEOUT_MS` | ZC kesintisi gelmezse `FAULT_ZERO_CROSS_LOST` süresi |
| **Millisecond (ms)** | 3000 ms | `STM_BAGLANTI_TIMEOUT` | ESP32'nin tankı çevrimdışı (offline) ilan etme süresi |
| **Second (s)** | 1 s (1000 ms) | `ProcessTimer_Process` | Geri sayım süreç zamanlayıcı eksiltme periyodu |
