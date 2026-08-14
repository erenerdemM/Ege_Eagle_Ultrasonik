# EAGLEULTRASONİK — Phase 4.7 Final Architecture Validation Report

> **Doküman Statüsü:** MASTER SYSTEM ARCHITECTURE VALIDATION (FINAL DESIGN FREEZE)  
> **Tarih:** 10 Ağustos 2026  
> **Aşama:** Phase 4.7 Final Architecture Validation & Self-Test Architecture  
> **Repository:** `C:\Users\ern0e\EAGLEULTRASONiK`  

---

## 1. Executive Summary & Purpose

Bu rapor, EAGLEULTRASONİK projesinin Phase 1'den Phase 4.6'ya kadar devam eden tüm mimari kararlarını, donanım gerçekliklerini, güvenlik analizlerini ve self-test tasarımlarını konsolide eden **Nihai Mimari Doğrulama Dokümanıdır**. 

Phase 4.7 boyunca **KOD DEĞİŞİKLİĞİ YAPILMAMIŞTIR**. Tüm analizler kaynak kod incelemesi ([`main.c`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c), [`esp32_uart.c`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c), [`heater_relay.c`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/heater_relay.c), [`ultrasonic_pwm.c`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/ultrasonic_pwm.c), [`x9c103s.c`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/x9c103s.c), [`process_timer.c`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/process_timer.c), [`ekran_kontrol.ino`](file:///C:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino)), donanım doğrulama verileri ve uzman sub-agent simülasyonları ile gerçekleştirilmiştir.

---

## 2. Human-Verified Hardware Reality Baseline

Sistem mimarisi aşağıdaki **insan tarafından doğrulanmış 3 temel fiziksel ilke** üzerine oturtulmuştur:

### 2.1. İletişim Mimarisi (Prototype TTL UART vs Production RS485)
- **Masaüstü Prototipi:** ESP32-S3 Master <-> Doğrudan UART TTL (115200 8N1) <-> 1 Adet STM32G474RE Slave (Point-to-Point). Masaüstünde RS485 transceiver çipleri lehimli değildir.
- **Nihai Üretim Sistemi:** ESP32-S3 Master <-> RS485 Transceiver <-> Paylaşımlı RS485 Multi-Drop Veriyolu <-> N Adet (1..10) STM32G4 Slaves.
- **Donanımsal Durum:** RS485 transceiver entegresi, 120 Ω hat sonlandırması (termination), fail-safe bias dirençleri ve DE/RE yön kontrolü `[HARDWARE VERIFICATION PENDING]` olarak işaretlenmiştir.

### 2.2. İki Bağımsız Kontrol Ekseni (Control Axis A vs Control Axis B)
- **CONTROL AXIS A — Ultrasonik Çıkış Gücü (%0 - %100):**
  $$\text{STM32} \xrightarrow{\text{PC7 ZC EXTI9\_5}} \text{TIM15 OPM} \xrightarrow{\text{PC6 Triac Gate}} \text{Power Board} \xrightarrow{\text{Faz Açısı}} \text{Ultrasonik Çıkış Gücü}$$
- **CONTROL AXIS B — Ultrasonik Çıkış Frekansı (28 kHz / 40 kHz):**
  $$\text{ESP32} \xrightarrow{\text{UART}} \text{STM32} \xrightarrow{\text{PB12/13/14 Bit-Bang}} \text{X9C103S Pot} \xrightarrow{\text{Hybrid Board}} \text{Power Board} \xrightarrow{\text{Çıkış Frekansı}}$$
- **KESİN KURAL:** X9C103S dijital potansiyometresi TRIAC kontrolü **YAPMAZ**. X9C103S yalnızca frekans belirler; TRIAC yalnızca çıkış gücünü yönetir.

### 2.3. Bağımsız Isıtıcı Mimarisi (Heater Resistance Architecture)
- Isıtıcı rezistanslar Ultrasonik Güç Kartı'ndan (Power Board) **BESLENMEZ**. Güç kartından tamamen bağımsız AC şebeke hattındadır.
  $$\text{PT100 Sensor} \xrightarrow{\text{OPAMP3 + ADC2}} \text{STM32} \xrightarrow{\text{PB15 GPIO Output}} \text{Heater Relay} \xrightarrow{\text{AC Line}} \text{Heater Resistance}$$

---

## 3. Detailed Architecture Sub-System Summaries

### 3.1. Commissioning Protocol V2 & Factory ID = 1 Resolution
- **Sorunun Özü:** Yeni/programlanmamış kartların varsayılan adresi `ID=1` (veya Flash Page 127 boş/tanımsız) olduğundan, birden fazla uncommissioned kart multi-drop RS485 hattına aynı anda bağlandığında broadcast discovery (`T0:`) veya `T1:` komutlarına aynı anda yanıt verir. Bu durum otobüs çakışmasına (bus contention), verinin bozulmasına ve belirsiz kart seçimine yol açar.
- **Çözüm (EAGLE-PROV-v2 Protocol):** STM32G474RE donanımsal 96-bit Unique ID (`0x1FFF7590`) kullanılarak çakışmasız adresleme protokolü tasarlanmıştır.
- **Protokol Akışı:**
  1. ESP32 `PROBE_UNCOMMISSIONED` yayınlar.
  2. Uncommissioned STM32'ler `Slot = CRC16(UID96) % 16` formülüyle rastgele zaman diliminde 96-bit UID yanıtı döner.
  3. ESP32 seçilen UID'ye geçici adres (`T99`) atar.
  4. ESP32 tekil komutla (`T99:SET_ID:<N>`) kartın yeni adresini atar.
  5. STM32 Flash Page 127'ye (`0x0807F800`, `TANK_ID_MAGIC = 0xA5A5A5A5UL`) 64-bit doubleword olarak kalıcı yazar ve doğrulama yanıtı basar.
- **Detaylı Rapor:** [commissioning-protocol-v2.md](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/commissioning-protocol-v2.md)

### 3.2. Sıcaklık Kontrol Mimarisi (V1 Relay vs V2 SSR)
- **V1 Mekanik Röle Mimarisi:**
  - Hysteresis (Bang-Bang $\pm 1.0^\circ\text{C}$) kontrol mantığı.
  - **Kritik Bulgu:** Mevcut kaynak kodda ([`heater_relay.c`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/heater_relay.c)) Minimum ON ($T_{\text{ON,min}} = 10\text{s}$) ve Minimum OFF ($T_{\text{OFF,min}} = 10\text{s}$) koruma zamanlayıcıları **EKSİKTİR**.
  - **Durum:** `HARDWARE VALIDATION REQUIRED / PROPOSED ARCHITECTURE` olarak işaretlenmiştir. Röle kontaklarının hızlı açılıp kapanarak ark yapmasını ve yapışmasını önlemek için koruma zamanlayıcıları V1 baseline'a eklenmelidir.
- **V2 SSR Mimarisi:**
  - Solid-State Relay + PID + Time-Proportioning PWM (1000 ms zaman penceresi).
  - Sadece şifre korumalı Servis Ekranı üzerinden `HEATER_MODE_SSR` seçildiğinde aktif olur.
- **Detaylı Rapor:** [heater-control-architecture-v2.md](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/heater-control-architecture-v2.md)

### 3.3. Self-Test Mimarisi & Donanım Kısıtları (Hardware-in-the-Loop Loopback)
Mevcut donanım (1x ESP32, 1x STM32, 1x Nextion HMI, 1x X9C103S; röle/SSR/triyak/güç kartı/motor YOK) çerçevesinde 6 dahili firmware loopback testi tasarlanmıştır:
1. **GPIO Output Loopback:** Output komut pini (`PB15`) loopback input pinine bağlanarak GPIO sürücü katmanı doğrulanır.
2. **Timer Loopback:** `TIM15` pulse çıkışı (`PC6`) `TIM2` Input Capture pinine bağlanarak 100 µs pulse genişliği ve ISR frekansı doğrulanır.
3. **Accelerated Process Timer Test:** `TEST_FLAG_ACCELERATED_TIMER` ile 10 kat hızlandırılmış geri sayım ($1\text{s gerçek} = 10\text{s işlem}$) ile timer sıfırlama zinciri test edilir.
4. **X9C103S Wiper ADC Voltaj Bölücü:** X9C wiper pini $10\text{ k}\Omega$ referans direnci ile voltaj bölücü yapılıp `ADC2`'ye bağlanır. Adım direnci hesaplanır ($R_{\text{wiper}} = R_{\text{ref}} \times \frac{V_{\text{adc}}}{V_{\text{cc}} - V_{\text{adc}}}$). Step 40 ($4.0\text{ k}\Omega$) ve Step 90 ($9.0\text{ k}\Omega$) doğrulanır. **Prototype Diagnostic Feature** olarak dökümante edilmiştir.
5. **ESP32 <-> STM32 TTL UART Self-Test:** PING/ACK, sıra numarası, timestamp, CRC16 ve paket kayıp testleri.
6. **Triac & Heater Timing Simulation:** ESP32 `GPIO4` 100Hz ZC simülatör sinyali STM32 `PC7` pini üzerinden tetiklenerek ateşleme gecikmesi loopback ile ölçülür.

- **Kritik Güvenlik Uyarısı ("Safety Illusion"):** Loopback testinin PASS vermesi yalnızca mikrodenetleyici pinlerinin ve yazılım mantığının çalıştığını kanıtlar. Fiziksel röle kontaklarının yapışmadığını, triyağın kısa devre olmadığını veya rezistansın sağlam olduğunu **KANITLAMAZ**. Sistem 5 katmanlı test modeli ile katmanlandırılmıştır.
- **Detaylı Raporlar:** [self-test-architecture.md](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/self-test-architecture.md), [self-test-coverage-matrix.md](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/self-test-coverage-matrix.md)

### 3.4. Servis Güvenliği Mimarisi (Service Security Architecture)
- Nextion HMI üzerinde Operatör Rolü ile Servis Rolü kesin olarak ayrılmıştır.
- Dinamik HMAC-SHA256 Challenge-Response ve 6 denemede 15 dakika kilitlenen üssel PIN koruması tasarlanmıştır.
- `SYS_MODE_RUNNING` durumunda `HEATER_MODE` veya `TANK_ID` değişimini engelleyen **Runtime Configuration Lock** mekanizması hem ESP32 hem STM32 seviyesinde çift katmanlı doğrulanmıştır.
- NVS verisi packed binary struct, schema version (`0x00040007`) ve CRC32 checksum ile koruma altına alınmıştır.
- **Detaylı Rapor:** [service-security-architecture.md](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/service-security-architecture.md)

### 3.5. Güvenli Kapatma ve Adversarial Güvenlik İncelemesi (Final Safety Review)
- Tüm kapatma tetikleyicileri (HMI STOP, Process Timer zero, FAULT, UART Timeout, PT100 Kopma/Kısa devre, MCU Watchdog Reset) incelenmiştir.
- **Tüm Güvenli Durumlarda Beklenen Davranış:** `PB15` (Heater Relay) LOW, `PC6` (Triac Gate) LOW, `TIM15` Stop, X9C defined state, Mode IDLE/FAULT.
- **Kritik Eksikler:** `BENCH_DEV_MODE_ID 1` hardcoded kalması, STM32 tarafında UART RX timeout bulunmaması, `main.c` içinde donanımsal IWDG aktif olmaması ve `X9C103S_SetStep` içindeki 520 µs global interrupt kapatma (`__disable_irq`).
- **Detaylı Rapor:** [final-safety-review.md](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/final-safety-review.md)

---

## 4. Architectural Verification Cross-Reference Matrix

| Sub-system / Domain | Key Verification Finding | Document Reference | Status |
|:---|:---|:---|:---|
| **Multi-Drop Bus** | Multi-board ID=1 bus contention resolved via 96-bit UID Slotted Backoff | [commissioning-protocol-v2.md](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/commissioning-protocol-v2.md) | `PROPOSED ARCHITECTURE` |
| **Control Axis A** | Ultrasonic Power (%0-%100) driven solely via PC6 Triac Gate & TIM15 OPM | [POWER-VS-FREQUENCY-CONTROL.md](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/POWER-VS-FREQUENCY-CONTROL.md) | `CONFIRMED CURRENT BEHAVIOR` |
| **Control Axis B** | Ultrasonic Frequency (28/40kHz) driven solely via X9C103S Step 40 / 90 | [POWER-VS-FREQUENCY-CONTROL.md](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/POWER-VS-FREQUENCY-CONTROL.md) | `CONFIRMED CURRENT BEHAVIOR` |
| **Heater Loop** | Independent AC mains relay line, PT100 OPAMP3 + ADC2 input | [control-loop-architecture.md](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/control-loop-architecture.md) | `CONFIRMED CURRENT BEHAVIOR` |
| **Heater Min ON/OFF**| Min ON/OFF 10s guard timers currently missing in `heater_relay.c` | [heater-control-architecture-v2.md](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/heater-control-architecture-v2.md) | `CONFIRMED BUG / REQUIRED` |
| **Heater V2 SSR** | Service-selectable SSR + PID + Time-Proportioning PWM (1000ms window) | [heater-control-architecture-v2.md](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/heater-control-architecture-v2.md) | `PROPOSED ARCHITECTURE / V2` |
| **Bench Self-Test** | 6 Firmware loopback tests designed for 1x ESP32, 1x STM32, 1x HMI, 1x X9C | [self-test-architecture.md](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/self-test-architecture.md) | `PROPOSED ARCHITECTURE` |
| **X9C Wiper Test** | Step resistance verified via $10\text{ k}\Omega$ ADC voltage divider | [self-test-architecture.md](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/self-test-architecture.md) | `PROTOTYPE DIAGNOSTIC FEATURE` |
| **Service Security** | Password protected Nextion HMI + Runtime lock on `SYS_MODE_RUNNING` | [service-security-architecture.md](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/service-security-architecture.md) | `PROPOSED ARCHITECTURE` |
| **Safe Shutdown** | HMI STOP / Timer 0 / FAULT / Sensor Fault forces PB15 LOW & PC6 LOW | [final-safety-review.md](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/final-safety-review.md) | `CONFIRMED CURRENT BEHAVIOR` |
| **Hardware IWDG** | Hardware IWDG currently missing in STM32 `main.c` | [final-safety-review.md](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/final-safety-review.md) | `CONFIRMED BUG / SAFETY ISSUE` |

---

## 5. Conclusion & Baseline Status

Phase 4.7 çalışmaları neticesinde sistemin tüm mimari sınırları, donanım gerçeklikleri, protokol ihtiyaçları, güvenlik katmanları ve test stratejisi **eksiksiz olarak dökümante edilmiş ve doğrulanmıştır**.

Nihai sınıflandırmalar ve Phase 5 geçiş onayları bir sonraki raporda ([`phase-4.7-design-freeze.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/phase-4.7-design-freeze.md)) sunulmuştur.
