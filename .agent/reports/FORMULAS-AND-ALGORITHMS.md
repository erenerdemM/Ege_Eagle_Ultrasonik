> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONİK — Formüller ve Algoritmalar Dokümanı

> **Doküman Statüsü:** Mathematical & Algorithm Specification  
> **Tarih:** 10 Ağustos 2026

---

## 1. Kod İçi Matematiksel Formüller Matrisi

| Formula | Variables | Unit | Source File | Line | Purpose |
| --- | --- | --- | --- | --- | --- |
| $T_{c} = (\text{ADC}_{\text{raw}} \times 0.0327) - 20.0$ | $\text{ADC}_{\text{raw}} \in [0, 4095]$ | °C | [pt100_adc.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/pt100_adc.c) | 67 | Ham ADC2 verisinden PT100 sıcaklığını hesaplama |
| $D_{\text{target}} = 9500 - \frac{9000 \times P_{\text{pct}}}{100}$ | $P_{\text{pct}} \in [0, 100]$ | µs | [ultrasonic_pwm.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/ultrasonic_pwm.c) | 43 | Güç yüzdesinden Triyak Firing Delay hesaplama |
| $P_{\text{pct}} = \frac{(9500 - D_{\text{us}}) \times 100}{9000}$ | $D_{\text{us}} \in [500, 9500]$ | % | [ultrasonic_pwm.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/ultrasonic_pwm.c) | 58 | Firing delay'den anlık gerçekleşen gücü hesaplama |
| $T_{x10} = \text{int}(T_{c} \times 10.0)$ | $T_{c}$ (float) | 0.1°C | [esp32_uart.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c) | 217 | Telemetri paketi için sıcaklığı tamsayıya çevirme |
| $T_{\text{sec}} = M_{\text{setpoint}} \times 60$ | $M_{\text{setpoint}} \in [0, 100]$ | saniye | [process_timer.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/process_timer.c) | 27 | Dakika cinsinden süreyi saniyeye çevirme |
| $T_{\text{half\_us}} = \frac{1000000}{F_{\text{hz}} \times 2}$ | $F_{\text{hz}} = 100$ | µs | [ekran_kontrol.ino](file:///C:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino) | 27 | ZC simülatör 100Hz kare dalga yarı periyodu (5000µs) |

---

## 2. Detaylı Algoritma Açıklamaları

### 2.1. PT100 Ölçüm ve Doğrulama Algoritması (`pt100_adc.c`)
1. **Şartlandırma:** PT100 sensöründen gelen voltaj OPAMP3 (PGA gain x2) ile yükseltilir.
2. **Örnekleme:** `hadc2` üzerinden 12-bit tekli okuma yapılır.
3. **Sensör Kopuk/Kısa Devre ve Pencere Kontrolü:**
   - `adc_raw >= 4090` (Açık Devre) veya `adc_raw <= 5` (Kısa Devre).
   - Gerçekçi pencere dışı: `adc_raw < ADC_RAW_VALID_MIN` (-10°C karşılığı) veya `adc_raw > ADC_RAW_VALID_MAX` (110°C karşılığı).
4. Herhangi bir ihlal varsa `fault_flags` seti verilir, mod `SYS_MODE_FAULT` yapılır ve `current_temp_c = 0.0f` zorlanır (ESP32 ekranda `"--.-"` gösterir).

### 2.2. Isıtıcı Röle Bang-Bang Histerezis Algoritması (`heater_relay.c`)
- Mod `SYS_MODE_RUNNING` değilse röle **KAPALI** (`PB15 LOW`).
- `RUNNING` modundayken:
  - $T_{\text{anlik}} \le (T_{\text{hedef}} - 1.0^\circ\text{C}) \rightarrow$ Röle **AÇIK** (`PB15 HIGH`).
  - $T_{\text{anlik}} \ge (T_{\text{hedef}} + 1.0^\circ\text{C}) \rightarrow$ Röle **KAPALI** (`PB15 LOW`).
  - Deadband bölgesi $(T_{\text{hedef}} - 1.0^\circ\text{C} < T_{\text{anlik}} < T_{\text{hedef}} + 1.0^\circ\text{C}) \rightarrow$ Röle **önceki durumunu korur**.

### 2.3. Triyak Faz Açısı PWM ve Soft-Start Algoritması (`ultrasonic_pwm.c`)
1. `PC7` pinindeki her yükselen zero-cross EXTI kesmesinde `last_zero_cross_tick` güncellenir ve `TIM15` sayacı sıfırlanır.
2. `TIM15` `CCR1` değerine `current_delay_us` yazılır. ARR periyodu `delay + 100us` kurulur.
3. `current_delay_us`, hedef `target_delay_us` değerine doğru her döngüde en fazla 20µs (`SOFTSTART_RAMP_STEP_US`) azaltılır (Soft-start rampalama).
4. 500 ms boyunca zero-cross kesmesi gelmezse `FAULT_ZERO_CROSS_LOST` seti verilerek triyak kapatılır.

### 2.4. X9C103S Çift Frekans Kontrol Algoritması (`x9c103s.c`)
- **Init Sıfırlaması:** `UD` pini LOW yapılır, 100 adet `INC` darbesi gönderilerek silecek en alt adıma (Step 0) çekilir.
- **28 kHz Seçimi:** Silecek Step 40'a ilerletilir (40.4 kΩ direnç adımı).
- **40 kHz Seçimi:** Silecek Step 90'a ilerletilir (90.9 kΩ direnç adımı).
- Her adım geçişi mikro-saniye seviyesinde `INC` pini toggle edilerek yapılır.
