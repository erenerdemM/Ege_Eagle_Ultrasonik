> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONİK — Kök Neden Analizi (Root Cause Analysis)

> **Doküman Statüsü:** Lead Embedded Systems Architect Audit Output  
> **Tarih:** 10 Ağustos 2026

---

## Kök Neden Gruplandırma Matrisi

Tüm doğrulanmış bulgular 5 ana kök neden grubunda toplanmıştır:

### ROOT-001: Zamanlama ve Kesme Mimarisi Eksiklikleri (Timing Architecture)
Zaman kritik görevlerin (triyak ateşleme, potansiyometre adımlama, ADC örnekleme) non-blocking donanımsal zamanlayıcılar yerine senkron CPU döngüleri veya kesme karartmaları ile yönetilmesi.
- **İlişkili Bulgular:**
  - `BUG-NEW-01` / `ALG-002` (X9C103S 600µs Kesme Karartması - `x9c103s.c:31`)
  - `ALG-003` (Ana döngü hızına bağlı Soft-Start rampası - `ultrasonic_pwm.c:144`)
  - `ALG-001` (50Hz sabit AC yarı periyot varsayımı - `ultrasonic_pwm.c:16`)
  - `BUG-LOW-01` / `ALG-006` (Senkron ADC Polling - `pt100_adc.c:43`)

### ROOT-002: Haberleşme ve Tampon Dayanıklılığı (Communication Robustness)
UART seri haberleşme katmanında dairesel arabellek (ring buffer), onay (ACK/NACK) ve veri doğrulama (CRC/Checksum) mekanizmalarının bulunmaması.
- **İlişkili Bulgular:**
  - `BUG-HIGH-01` / `PROTO-002` (STM32 Tek Satırlık Arabellek ve Satır Düşürme - `esp32_uart.c:270`)
  - `PROTO-001` (ESP32 Sınırsız String Tamponu - `ekran_kontrol.ino:602`)
  - `PROTO-003` (CRC / Checksum Eksikliği)
  - `PROTO-004` (Açık Çevrim ACK/NACK Eksikliği)
  - `ESP32-BUG-03` (Gecikmesiz Ardışık Komut Gönderimi - `ekran_kontrol.ino:404`)

### ROOT-003: Güvenlik ve Emniyet Mimarisi (Safety Architecture)
Donanım kilitlenmelerinde sistemi güvenli moda düşürecek watchdog ve komut doğrulama kilitlerinin eksik veya hatalı yapılandırılması.
- **İlişkili Bulgular:**
  - `SEC-001` (STM32 Donanımsal IWDG Eksikliği ve Yangın Riski - `main.c:172`)
  - `ESP32-BUG-01` (Boot Anında 3 Saniyelik Watchdog Bypass Hatası - `ekran_kontrol.ino:79`)
  - `SEC-003` (Sınırsız Evrensel Broadcast `T0:SET_ID` Açığı - `esp32_uart.c:98`)
  - `SEC-004` (HMI Bağlantı Kaybı Zaman Aşımı Eksikliği - `esp32_uart.c:64`)

### ROOT-004: Durum ve Yapılandırma Yönetimi (State & Configuration Management)
Geliştirme bayraklarının ve kalıcı hafıza (NVS / Flash) parametrelerinin çalışma zamanında sanitize edilmeden kullanılması.
- **İlişkili Bulgular:**
  - `BUG-CRIT-01` / `SEC-002` (`BENCH_DEV_MODE_ID` Sabit ID Bayrağı - `main.c:53`)
  - `BUG-MED-03` (NVS'den Okunan `max_goz_sayisi` Sınır Sanitizasyon Eksikliği - `ekran_kontrol.ino:43`)

### ROOT-005: Bellek ve Sinyal İşleme Kalitesi (Memory & Signal Processing)
Dinamik bellek kullanımının (String) ve analog sinyal filtrelemesinin (Moving Average) eksikliği.
- **İlişkili Bulgular:**
  - `ESP32-BUG-02` (ESP32 String Kullanımı ve Heap Bölünmesi - `ekran_kontrol.ino:238`)
  - `BUG-MED-01` / `ALG-004` (PT100 Ham ADC Okumasında Filtre Eksikliği - `pt100_adc.c:49`)
  - `BUG-HIGH-02` (ESP32 HMI İşleyicide Bloklayıcı `delay()` Kullanımı - `ekran_kontrol.ino:484`)