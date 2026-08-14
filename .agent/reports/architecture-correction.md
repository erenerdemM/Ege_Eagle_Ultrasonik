# EAGLEULTRASONİK — Mimari Düzeltme ve Tasarım Tabanı Raporu (Architecture Correction Report)

> **Doküman Statüsü:** Lead Embedded Systems Architect Master Specification  
> **Tarih:** 10 Ağustos 2026  
> **Aşama:** Phase 4.6 Baseline Adjustment

---

## 1. Giriş ve Düzeltme Amacı

Bu doküman, önceki fazlarda (Phase 1..Phase 4.5) yapılan mimari varsayımlar ile gerçek donanım gerçekliklerini (Hardware Reality) karşılaştırarak çelişkileri düzeltir ve nihai **Tasarım Baseline'ını (Design Baseline v2)** oluşturur.

---

## 2. Düzeltilmiş Ana Mimari Kabuller (Corrected Baseline Principles)

### 2.1. Master / Slave Görev Dağılımı
- **ESP32 (Master):** Nextion HMI ile 9600 baud haberleşir, operatör komutlarını ve reçeteleri (P1, P2, P3) NVS belleğinde yönetir, RS485 otobüsündeki 1..10 adet STM32 Slave tankını koordine eder.
- **STM32 (Slave):** Her bir yıkama tankı için zaman-kritik gerçek zamanlı donanım denetleyicisidir. PT100 sıcaklık okuma, histerezisli rezistans rölesi sürme, Güç Kartı TRIAC faz açısı kontrolü, Hibrit Kart X9C103S frekans adımlama, süreç zamanlayıcı ve donanımsal emniyet görevlerini yürütür.

### 2.2. Fiziksel Katman (Prototype UART vs Production RS485)
- **Masaüstü Prototip (Current Prototype):** Test masasında donanımsal RS485 transceiver çipi bulunmadığı için ESP32 ve 1 adet STM32 kartı doğrudan nokradan noktaya (Point-to-Point) UART TTL (115200 8N1) ile bağlıdır.
- **Nihai Üretim Sistemi (Intended Production):** ESP32 Master ve N adet STM32 Slave, endüstriyel RS485 diferansiyel veriyolu üzerinden half-duplex multi-drop mimariyle haberleşecektir.
- **Tasarım İlkesi:** Uygulama protokolü (`T<ID>:ASCII`) her iki fiziksel katmanda aynıdır. RS485 donanımsal parametreleri (terminasyon, bias, transceiver yön pini vb.) **[HARDWARE VERIFICATION PENDING]** olarak işaretlenmiştir.

---

## 3. Yanlış / Eksik Kabullerin Düzeltme Matrisi

| Önceki Yanlış / Muğlak Kabul | Düzeltilmiş Gerçek Mimari (Corrected Baseline) | Düzeltme Gerekçesi & Kaynak |
| --- | --- | --- |
| `BENCH_DEV_MODE_ID = 1` bir Critical Bug'dır. | Factory ID=1, henüz programlanmamış kartın ilk açılış adresidir. Asıl risk üretim otobüsüne birden fazla ID=1 kartın takılmasıdır. | Donanım Devreye Alma (Commissioning) gereksinimi. |
| X9C103S triyak zamanlamasını ve çıkış gücünü kontrol eder. | X9C103S ve TRIAC iki bağımsız eksendir. TRIAC = Çıkış Gücü (%0-100), X9C103S = Çıkış Frekansı (28/40kHz). | Donanım Şeması ve Hibrit Kart yapısı. |
| Isıtıcı rezistanslar Ultrasonik Güç Kartından beslenir. | Isıtıcı rezistanslar Güç Kartından **BAĞIMSIZDIR**. Ayrı şebeke hattı üzerinden PT100 + STM32 + Röle ile kontrol edilir. | Elektriksel Donanım Ayrımı. |
| Süreç bitince sadece triyak kapanır. | Süreç bittiğinde (`ProcessTimer = 0`), STOP basıldığında veya FAULT anında **hem Isıtıcı hem Ultrasonik Güç** güvenli modda kapatılır. | Unified `SAFE_PROCESS_STOP()` Emniyet Mimarisi. |
| Sıcaklık kontrolü PID algoritmasıdır. | Mevcut yazılım açık şekilde **Hysteresis (Bang-Bang ±1.0°C)** algoritmasıdır. PID V2 versiyonuna ertelenmiştir. | [pt100_adc.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/pt100_adc.c) ve [heater_relay.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/heater_relay.c). |
