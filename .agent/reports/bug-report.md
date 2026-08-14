> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONİK — Detaylı Hata ve Açık Raporu (Bug Report)

> **Doküman Statüsü:** Lead Embedded Systems Engineer Audit Output  
> **Tarih:** 10 Ağustos 2026  
> **Kapsam:** Tüm Kaynak Kodları, Protokol ve Mimaride Tespit Edilen Risk ve Hatalar

---

## 1. Hata Derecelendirme Özeti

| Derece (Severity) | Adet | Açıklama |
|---|:---:|---|
| **CRITICAL** | 1 | Sistemi çalıştırmayan veya sahada felakete yol açabilecek kritik hatalar. |
| **HIGH** | 3 | Veri kaybı, sistem bloklanması veya donanım adımlama hataları. |
| **MEDIUM** | 3 | Gürültü hassasiyeti, paket doğrulama ve mimari iyileştirme ihtiyacı. |
| **LOW** | 2 | Performans optimizasyonları ve kod kirliliği. |
| **INFO** | 1 | Donanımsal watchdog ve geliştirme tavsiyeleri. |

---

## 2. Detaylı Hata Listesi

### 🔴 BUG-CRIT-01: Geliştirici Modu Bayrağının Aktif Bırakılması (`BENCH_DEV_MODE_ID`)
- **Severity:** `CRITICAL`
- **File:** [main.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L53)
- **Line:** 53
- **Function:** `main()`
- **Problem:** `#define BENCH_DEV_MODE_ID 1` bayrağı aktif bırakılmıştır.
- **Evidence:** `main.c` satır 204: `#if (BENCH_DEV_MODE_ID > 0) MY_TANK_ID = BENCH_DEV_MODE_ID; #else ...`
- **Root Cause:** Masa üstü (bench) testleri için yazılan bayrağın derleme öncesi sıfırlanmaması.
- **Potential Impact:** Çoklu kart (multi-drop) hattına bağlanan TÜM STM32 kartları DIP Switch ve Flash ayarlarını göz ardı ederek kendilerini **ID 1** yapacaktır. Hattaki kartlar çakışır ve adresli haberleşme tamamen çöker.
- **Reproduction Scenario:** 2 adet STM32 kartını aynı UART hattına bağlayıp güç verin. İkisi de `T1` telemetrisi yayınlayacak ve otobüste veri çakışacaktır.
- **Recommended Fix:** `#define BENCH_DEV_MODE_ID 0` olarak güncellenmelidir.
- **Verification Method:** Değer 0 yapıldıktan sonra DIP switch 2 pozisyonuna alınıp `STAT,2,...` yayınlandığı doğrulanmalıdır.

---

### 🟠 BUG-HIGH-01: UART RX Tekli Arabellek Düşürme Riski (Single Line Buffer Drop)
- **Severity:** `HIGH`
- **File:** [esp32_uart.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L250)
- **Line:** 250 - 275
- **Function:** `HAL_UART_RxCpltCallback()`
- **Problem:** `line_ready == 1` iken (yani önceki satır `main()` tarafından işlenmeyi beklerken) gelen yeni karakterler ve satırlar dropping edilmekte/silinmektedir.
- **Evidence:** `esp32_uart.c` satır 271: `/* if line_ready is still set ... incoming bytes are dropped */`
- **Root Cause:** Ring Buffer / FIFO dairesel arabellek yerine tek satırlık `rx_line` tamponu kullanılması.
- **Potential Impact:** Yoğun otobüs trafiğinde ESP32'den hızlı arkaya gönderilen komutlar (`SET_TIME`, `SET_TEMP`, `START`) işlenmeden silinebilir.
- **Recommended Fix:** En az 4 elemanlı dairesel bir komut FIFO kuyruğu kurulmalıdır.
- **Verification Method:** 10 ms arayla peş peşe 3 komut gönderilip hepsinin işlendiği doğrulanmalıdır.

---

