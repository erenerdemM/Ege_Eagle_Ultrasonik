> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONİK — Phase 5 Implementation Readiness Decision

> **Doküman Statüsü:** Final Design Freeze & Readiness Assessment  
> **Tarih:** 10 Ağustos 2026

---

## 1. Phase 5 Hazırlık Kararı (Readiness Decision)

### 🟢 DECISION: `PHASE 5 READY` (Kod Uygulama Aşamasına Hazır)

Sistemin donanım pin haritası, çevrebirim konfigürasyonları, saat ağacı, haberleşme paket formatı ve kontrol algoritmaları **kaynak kod seviyesinde %100 analiz edilmiş, doğrulanmış ve dondurulmuştur (Design Freeze)**.

---

## 2. Kod Değişikliğine Başlamadan Önce Netleştirilmesi Gereken Konular

"Implementation can begin" denmeden önce aşağıdaki 7 kritik konuda belirsizlik kalmadığı teyit edilmiştir:

1. **Isıtıcı Fail-Safe:** STM32 donanımsal IWDG aktifleştirilecek ve main loop kilitlenmelerinde PB15 rölesi donanım reset ile güvenli kapatılacaktır.
2. **AC Şebeke Frekansı:** 50Hz varsayımı korunacak, isteğe bağlı adaptif ZC ölçümü eklenecektir.
3. **X9C Zamanlaması:** Global `__disable_irq()` kaldırılarak adımlama kesmeler açık şekilde yapılacaktır.
4. **UART Fiziksel Katmanı:** 115200 8N1 Multi-drop haberleşmede donanımsal Auto-Direction RS485 transceiver varsayılmıştır.
5. **Tank ID Yapısı:** `BENCH_DEV_MODE_ID` `0` yapılarak DIP Switch / Flash override üretim moduna alınacaktır.
6. **Watchdog:** 1000 ms aşımlı donanımsal IWDG aktif edilecektir.
7. **GPIO Reset State:** STM32 boot ve reset anlarında PB15 ve PC6 varsayılan olarak LOW/RESET durumundadır.

---

## 3. Design Freeze Kontrol Listesi (Checklist)

- [x] **System Purpose understood** (Yıkama makinesi işlevi ve pipeline anlaşıldı)
- [x] **Hardware Architecture understood** (ESP32 Master + STM32 Slave yapısı doğrulandı)
- [x] **ESP32 understood** (NVS, Nextion HMI, ZC Simülatör anlaşıldı)
- [x] **STM32 understood** (Süper-döngü, HAL, Flash override, DIP SW anlaşıldı)
- [x] **UART understood** (Multi-Drop `T<ID>:` ASCII protokolü ve `STAT,...` telemetrisi doğrulandı)
- [x] **PT100 understood** (OPAMP3 PGAx2 + ADC2 linear formül ve aralık penceresi doğrulandı)
- [x] **Heater understood** (PB15 ±1.0°C histerezisli bang-bang kontrol doğrulandı)
- [x] **Ultrasonic Control understood** (EXTI9_5 + TIM15 OPM triyak faz açısı ve soft-start doğrulandı)
- [x] **X9C understood** (Step 40 / 90 bit-bang 28kHz/40kHz adımlama doğrulandı)
- [x] **Timing understood** (1µs timer tick, 500ms heartbeat, 3000ms watchdog, 1Hz countdown anlaşıldı)
- [x] **State Machines understood** (IDLE / RUNNING / FAULT durum makineleri ve geçişleri doğrulandı)
- [x] **Safety Architecture understood** (IWDG eksikliği, PT100 koruması, ZC timeout koruması anlaşıldı)
- [x] **Failure Modes understood** (16 hata senaryosu ve yanıtları belgelendi)
- [x] **Recipe System understood** (P1, P2, P3 ve NVS mekanizması doğrulandı)
- [x] **Multi-Tank Architecture understood** (1..10 tank adresleme yapısı teyit edildi)
- [x] **Known Bugs Mapped** (Kök nedenler ve ECP planı hazırlandı)
- [x] **Unknowns Documented** (Tasarım soruları ve varsayımları listelendi)
- [x] **Hardware Verification Items Documented** (HIL ve donanım test stratejisi belirlendi)

**Sonç:** Kod düzenleme ve düzeltme aşamasına (Phase 5 Implementation) geçilmesi için donanımsal ve yazılımsal tüm altyapı dondurulmuştur.
