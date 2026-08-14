> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONİK — Phase 3 Adversarial System Review Report

> **Doküman Statüsü:** Lead Embedded Systems Architect & Adversarial Review Output  
> **Tarih:** 10 Ağustos 2026  
> **Metodoloji:** Bağımsız Çapraz Sorgulama, Kod İçi İz Sürümü ve Matematiksel Doğrulama

---

## 1. Giriş ve Çapraz Doğrulama Yaklaşımı

Phase 3 aşamasında, Phase 1 ve Phase 2'de elde edilen tüm bulgular "Çapraz Sorgulama" (Adversarial Review) süzgecinden geçirilmiştir. Hiçbir iddia doğru kabul edilmemiş, `STM32` C kaynak kodları, `ESP32` `.ino` taslağı ve `Python HIL` test altyapısı üzerinden satır satır doğrulanmıştır.

Tüm bulgular 8 kesin sınıftan birine oturtulmuştur:
`CONFIRMED BUG`, `CONFIRMED SAFETY ISSUE`, `CONFIRMED RELIABILITY ISSUE`, `DESIGN LIMITATION`, `PERFORMANCE ISSUE`, `CODE QUALITY ISSUE`, `FALSE POSITIVE`, `UNVERIFIED`.

---

## 2. Altı Kritik Bulgunun Derinlemesine Sorgulama Sonuçları

### 🔴 1. BENCH_DEV_MODE_ID (`main.c:53`)
- **İddia:** `BENCH_DEV_MODE_ID 1` sabiti aktif bırakıldığı için tüm kartlar ID 1 ile boot eder.
- **Doğrulama Adımları (`main.c:204-215`):**
  ```c
  #if (BENCH_DEV_MODE_ID > 0)
    MY_TANK_ID = BENCH_DEV_MODE_ID;
  #else
    uint8_t override_id = TankId_Load();
    MY_TANK_ID = (override_id != 0U) ? override_id : ReadDipSwitchId();
  #endif
  ```
- **Sorgulama Sonucu:** Kod akışında `#if (BENCH_DEV_MODE_ID > 0)` derleme zamanı makrosu doğrudan `MY_TANK_ID = 1` eşitliğini zorlamakta, DIP switch (`ReadDipSwitchId()`) ve Flash okumasını (`TankId_Load()`) tamamen bypass etmektedir. Üretim derlemesinde bu değer `0` yapılmazsa otobüsteki tüm kartlar `T1` adresini yanıtlayacak ve çakışma yaşanacaktır.
- **Sınıflandırma:** `CONFIRMED BUG` (Severity: `CRITICAL`)

---

### 🔴 2. X9C103S Interrupt Blackout (`x9c103s.c:31`)
- **İddia:** `X9C103S_SetStep` ve `Init` fonksiyonlarındaki `__disable_irq()` 600µs boyunca kesmeleri kapatır.
- **Doğrulama ve Zamanlama Hesabı:**
  - STM32G474RE SYSCLK = 170 MHz.
  - `X9C_DelayUs(us)` döngüsü: `count = us * 45U; for (volatile uint32_t i = 0U; i < count; i++) __asm__ volatile("");`
  - `X9C103S_Init()` içerisinde 100 adım silecek sıfırlaması yapılır. Her adımda `INC` LOW 3µs, HIGH 3µs $\rightarrow$ Adım başı 6µs delay. 100 adım $\times 6\mu s = 600\mu s$ saf gecikme.
  - `__disable_irq()` bloğu 600µs boyunca açık kalır.
- **Sistem İçi Etkileri:**
  - **UART Veri Kaybı:** 115200 Baud hızında 1 byte iletim süresi $\approx 86.8\mu s$'dir. STM32 USART3 donanımsal FIFO'su pasif edildiğinden (`DisableFifoMode`), en fazla 2 byte alınabilir. 600µs boyunca 7 byte veri gelir ve donanımsal **Overrun Error (ORE)** oluşur! `HAL_UART_ErrorCallback` tetiklenerek partial paket silinir (`rx_index = 0`).
  - **Triyak Ateşleme Sapması:** 50Hz zero-cross yarı-periyodu 10.000µs'dir. 600µs kesme karartması yarı periyodun %6'sına (%10.8° faz açısına) denk gelir. Zero-cross EXTI kesmesi 600µs gecikmeli işleneceğinden triyak yanlış faz açısıyla ateşlenir.
- **Sınıflandırma:** `CONFIRMED BUG` (Severity: `HIGH`)

---

### 🟠 3. 50Hz AC Şebeke Varsayımı (`ultrasonic_pwm.c:16`)
- **İddia:** Sabit `AC_HALF_CYCLE_US 10000UL` ve `TRIAC_MAX_DELAY_US 9500UL` 60Hz şebekede çöker.
- **Doğrulama ve Matematik:**
  - 50 Hz şebeke: Yarı periyot = 10.000 µs. Max gecikme (min güç) = 9.500 µs. (Geçerli)
  - 60 Hz şebeke: Yarı periyot = 8.333 µs. Max gecikme = 9.500 µs.