### 🟠 BUG-HIGH-02: ESP32 HMI Olay İşleyicide Bloklayıcı `delay()` Kullanımı
- **Severity:** `HIGH`
- **File:** [ekran_kontrol.ino](file:///C:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino#L484)
- **Line:** 484, 581
- **Function:** `komutIsle()`
- **Problem:** HMI buton görsel efekti için `delay(400)` ve `delay(600)` blocking çağrıları yapılmıştır.
- **Evidence:** `nextionGonder("b_save.bco=2016"); delay(400); nextionGonder("b_save.bco=50712");`
- **Root Cause:** Animasyon geri dönüşünün non-blocking zamanlayıcı yerine bloklayıcı bekleme ile yapılması.
- **Potential Impact:** Kullanıcı SAVE butonuna bastığında ESP32 ana döngüsü 600 ms boyunca donar. Bu esnada STM32'den gelen telemetri paketleri UART FIFO'sunda birikir veya zaman aşımına yaklaşır.
- **Recommended Fix:** `millis()` tabanlı non-blocking bir görsel durum makinesi kullanılmalıdır.
- **Verification Method:** SAVE butonuna basılırken Serial1 okumasının kesintisiz aktığı izlenmelidir.

---

### 🟠 BUG-HIGH-03: X9C103S NOP Döngüsü Zamanlama Riski (Uncalibrated Software Delay)
- **Severity:** `HIGH`
- **File:** [x9c103s.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/x9c103s.c#L18)
- **Line:** 18 - 25
- **Function:** `X9C_DelayUs()`
- **Problem:** `for (volatile uint32_t i = 0U; i < count; i++) __asm__ volatile("");` kaba döngüsü.
- **Evidence:** `count = us * 45U;` sabiti derleyici optimizasyon seviyesine (`-O0`, `-O2`, `-O3`) duyarlıdır.
- **Root Cause:** Mikrosaniye seviyesinde zamanlama için donanımsal zamanlayıcı veya DWT (Data Watchpoint and Trace) register'ı yerine CPU döngüsü sayılması.
- **Potential Impact:** Farklı derleyici seviyelerinde pin zamanlamaları X9C103S entegresinin minimum $t_{INC}$ ($1\mu s$) veya $t_{CPH}$ ($10\mu s$) sürelerini ihlal edebilir ve potansiyometre adım kaçırabilir.
- **Recommended Fix:** `DWT->CYCCNT` (Cortex-M4 Cycle Counter) donanımsal mikro-saniye gecikmesi kullanılmalıdır.
- **Verification Method:** Osiloskop ile INC pini darbe genişliği ölçülmelidir.

---

### 🟡 BUG-MED-01: PT100 ADC Okumasında Dijital Filtre Eksikliği
- **Severity:** `MEDIUM`
- **File:** [pt100_adc.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/pt100_adc.c#L34)
- **Line:** 34 - 68
- **Function:** `PT100_ADC_Process()`
- **Problem:** ADC okuması tek bir anlık numune üzerinden yapılmakta, hareketli ortalama (moving average) filtresi kullanılmamaktadır.
- **Evidence:** `adc_raw = HAL_ADC_GetValue(&hadc2);` ham verisi doğrudan sıcaklığa dönüştürülmektedir.
- **Root Cause:** Sinyal gürültü bastırma yazılım katmanının eklenmemiş olması.
- **Potential Impact:** Triyak veya anahtarlamalı güç kaynaklarının oluşturduğu elektriksel gürültü anlık spike'lara yol açarak sıcaklığın aniden atlamasına veya yanlış arıza tetiklenmesine neden olabilir.
- **Recommended Fix:** En az 8 numunelik Moving Average veya Median filtre uygulanmalıdır.
- **Verification Method:** ADC girdisine gürültü eklenip okunan sıcaklığın kararlılığı test edilmelidir.

---

### 🟡 BUG-MED-02: Otobüs Haberleşmesinde CRC/Checksum Eksikliği
- **Severity:** `MEDIUM`
- **File:** [esp32_uart.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c) & [ekran_kontrol.ino](file:///C:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino)
- **Line:** Genel Haberleşme Katmanı
- **Problem:** Komut ve telemetri paketlerinde hiçbir hata kontrol kodu (CRC8/XOR Checksum) bulunmamaktadır.
- **Evidence:** `T1:SET_POWER:50\n` düz ASCII string olarak iletilmektedir.
- **Root Cause:** Protokolün basitleştirilmiş ASCII yapıda tasarlanması.
- **Potential Impact:** Hatta oluşabilecek 1-bit gürültü (örneğin 50 değerinin 90'a dönüşmesi) fark edilmeden kabul edilebilir.
- **Recommended Fix:** Paket sonuna 2 karakterlik Hex XOR Checksum (ör. `*4E\n`) eklenmelidir.
- **Verification Method:** Bozuk paketler gönderilip reddedildiği doğrulanmalıdır.

---

### 🟡 BUG-MED-03: `max_goz_sayisi` Sınır Aşımı Riski
- **Severity:** `MEDIUM`
- **File:** [ekran_kontrol.ino](file:///C:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino#L568)
- **Line:** 568 - 571
- **Function:** `komutIsle()` (`MAX_UP`)
- **Problem:** `max_goz_sayisi` artırılırken `MAX_GOZ - 1` (10) sınırlandırılması yapılmış olsa da dizi boyutlandırması hassastır.
- **Evidence:** `if (max_goz_sayisi < MAX_GOZ - 1) max_goz_sayisi++;`
- **Root Cause:** Dizi sınırlarının tek bir kütüphane sabitiyle sıkı bağlı olması.
- **Recommended Fix:** `MAX_GOZ` sınırı için macro korumaları güçlendirilmelidir.

---

### 🟢 BUG-LOW-01: Main Loop İçi Bloklayıcı ADC Polling Çağrısı
- **Severity:** `LOW`
- **File:** [pt100_adc.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/pt100_adc.c#L43)
- **Line:** 43
- **Function:** `PT100_ADC_Process()`
- **Problem:** `HAL_ADC_PollForConversion(&hadc2, 10)` çağrısı ana döngüde polling yapmaktadır.
- **Impact:** Süper-döngü süresine gereksiz mikrosaniye gecikmeleri ekler.
- **Recommended Fix:** ADC okuması kesme (IT) veya TIMER+ADC tetikleme moduna geçirilmelidir.

---

### 🟢 BUG-LOW-02: Eski PIC Kodu Kalıntısı
- **Severity:** `LOW`
- **File:** `STM32/Ultrasonik_G4_Master/Eski_PIC_Kodlari/500W_Display.mbas`
- **Problem:** Kullanılmayan MikroBasic kaynak dosyası depoda durmaktadır.
- **Impact:** Kod kirliliği.

---

### 🔵 BUG-INFO-01: STM32 Donanımsal Watchdog (IWDG) Eksikliği
- **Severity:** `INFO`
- **File:** [main.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c)
- **Problem:** Donanımsal `IWDG` çevre birimi aktif edilmemiştir.
- **Impact:** Ağır ESD gürültüsünde MCU kilitlenirse otomatik reset atılamaz.
- **Recommended Fix:** Donanımsal IWDG (1 saniye zaman aşımlı) aktif edilmeli ve süper-döngüde resetlenmelidir.
