> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONİK — Düzeltme Uygulama Sırası (Implementation Order Roadmap)

> **Doküman Statüsü:** Lead Embedded Systems Architect Output  
> **Tarih:** 10 Ağustos 2026  
> **Yaklaşım:** Risk Tabanlı Aşamalı Düzeltme (Phase 4A -> Phase 4F)

---

## Düzeltme Fazları ve Öncelik Sıralaması

```mermaid
graph TD
    P4A["PHASE 4A: Immediate Safety (P0)\n- BENCH_DEV_MODE_ID = 0\n- Donanımsal IWDG (1000ms)\n- ESP32 Boot Watchdog Fix"]
    P4B["PHASE 4B: Critical Functional & Safety (P1)\n- STM32 Fail-Safe RX Timeout (5s)\n- X9C103S __disable_irq Karartması Kaldırma\n- T0:SET_ID Sadece IDLE Kısıtı"]
    P4C["PHASE 4C: Communication Reliability (P1-P2)\n- ESP32 non-blocking delay()\n- STM32 Ping-Pong / Ring UART Buffer\n- ESP32 Command Pacing (10ms Delay)"]
    P4D["PHASE 4D: Real-Time & Timing (P2)\n- Dynamically Measured AC Zero-Cross Period\n- Soft-Start EXTI Gated Ramping"]
    P4E["PHASE 4E: Performance (P3)\n- PT100 Moving Average Filter (8-sample)\n- Triac Non-Linear Power Lookup Table"]
    P4F["PHASE 4F: Code Quality (P4)\n- NVS max_goz_sayisi Sanitizasyon\n- ESP32 String.reserve() / char buffer"]

    P4A --> P4B
    P4B --> P4C
    P4C --> P4D
    P4D --> P4E
    P4E --> P4F
```

---

## Faz Detayları ve Hedef Dosyalar

### 🚨 PHASE 4A: Immediate Safety (Acil Emniyet - P0)
1. **`BENCH_DEV_MODE_ID` Sıfırlama:** `main.c:53` -> `#define BENCH_DEV_MODE_ID 0`.
2. **IWDG Aktifleştirme:** `main.c` -> `MX_IWDG_Init()` + `HAL_IWDG_Refresh()`.
3. **ESP32 Boot Watchdog Düzeltmesi:** `ekran_kontrol.ino` -> `isKartBagli()` içine `stm_bagli[goz_id]` kontrolü ekleme.

### 🛡️ PHASE 4B: Critical Functional & Safety (Kritik İşlevsel Emniyet - P1)
1. **STM32 Fail-Safe İletişim Zaman Aşımı:** `esp32_uart.c` -> 5 saniye veri gelmezse `SYS_MODE_IDLE`'a çekme.
2. **X9C103S Kesme Karartması Kaldırma:** `x9c103s.c` -> `__disable_irq()` bloklarını daraltma/kaldırma.
3. **`T0:SET_ID` Yayın Kısıtlaması:** `esp32_uart.c` -> Sadece `SYS_MODE_IDLE` modunda Flash silmeye izin verme.

### 📡 PHASE 4C: Communication Reliability (Haberleşme Kararlılığı)
1. **ESP32 `delay()` Kaldırma:** `ekran_kontrol.ino` -> `millis()` durum makinesi.
2. **STM32 UART Tamponu Güçlendirme:** `esp32_uart.c` -> Ping-pong satır tamponu.
3. **ESP32 Komut Pacing:** `ekran_kontrol.ino` -> Ardışık komutlar arasına 10ms bekleme koyma.

### ⏱️ PHASE 4D: Real-Time & Timing (Gerçek Zamanlı Zamanlama)
1. **Şebeke Frekansı Adaptasyonu:** `ultrasonic_pwm.c` -> EXTI periyot ölçümü.
2. **Soft-Start ZC Senkronizasyonu:** `ultrasonic_pwm.c` -> Rampa düşüşünü EXTI kesmesine bağlama.

### 📊 PHASE 4E: Performance (Performans & Filtreleme)
1. **PT100 Filtresi:** `pt100_adc.c` -> 8 numunelik Moving Average.
2. **Triyak Güç Haritası:** `ultrasonic_pwm.c` -> RMS güç eğrisi LUT.

### 🧹 PHASE 4F: Code Quality (Kod Kalitesi)
1. **NVS Sanitizasyonu:** `ekran_kontrol.ino` -> `max_goz_sayisi` sınır kontrolü.
2. **String Fragmantasyon Önlemi:** `ekran_kontrol.ino` -> C char dizileri veya `reserve()`.
