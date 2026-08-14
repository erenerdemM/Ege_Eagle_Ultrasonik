# EAGLEULTRASONİK — PHASE 6.2 PHYSICAL TEST PROCEDURE

## ⚠️ SAFETY FIRST
- **NO 220V AC MAINS VOLTAGE IS PERMITTED.**
- All tests are performed under isolated 5V USB / low-voltage DC power.
- All bench loopback lines use 1kΩ series protection resistors.

---

## TEST SEQUENCE (TEST 0 TO TEST 13)

### TEST 0: POWER-OFF CONTINUITY & ISOLATION
- **Setup:** Disconnect all USB power cables.
- **Action:** Measure resistance between ground pins and between 5V rails using a multimeter.
- **Measurement Points:**
  - Nucleo GND $\leftrightarrow$ ESP32 GND $\leftrightarrow$ Nextion GND $\leftrightarrow$ MAX485 GND ($R < 0.5\Omega$).
  - Nucleo 5V $\leftrightarrow$ ESP32 5V $\leftrightarrow$ Nextion 5V ($R = \infty$, Open Circuit).
  - RS485 Line A $\leftrightarrow$ Line B ($R \approx 60\Omega$, $120\Omega \parallel 120\Omega$).
  - X9C Pin 3 (VH) $\leftrightarrow$ Nucleo 3.3V ($R < 0.5\Omega$).
- **Pass Criteria:** Common GND continuity confirmed, 5V rails 100% isolated, bus termination $60\Omega$, VH tied to 3.3V.

### TEST 1: STM32 STANDALONE POWER-ON
- **Setup:** Connect Nucleo USB cable (USB #1).
- **Measurement Points:** Nucleo 5V pin ($5.0\text{V} \pm 0.25\text{V}$) and 3.3V pin ($3.3\text{V} \pm 0.1\text{V}$).
- **Pass Criteria:** Power LEDs illuminated, voltages within range.

### TEST 2: ESP32 STANDALONE POWER-ON
- **Setup:** Connect ESP32 USB cable (USB #2).
- **Measurement Points:** ESP32 5V pin ($5.0\text{V} \pm 0.25\text{V}$) and Common GND offset ($0.0\text{mV}$).
- **Pass Criteria:** ESP32 boots cleanly without brownout.

### TEST 3: DIP SWITCH ADDRESS DECODING
- **Setup:** Toggle DIP switches 1-4.
- **Measurement Points:** PC8, PC9, PC10, PC11 logic levels.
- **Pass Criteria:** SW1-4 = 0001 yields Tank ID = 1; SW1-4 = 0010 yields Tank ID = 2.

### TEST 4: X9C DIGITAL POTENTIOMETER CONTROL
- **Setup:** Connect logic analyzer to PB12 (CS), PB13 (U/D), PB14 (INC).
- **Action:** Issue `X9C103S_SetStep(40)` command.
- **Pass Criteria:** CS goes LOW, U/D sets direction, INC pulses 40 times.

### TEST 5: X9C WIPER VW / PA0 ADC READING
- **Setup:** Connect multimeter to X9C Pin 5 (VW).
- **Action:** Step X9C from min to max resistance.
- **Pass Criteria:** Voltage sweeps smoothly from $0.0\text{V}$ to $3.3\text{V}$ (never exceeding 3.3V). PA0 ADC telemetry tracks voltage.

### TEST 6: HEATER RELAY LOOPBACK
- **Setup:** Bench loopback PB15 $\to$ 1k$\Omega$ $\to$ PA4.
- **Action:** Command Heater ON / OFF.
- **Pass Criteria:** OFF: PB15 = 0.0V, PA4 readback = 0; ON: PB15 = 3.3V, PA4 readback = 1.

### TEST 7: TRIAC GATE CONTROL LOOPBACK
- **Setup:** Bench loopback PC6 $\to$ 1k$\Omega$ $\to$ PA6.
- **Action:** Set setpoint power to 50%.
- **Pass Criteria:** PC6 outputs soft-start pulse stream ($20\text{k-}40\text{kHz}$), PA6 detects pulses.

### TEST 8: TIM1 / TIM15 TIMER AUDIT
- **Action:** Inspect oscilloscope on PA8 (TIM1_CH1) and interrupt counter for TIM15.
- **Pass Criteria:** TIM1 generates hardware PWM, TIM15 handles one-pulse firing delay.

### TEST 9: ESP32 100Hz ZERO-CROSS SIMULATION
- **Setup:** Scope on ESP32 GPIO4.
- **Pass Criteria:** Stable **100 Hz square wave** ($10.0\text{ ms}$ period, 50% duty cycle, 3.3V logic level).

### TEST 10: STM32 PC7 EXTI7 INTERRUPT SYNC
- **Setup:** Connect ESP32 GPIO4 $\to$ 1k$\Omega$ $\to$ STM32 PC7.
- **Pass Criteria:** EXTI7 rising edge interrupt fires 100 times per second, re-arming TIM15.

### TEST 11: NEXTION HMI COMMUNICATION
- **Setup:** ESP32 GPIO17/16 connected to Nextion display.
- **Pass Criteria:** 9600 baud communication OK, touch commands parsed, dual-buffer UI updates smoothly.

### TEST 12: STM32 ↔ ESP32 RS485 BIDIRECTIONAL BUS
- **Setup:** Differential lines A/B connected between MAX485 #1 and MAX485 #2.
- **Pass Criteria:** 115200 baud ASCII unicast (`T1:START`) and 10-field CSV telemetry (`STAT,1,2,...`) exchanged error-free. DE/RE timing verified.

### TEST 13: END-TO-END SYSTEM INTEGRATION
- **Setup:** Complete system interconnected.
- **Pass Criteria:** DIP switch sets Tank ID $\to$ HMI displays node $\to$ User presses START on HMI $\to$ STM32 enters RUNNING mode $\to$ Heater & Triac outputs activate $\to$ Telemetry updates HMI in real time.
