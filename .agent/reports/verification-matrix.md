> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONİK — Doğrulama Matrisi (Verification Matrix)

> **Doküman Statüsü:** Lead Embedded Systems Architect Audit Output  
> **Tarih:** 10 Ağustos 2026

---

| Finding ID | Title / Topic | Previous Agent Severity | Reviewer Classification | Status | Evidence Source | Confidence |
| --- | --- | --- | --- | --- | --- | --- |
| **BUG-CRIT-01** | Geliştirici Modu Bayrağı (`BENCH_DEV_MODE_ID=1`) | CRITICAL | CONFIRMED BUG | **CONFIRMED** | `main.c:53`, `main.c:204` | HIGH |
| **SEC-001** | Donanımsal Watchdog (`IWDG`) Eksikliği | HIGH | CONFIRMED SAFETY ISSUE | **CONFIRMED** | `main.c:172` (IWDG yok) | HIGH |
| **BUG-NEW-01** | X9C103S 600µs Kesme Karartması | HIGH | CONFIRMED BUG | **CONFIRMED** | `x9c103s.c:31`, `x9c103s.c:93` | HIGH |
| **ALG-001** | 50Hz Sabit AC Yarı Periyot Varsayımı (60Hz Sorunu) | HIGH | CONFIRMED RELIABILITY ISSUE | **CONFIRMED** | `ultrasonic_pwm.c:16` | HIGH |
| **ESP32-BUG-01**| ESP32 Boot Esnasında 3sn Watchdog Bypass Hatası | HIGH | CONFIRMED SAFETY ISSUE | **CONFIRMED** | `ekran_kontrol.ino:79` | HIGH |
| **SEC-003** | Sınırsız Evrensel Broadcast (`T0:SET_ID`) | HIGH | CONFIRMED BUG | **CONFIRMED** | `esp32_uart.c:98` | HIGH |
| **BUG-HIGH-01** | STM32 UART Tek Satırlık Arabellek ve Satır Düşürme | HIGH | CONFIRMED RELIABILITY ISSUE | **CONFIRMED** | `esp32_uart.c:270` | HIGH |
| **BUG-HIGH-02** | ESP32 HMI İşleyicide Bloklayıcı `delay()` Kullanımı | HIGH | CONFIRMED RELIABILITY ISSUE | **CONFIRMED** | `ekran_kontrol.ino:484` | HIGH |
| **SEC-004** | HMI Bağlantı Kaybı Zaman Aşımı Eksikliği | HIGH | CONFIRMED SAFETY ISSUE | **CONFIRMED** | `esp32_uart.c:64` | HIGH |
| **ALG-003** | Ana Döngü Hızına Bağlı Soft-Start Rampası | HIGH | CONFIRMED BUG | **CONFIRMED** | `ultrasonic_pwm.c:144` | HIGH |
| **BUG-MED-01** | PT100 Ham ADC Okumasında Filtre Eksikliği | MEDIUM | CONFIRMED RELIABILITY ISSUE | **CONFIRMED** | `pt100_adc.c:49` | HIGH |
| **ESP32-BUG-02**| ESP32 String Kullanımı ve Heap Bölünmesi | MEDIUM | CODE QUALITY ISSUE | **CONFIRMED** | `ekran_kontrol.ino:238` | HIGH |
| **BUG-MED-03** | `max_goz_sayisi` NVS Sanitizasyon Eksikliği | MEDIUM | CONFIRMED BUG | **CONFIRMED** | `ekran_kontrol.ino:131` | HIGH |
| **PROTO-003** | Otobüs Haberleşmesinde CRC/Checksum Eksikliği | MEDIUM | CONFIRMED RELIABILITY ISSUE | **CONFIRMED** | `esp32_uart.c:80` | HIGH |
| **PROTO-004** | Açık Çevrim Komut İletimi (ACK/NACK Eksikliği) | LOW | DESIGN LIMITATION | **CONFIRMED** | `ekran_kontrol.ino:170` | HIGH |
| **ALG-005** | Doğrusal Faz Açısı Güç Hesaplama Varsayımı | LOW | PERFORMANCE ISSUE | **CONFIRMED** | `ultrasonic_pwm.c:35` | HIGH |
| **BUG-LOW-01** | Main Loop İçi Bloklayıcı ADC Polling | LOW | CODE QUALITY ISSUE | **CONFIRMED** | `pt100_adc.c:43` | HIGH |