> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONİK — Phase 4.5 Rev.2 Design Freeze Challenge Report

> **Doküman Statüsü:** Lead Systems Architect & Hardware Verification Output  
> **Tarih:** 10 Ağustos 2026  
> **Revizyon:** Rev.2 Design Freeze Challenge

---

## 1. Giriş ve İnceleme Yaklaşımı

Phase 4.5 Rev.2 aşamasında, proje sahibi tarafından açıklanan **5 İnsan Doğrulamalı Tasarım Gerçeği (Human Verified Design Facts)** sisteme entegre edilmiş, bağımsız iki kontrol ekseni (**Güç Kontrolü / Triyak** ve **Frekans Kontrolü / X9C103S**) ile **Mevcut Prototip (UART)** ve **Nihai Üretim Mimarisi (RS485)** ayrımı tam olarak analiz edilmiştir.

---

## 2. Zamanlama Sorgulamaları (Timing Challenge Analysis)

### 2.1. Triyak Ateşleme Zamanlaması vs X9C Adımlama Zamanlaması
Triyak ateşleme zamanlaması (`TIM15 OPM` + `EXTI9_5 ZC`) ve X9C103S adımlama zamanlaması (`PB12/13/14 Bit-Bang`) **birbirinden tamamen bağımsız donanımsal zamanlama zincirleridir**.
- **Triyak Timing Chain:** $\text{Zero-Cross} \rightarrow \text{EXTI} \rightarrow \text{TIM15} \rightarrow D_{\text{us}} \rightarrow \text{PC6 Gate}$.
- **X9C Timing Chain:** $\text{CS LOW} \rightarrow \text{U/D Direction} \rightarrow \text{INC Pulse} \rightarrow \text{Wiper Step} \rightarrow R_{\text{pot}}$.

### 2.2. X9C Interrupt Blackout (BUG-NEW-01) Yeniden Değerlendirmesi
- **Sorgulama:** `x9c103s.c` içerisindeki `__disable_irq()` 600 µs boyunca kesmeleri kapatmaktadır.
- **Etki Analizi:**
  - **USART3 UART RX:** 115200 Baud hızında 1 byte $\approx 86.8\mu s$'dir. Donanımsal FIFO kapalı olduğundan (`DisableFifoMode`), 600 µs boyunca 2 byte'tan fazla veri geldiğinde donanımsal **Overrun Error (ORE)** oluşur. `CONFIRMED BUG`.
  - **EXTI Zero-Cross Kesmesi:** 600 µs kesme karartması sırasında zero-cross kenarı gelirse EXTI kesmesi gecikmeli işlenir. Triyak faz ateşleme açısında anlık 600 µs kayma oluşur. `CONFIRMED BUG`.
- **Sınıflandırma:** `CONFIRMED BUG` (Severity: `P1`).

### 2.3. Triyak 50Hz / 60Hz (ALG-001) Yeniden Değerlendirmesi
- **Sorgulama:** `ultrasonic_pwm.c` içerisinde `#define AC_HALF_CYCLE_US 10000UL` sabit 50Hz varsayımı kullanılmaktadır.
- **Değerlendirme:** Türkiye ve Avrupa elektrik şebekesi 50Hz olduğu için prototip cihaz **50Hz Kısıtlı Tasarım (Design Constraint)** olarak tasarlanmıştır. Cihaz 60Hz şebekeye takılmadığı sürece bir bug oluşturmaz.
- **Sınıflandırma:** `DESIGN CONSTRAINT` (Severity: `P2`).

---

## 3. Yeniden Sınıflandırılmış Bulgular Matrisi (Finding Reconciliation Table)

| Finding ID | Title / Issue | Previous Severity | New Severity | Confidence | Evidence Source | Reason / Reconciliation |
| --- | --- | :---: | :---: | :---: | --- | --- |
| **BUG-CRIT-01** | `BENCH_DEV_MODE_ID = 1` Sabit ID Bayrağı | CRITICAL | **P0** | HIGH | `main.c:53` | Tüm slave kartların ID=1 açılmasına ve otobüsün çakışmasına sebep olur. |
| **SEC-001** | Donanımsal Watchdog (`IWDG`) Eksikliği | HIGH | **P0** | HIGH | `main.c:172` | MCU donarsa PB15 rölesi açık kalır, yangın riski (**Thermal Runaway**). |
| **BUG-NEW-01** | X9C103S 600µs Kesme Karartması | HIGH | **P1** | HIGH | `x9c103s.c:31` | `__disable_irq()` 600µs UART Overrun Error ve EXTI ZC faz kayması yaratır. |
| **ESP32-BUG-01**| ESP32 Boot 3sn Watchdog Bypass Hatası | HIGH | **P1** | HIGH | `ekran_kontrol.ino:79` | Boot anında t=0..3sn arası çevrimdışı kartı çevrimiçi zanneder, hayalet START atar. |
| **SEC-004** | STM32 İletişim Kaybı Timeout Eksikliği | HIGH | **P1** | HIGH | `esp32_uart.c:64` | Kablo koptuğunda STM32 ayarlanan süre bitene kadar yükleri açık tutar. |
| **SEC-003** | Sınırsız Evrensel Broadcast `T0:SET_ID` | HIGH | **P1** | HIGH | `esp32_uart.c:98` | RUNNING modunda `T0:SET_ID` gelirse Flash siler, CPU 40ms donar, triyak kontrolden çıkar. |
| **BUG-HIGH-01** | STM32 Single-Line UART Tamponu | HIGH | **P1** | HIGH | `esp32_uart.c:270` | Hızlı komut paketlerinde ikinci paket düşürülür. |
| **BUG-HIGH-02** | ESP32 HMI `delay(400)` Blocking Çağrısı | HIGH | **P1** | HIGH | `ekran_kontrol.ino:484` | HMI renk değişimi için main loop 600ms kilitlenir, telemetri düşer. |
| **ALG-001** | 50Hz Sabit AC Yarı Periyot Varsayımı | HIGH | **P2** | HIGH | `ultrasonic_pwm.c:16` | 50Hz hedefli tasarım kısıtıdır (`DESIGN CONSTRAINT`). |
| **BUG-MED-01** | PT100 Ham ADC Okumasında Filtre Eksikliği | MEDIUM | **P2** | HIGH | `pt100_adc.c:49` | Filtresiz ADC okuması gürültüde röle tıkırtısına (chatter) sebep olur. |

