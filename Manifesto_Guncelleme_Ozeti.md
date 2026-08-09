# Manifesto_V3.md Güncelleme Özeti

> **Güncelleme Tarihi:** 9 Ağustos 2026  
> **Gerçekleştiren Ajanlar:** Main Agent, `STM32_Uzmani` ve `ESP_Ekran_Haberlesmeci`  
> **Hedef Dosya:** [Manifesto_V3.md](file:///c:/Users/ern0e/EAGLEULTRASONiK/Manifesto_V3.md)  

---

## 1. Genel Durum Değerlendirmesi

`STM32` ve `esp32` klasörlerindeki kaynak kodları, kesme fonksiyonları, timer konfigürasyonları ve haberleşme protokolü `AGENTS.md` tanımlarına uygun alt ajanlar ile detaylıca incelenmiştir.

ESP32 Master kodunun (`ekran_kontrol.ino`) Manifesto ile %100 uyumlu olduğu doğrulanmış; STM32 Slave kod tabanında (`main.c`, `main.h`, `ultrasonic_pwm.c`, `esp32_uart.c`, `pt100_adc.c`, `heater_relay.c`) ise kod seviyesinde var olan fakat Manifesto dokümanında eksik veya yüzeysel bırakılmış donanım pin atamaları ve HIL aynalama detayları tespit edilerek [Manifesto_V3.md](file:///c:/Users/ern0e/EAGLEULTRASONiK/Manifesto_V3.md) belgesi doğrudan güncellenmiştir.

---

## 2. Güncellenen ve Eklene Başlıklar / Kurallar

### 🔴 1. Donanım ve Pin Haritası Tablosu Eklendi (§1.2 - Yeni Bölüm)
Kod tabanında tanımlı tüm fiziksel pinler ve çevresel birimlerin net ve tartışmasız bir referansı olarak **Bölüm 1.2** dokümana eklendi:
- **Triyak Gate Kontrol:** `PC6` (`TRIAC_GATE_Pin`, Push-Pull Çıkış)
- **Zero-Cross Girişi:** `PC7` (`ZERO_CROSS_Pin`, EXTI9_5 Yükselen Kenar Kesmesi)
- **Isıtıcı Röle Kontrol:** `PB15` (`HEATER_RELAY_Pin`, Push-Pull Çıkış)
- **DIP Switch 1..4:** `PC8`, `PC9`, `PC10`, `PC11` (`GPIOC`, Aktif-Düşük, Dahili Pull-up)
- **Multi-Drop Bus UART3:** `PC10` (TX), `PC11` (RX) (`USART3`, 115200 8N1)
- **HIL Debug LPUART1:** `PA2` (TX), `PA3` (RX) (`LPUART1`, ST-Link VCP COM11, 115200 8N1)
- **PT100 Sensör Girişi:** OPAMP3 $PGA=2 \rightarrow$ Dahili `ADC_CHANNEL_VOPAMP3_ADC2` kanalı ile ADC2
- **ESP32 UART Pinleri:** `GPIO18` (Bus RX), `GPIO8` (Bus TX), `GPIO16/17` (Nextion HMI RX/TX), `GPIO4` (100Hz ZC Simülatörü)

### 🟡 2. ST-Link VCP (LPUART1 / COM11) Telemetri Aynalama (§1.1 & §4.4)
- STM32'nin `ESP32_UART_SendStatus()` fonksiyonunda `STAT,...` paketlerini sadece `USART3` (ESP32 bus) üzerinden göndermekle kalmayıp, HIL test ortamının izlenebilirliği için `LPUART1` (ST-Link VCP / COM11) portuna **aynaladığı (mirroring)** bilgisi §1.1 ve §4.4'e eklendi.
- Ayrıca `HIL_DeepDebug_Print()` ile her 500ms'de bir `DEBUG_STM: ADC=..., DELAY=..., RELAY=...` ham donanım verisinin basıldığı belgelendi.

### 🟢 3. Triyak TIM15 One-Pulse Mod (OPM) ve Gate Pulse Zamanlaması (§4.2)
- TIM15 zamanlayıcısının $1\mu s$ tick çözünürlüğünde One-Pulse Mode (OPM) ile çalıştığı belirtildi.
- `PC7` EXTI9_5 kesmesinde `CCR1`'e ateşleme gecikmesi (`current_delay_us`), `ARR`'ye darbe sonlanma süresi (`delay + 100us`) yazıldığı eklendi.
- Gate pini `PC6`'nın `HAL_TIM_OC_DelayElapsedCallback` ile `HIGH` yapıldığı, `HAL_TIM_PeriodElapsedCallback` (ARR Update) ile `LOW` yapılarak kesmeyle sonlandırıldığı detaylandırıldı.

### 🔵 4. DIP Switch Pinleri ve Boot Önceliği Açıklığı (§2.1 & §2.2)
- DIP switchlerin `GPIOC` üzerindeki spesifik pin atamaları (`PC8..PC11`) eklendi.
- Boot öncelik sırasındaki en üst kademe olan `#if (BENCH_DEV_MODE_ID > 0)` derleme bayrağının önceliği netleştirildi.

### 🟣 5. Isıtıcı Röle (`PB15`) ve PT100 OPAMP Dahili Kanal Detayı (§4.1 & §4.3)
- Isıtıcı röle pini `PB15` olarak dokümante edildi.
- PT100 okumasında OPAMP3 çıkışının dahili `ADC_CHANNEL_VOPAMP3_ADC2` kanalı üzerinden ADC2 ile okunduğu eklendi.

---

## 3. Sonuç

[Manifesto_V3.md](file:///c:/Users/ern0e/EAGLEULTRASONiK/Manifesto_V3.md) belgesi yapılan edit işlemleriyle STM32 ve ESP32 kod tabanındaki **fiili donanım haritasını, kesme yapılarını ve haberleşme davranışını %100 yansıtır hale getirilmiştir**.
