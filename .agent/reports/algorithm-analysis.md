> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# Algoritma ve Kontrol Döngüleri İnceleme Raporu

> **Doküman Statüsü:** Lead Embedded Systems Engineer Algorithm Audit Output  
> **Tarih:** 10 Ağustos 2026  
> **Kapsam:** Yıkama Makinesi Kontrol Döngüleri, Zamanlama ve Sinyal İşleme Algoritmaları

---

## 1. Tespit Edilen Ana Algoritmalar

### 1.1. Triyak Faz Açısı PWM ve Soft-Start Algoritması (`ultrasonic_pwm.c`)
- **Amacı:** Ultrasonik transdüser grubuna uygulanan şebeke gücünü %0-100 aralığında faz açısı keserek (phase-angle firing) ayarlamak ve ilk açılışta yüksek demoraj akımını önlemek.
- **Girdileri:** `setpoint_power_pct` (0-100%), 50Hz Zero-Cross EXTI yükselen kenar kesmeleri.
- **Çıktıları:** `PC6` gate tetikleme pulse'ları (100 µs), `current_delay_us`.
- **Çalışma Frekansı:** 100 Hz (50 Hz AC şebekenin her iki yarım periyodunda bir).
- **Matematik Modeli:**
  $$\text{Target Delay (µs)} = 9500 - \left(\frac{(9500 - 500) \times \text{power\_pct}}{100}\right)$$
  - Soft-start rampası: Her zero-cross adımında `current_delay_us` en fazla `SOFTSTART_RAMP_STEP_US` (20 µs) azaltılır.
- **Edge Case & Risk:**
  - Zero-cross sinyalinde gürültü/çift tetikleme olursa TIM15 yeniden başlatılır ve faz açısı kayabilir.

### 1.2. PT100 OPAMP3 + ADC2 Dönüşüm ve Sıcaklık Algoritması (`pt100_adc.c`)
- **Amacı:** Tank içi sıvı sıcaklığını okumak, kalibre etmek ve arıza durumlarını tespit etmek.
- **Girdileri:** `ADC2_CHANNEL_VOPAMP3_ADC2` (12-bit ham okuma: 0 - 4095).
- **Çıktıları:** `current_temp_c` (float), `fault_flags` (open/short bitmask).
- **Çalışma Frekansı:** Ana süper-döngü turlama frekansında.
- **Matematik Modeli:**
  $$\text{temp\_c} = (\text{adc\_raw} \times 0.0327) - 20.0$$
- **Edge Case & Risk:**
  - Dönüşümde hareketli ortalama (Moving Average) veya Kalman filtresi **kullanılmamaktadır**. Anlık donanımsal gürültü spikelarında sıcaklık aniden sıçrayabilir.

### 1.3. Isıtıcı Röle Bang-Bang Histerezis Algoritması (`heater_relay.c`)
- **Amacı:** Sıvı sıcaklığını hedef değerde tutmak ve rölenin kararsız/yüksek frekanslı açılıp kapanmasını (chattering) engellemek.
- **Girdileri:** `current_temp_c`, `setpoint_temp_c`.
- **Çıktıları:** `PB15` Röle Kontrol Pin State (HIGH/LOW).
- **Çalışma Frekansı:** Süper-döngü frekansında.
- **Matematik Modeli:**
  $$\text{Relay} = \begin{cases} \text{ON} & \text{eğer } T_{anlik} \le (T_{hedef} - 1.0^\circ\text{C}) \\ \text{OFF} & \text{eğer } T_{anlik} \ge (T_{hedef} + 1.0^\circ\text{C}) \\ \text{Eski Durum} & \text{aksi takdirde (Deadband)} \end{cases}$$

### 1.4. Driftsiz 1Hz Süreç Geri Sayım Zamanlayıcısı (`process_timer.c`)
- **Amacı:** Yıkama süresini hassas saniye adımlarıyla geriye saymak.
- **Matematik Modeli:**
  `last_tick_ms += 1000u;` birikimli ekleme yöntemiyle döngü gecikmelerinden kaynaklanan zaman kayması (drift) tamamen engellenmiştir.

### 1.5. X9C103S Potansiyometre Adımlama Algoritması (`x9c103s.c`)
- **Amacı:** 28 kHz / 40 kHz jeneratör frekansı seçimi için dijital potansiyometre wiper adımını ayarlamak (Step 40 / Step 90).
- **Kritik Davranış:** Wiper sıfırlamada 100 darbe DOWN verilir, ardından hedef adıma UP adımlaması yapılır.