---

## 4. Donanım Test Sınırlamaları (Current Hardware Limitations)

Şu anda test masasında **1 adet STM32 kartı** bulunmakta ve **RS485 dönüştürücü çip bulunmamaktadır**. Bu nedenle aşağıdaki testler mevcut prototip üzerinde **YAPILAMAZ**:
- ❌ Çoklu-Düğüm (Multi-Node) RS485 otobüs testi.
- ❌ Çift ID (Duplicate ID) otobüs çakışma (bus collision) testi.
- ❌ RS485 hat gürültü ve diferansiyel sinyal testi.
- ❌ Uzun kablo hattı ve 120 Ω terminasyon testi.

Mevcut **Tek STM32 + ESP32 UART Prototipi** üzerinde YAPILABİLECEK testler:
- ✅ IWDG Donanımsal Reset testi (`main.c` freeze).
- ✅ X9C adımlama sırasında UART Overrun Error izleme.
- ✅ PT100 kopuk/kısa devre ve sıcaklık histerezis testi.
- ✅ Triyak faz açısı soft-start ve ZC kayıp koruma testi.
- ✅ ESP32 boot zaman aşımı ve HMI komut işleme testi.

---

## 5. Phase 5 Engelleri (Phase 5 Blockers)

Phase 5 kod uygulama aşamasına başlamadan önce çözülen ve engeli kalkan konular:
- [x] **Güç Kontrolü ve Frekans Kontrolü ayrıldı:** Triyak (güç) ve X9C103S (frekans) bağımsız iki eksen olarak modellendi.
- [x] **RS485 Mimarisi netleştirildi:** Uygulama protokolünün RS485 ile uyumlu olduğu, elektriksel detayların donanıma bırakıldığı netleşti.
- [x] **IWDG ve Fail-Safe stratejisi belirlendi:** Thermal runaway önlemek için 1000ms IWDG eklenecek.
- [x] **BENCH_DEV_MODE_ID sıfırlama planlandı:** Üretim derlemesinde 0 yapılacak.

---

## 6. Design Freeze Checklist & Final Verdict

```
[x] Current prototype understood (1 STM32 over UART)
[x] Production architecture understood (N STM32s over RS485 Bus)
[x] RS485 migration understood (Application protocol is compatible; electrical details UNKNOWN)
[x] Power control understood (TIM15 + PC6 Triac Gate phase firing to Power Card)
[x] Frequency control understood (X9C103S bit-bang digital pot to Hybrid Card for 28kHz/40kHz)
[x] X9C independent from TRIAC (Triac = Power %, X9C = Frequency kHz; strictly separate)
[x] PT100 algorithm understood (OPAMP3 PGAx2 + ADC2 linear conversion and validation window)
[x] Heater safety understood (PB15 hysteresis relay control + IWDG requirement)
[x] TRIAC algorithm understood (Zero-cross EXTI + TIM15 OPM 500us..9500us soft-start)
[x] X9C algorithm understood (Step 40 = 28kHz, Step 90 = 40kHz empirical mapping)
[x] UART protocol understood (T<ID>: ASCII lines @ 115200 8N1 + STAT telemetry)
[x] Multi-tank architecture understood (1..10 tank address routing via MY_TANK_ID)
[x] State machines understood (IDLE / RUNNING / FAULT transitions)
[x] Safety failure modes understood (16 failure scenarios mapped)
[x] Unknowns documented (RS485 physical layer parameters tagged UNKNOWN)
[x] Hardware test limitations documented (Bench setup: 1 STM32, no RS485 converter)
[x] Phase 5 implementation boundaries defined (Safety-first order: P0 -> P1 -> P2 -> P3)
```

### 🟢 NİHAİ KARAR: `PHASE 5 READY` (Kod Uygulama Aşamasına Hazır)

Rev.2 Tasarım Dondurma (Design Freeze) doğrulaması başarıyla tamamlanmıştır. Donanım gerçeklikleri ile yazılım mimarisi tam olarak hizalanmıştır. Kod düzenleme aşamasına (Phase 5) geçilebilir.
