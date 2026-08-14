> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# STM32 Firmware Derinlemesine İnceleme ve Güvenlik Analiz Raporu

> **Doküman Statüsü:** Lead Embedded Systems Engineer Firmware Audit Output  
> **Tarih:** 10 Ağustos 2026  
> **Hedef Platform:** STM32G474RETX (ARM Cortex-M4, 170 MHz, 512KB Flash, 128KB SRAM)

---

## 1. Alt Sistem Bazlı Analiz Özeti

### 1.1. Main & System Initialization Flow (`main.c`)
- **Clock Yapılandırması:** HSI (16 MHz) + PLL ile 170 MHz SYSCLK, Voltage Scale 1 Boost modunda çalışmaktadır. `FLASH_LATENCY_4` olarak doğru ayarlanmıştır.
- **Kimlik Mantığı (ID Management):**
  - Boot sırasındaki öncelik: `BENCH_DEV_MODE_ID` -> Flash Override (`0x0807F800`) -> DIP Switch (`PC8..PC11`).
  - **Tespit Etme:** Kodda `#define BENCH_DEV_MODE_ID 1` aktif durumdadır. Bu durum, DIP switch ve Flash ayarlarını bypass eder.
- **Flash Override Mekanizması:** Flash Bank 2 Page 127 (`0x0807F800`) double-word (64-bit) modunda programlanmaktadır (`magic: 0xA5A5A5A5 | id`). 

### 1.2. Donanım Sürücüleri ve Periferik Yönetimi

#### A. Triyak Faz Açısı PWM Kontrolü (`ultrasonic_pwm.c`)
- **Zero-Cross EXTI:** `PC7` pininden yükselen kenarda `EXTI9_5_IRQHandler` tetiklenir (`NVIC priority: 1,0`).
- **Zamanlama:** `TIM15` donanımı One-Pulse Mode (OPM) konfigürasyonundadır. 1 tick = 1 µs çözünürlükle çalışır.
- **Ateşleme Mantığı:** Zero-cross EXTI geldiğinde `TIM15` sayacı sıfırlanır, `CCR1` değerine `current_delay_us` yazılır. Gecikme dolunca Output Compare kesmesinde `PC6` High yapılır. ARR periyodu dolunca Update kesmesinde `PC6` Low yapılır ve zamanlayıcı kendini durdurur.
- **Soft-Start:** `current_delay_us` hedef gecikmeye doğru her zero-cross adımında `SOFTSTART_RAMP_STEP_US = 20µs` adımlarla rampalanır.
- **Emniyet:** 500 ms boyunca zero-cross kesmesi gelmezse `FAULT_ZERO_CROSS_LOST` seti verilir ve triyak kapatılır.

#### B. PT100 Sıcaklık Ölçümü ve Analog Katman (`pt100_adc.c`)
- **Sinyal Şartlandırma:** `OPAMP3` (PGA gain = 2x) dahili olarak `ADC2_CHANNEL_VOPAMP3` girişine bağlıdır.
- **Dönüştürme ve Hesaplama:** 12-bit ADC single conversion modunda okuma yapılır. `temp_c = adc_raw * 0.0327 - 20.0`.
- **Sensör Bütünlük Denetimi:**
  - Ray Sınırları: `adc_raw >= 4090` (Açık Devre) veya `adc_raw <= 5` (Kısa Devre).
  - Mantıksal Sınırlar: `-10°C` ile `110°C` sıcaklık aralığına karşılık gelen ham ADC değerlerinin (`ADC_RAW_VALID_MIN` / `MAX`) dışına çıkılması da FAULT olarak işlenir.
  - Hata anında `current_temp_c = 0.0f` değerine zorlanır ve ESP32 tarafında `"--.-"` basılması sağlanır.

#### C. Isıtıcı Röle Kontrolü (`heater_relay.c`)
- **Kontrol Algoritması:** `PB15` pini üzerinden ±1.0°C histerezisli deadband kontrolü uygulanır.
- **Emniyet Kilidi:** `SYS_MODE_RUNNING` dışındaki tüm durumlarda (IDLE veya FAULT) röle zorla kapatılır (`RelaySet(0)`).

#### D. Süreç Zamanlayıcı (`process_timer.c`)
- **Sayım Mantığı:** `SYS_MODE_RUNNING` moduna geçişte `setpoint_time_minutes * 60` geri sayım hafızasına yüklenir.
- **Kayma Önleme (Drift Compensation):** `last_tick_ms += 1000u` ifadesiyle kümülatif zaman kaymasının önüne geçilmiştir.
- **Sıfır Süre Koruması:** `remaining_seconds == 0` ise sistem başlatılmadan IDLE'a düşürülür.

#### E. X9C103S Dijital Potansiyometre Sürücüsü (`x9c103s.c`)
- **Frekans Seçimi:** 28 kHz için 40. adım, 40 kHz için 90. adım ayarlanır.
- **Zamanlama Koruması:** Silecek adımlama sırasındaki microsecond gecikmeler `__disable_irq()` / `__set_PRIMASK()` ile donanım kesmelerinden korunmuştur (Critical Section).

---

## 2. Tespit Edilen Firmware Riskleri ve Problemler

### 2.1. Kritik ve Yüksek Seviyeli Riskler

1. **Geliştirici Modu Bayrağı Üretim Unutkanlığı Risk (`main.c:53`)**:
   - Kod parçası: `#define BENCH_DEV_MODE_ID 1`
   - *Etki:* Bu bayrak `1` kaldığı sürece kartlar sahada DIP Switch okumayacak ve hepsi ID 1 olarak açılacaktır. Çoklu kart hattı çökebilir.

2. **Gecikmeli UART Rx Tampon Taşması (Line Buffer Dropping) (`esp32_uart.c:270`)**:
   - Kod parçası: `if (!line_ready) ... else { /* dropped */ }`
   - *Etki:* `line_ready = 1` durumundayken `ESP32_UART_Process()` henüz çağrılmadan otobüsten yeni bir paket gelirse, gelen veriler tamamen düşürülür.

3. **OPAMP3 / ADC2 Dönüşüm Bloklaması (Polling Delay) (`pt100_adc.c:43`)**:
   - Kod parçası: `HAL_ADC_PollForConversion(&hadc2, 10)`
   - *Etki:* Ana süper-döngü her turda ADC dönüşümünü bekler. ADC'de bir donanımsal kilitlenme olursa ana döngü 10ms boyunca bloklanır.

4. **X9C103S NOP Döngüsü Derleyici Optimizasyon Riski (`x9c103s.c:20`)**:
   - Kod parçası: `uint32_t count = us * 45U; for (volatile uint32_t i = 0U; i < count; i++) ...`
   - *Etki:* CPU frekansı değiştiğinde veya `-O3` derleyici optimizasyonlarında `us` süresi sapabilir, potansiyometre adım kaçırabilir.

### 2.2. Bellek ve Taşma/Yarış Koşulları (Race Conditions)

- **`g_system_state` Atomikliği:**
  - Tüm değişken elemanları 32-bit veya daha küçük (`uint16_t`, `float`, `uint8_t`, `enum`) olduğu için ARM Cortex-M4 mimarisinde hizalanmış okuma/yazmalar teker teker atomiktir. Ancak bir modül iki farklı elemanı (ör. `mode` ve `fault_flags`) aynı anda güncellerken kesme araya girerse geçici bir ara-durum oluşabilir.