- **Sistem İçi Etkileri:**
  - 60Hz şebekede setpoint düşük güç verildiğinde TIM15 sayacı 9500µs'ye kurulur. Ancak t=8333µs anında bir sonraki zero-cross EXTI kesmesi gelir!
  - `HAL_GPIO_EXTI_Callback` çağrılarak `__HAL_TIM_SET_COUNTER(&htim15, 0)` yapılır ve sayıcı 9500µs compare değerine ulaşamadan sıfırlanır!
  - Sonuç: Düşük güç bölgelerinde (%0-15 güç aralığı) triyak **hiç ateşlenmez**.
- **Sınıflandırma:** `CONFIRMED RELIABILITY ISSUE` (Severity: `HIGH`)

---

### 🟠 4. ESP32 Boot Watchdog Bypass (`ekran_kontrol.ino:79`)
- **İddia:** `stm_son_veri_zamani` başlangıçta 0 olduğu için boot sonrası 3 saniye kartlar "bağlı" sanılır.
- **Doğrulama:**
  - `stm_son_veri_zamani[goz_id]` global dizisi varsayılan `0` olarak açılır.
  - `isKartBagli(1)`: `return (millis() - 0) < 3000;`
  - Boot anında t=100ms için $100 - 0 = 100 < 3000 \rightarrow$ **TRUE**.
  - `baslatmaEngelliMi()`: `if (isKartBagli(secili_goz)) return false;` $\rightarrow$ Engelleme **DEVRE DIŞI**.
- **Sorgulama:** Fiziksel olarak STM32 kartı takılı olmasa dahi ilk 3 saniyede HMI'den START verilirse ESP32 otobüse START komutunu yayınlar.
- **Sınıflandırma:** `CONFIRMED SAFETY ISSUE` (Severity: `HIGH`)

---

### 🔴 5. Donanımsal Watchdog (IWDG) Eksikliği ve Thermal Runaway (`main.c:172`)
- **Sorgulama:** "CPU kilitlenirse röle ne durumda kalır?"
- **Fiziksel İnceleme:**
  - STM32 reset atılmadığı veya güç kesilmediği sürece kilitlenme anında GPIO pinlerinin çıkış durumunu korur.
  - Isıtıcı röle pini `PB15` (`HEATER_RELAY_Pin`), `RelaySet(1)` modundayken CPU kilitlenirse (ör. kilitli döngü veya HardFault) `PB15` sürekli **HIGH** kalır.
  - Donanımsal IWDG aktif edilmediği için kart kendini resetleyemez. Akışkan kaynar, ısıtıcı rezistans yanar (Thermal Runaway).
- **Sınıflandırma:** `CONFIRMED SAFETY ISSUE` (Severity: `CRITICAL`)

---

### 🟡 6. Sınırsız Evrensel Broadcast (`T0:SET_ID`) Risk Analizi (`esp32_uart.c:98`)
- **İddia:** `T0:SET_ID` komutu çalışma esnasında da işlenir.
- **Doğrulama (`esp32_uart.c:168`):**
  - `SET_ID` komutunda `TankId_SaveOverride()` çağrılır. Bu fonksiyon `HAL_FLASHEx_Erase()` ile Flash Bank 2 Page 127'yi siler ve yeniden yazar.
  - İşlem esnasında CPU 20-40ms bloklanır.
  - Makine `SYS_MODE_RUNNING` modundayken hatalı veya gürültülü bir `T0:SET_ID:2` komutu düşerse, çalışan kartın ID'si anında değişir, otobüste Tank 1 kaybolur ve ısıtıcı denetimsiz çalışmaya devam eder.
- **Sınıflandırma:** `CONFIRMED BUG` (Severity: `HIGH`)

---

## 3. ESP32 Dynamic String ve Heap Analizi

- **Telemetri İşleme Sıklığı:** 10 tanklı otobüste saniyede ~20 telemetri paketi alınır.
- **Bellek Tahsis Sayısı:** Her pakette `stmTelemetryIsle` içerisinde 8 adet `String.substring()` ve `hatOku` içerisinde byte-byte `tampon += c` tahsisi yapılır. Saniyede $\approx 180$ adet dinamik `malloc`/`free` işlemi gerçekleşir.
- **Değerlendirme:** ESP32-S3 Arduino FreeRTOS heap yapısında bu yoğunluk bir süre sonra bellek parçalanmasına (heap fragmentation) yol açar. Ancak cihaz anında çökmeyip birkaç gün içinde reset atacaktır.
- **Sınıflandırma:** `CODE QUALITY ISSUE` / `RELIABILITY RISK` (Severity: `MEDIUM`)