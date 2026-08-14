# EAGLEULTRASONİK — Ana Tasarım Referansı (Design Baseline v2)

> **Doküman Statüsü:** MASTER SYSTEM DESIGN BASELINE (NİHAİ REFERANS)  
> **Tarih:** 10 Ağustos 2026  
> **Aşama:** Phase 4.6 Architectural Alignment

---

## 1. System Purpose & Physical Architecture

EAGLEULTRASONİK, çoklu havuz (1..10 tank) desteğine sahip otomasyonlu bir endüstriyel ultrasonik yıkama ve sıcaklık kontrol sistemidir.

### 1.1. Physical & Electrical Topology
- **Master Node:** ESP32-S3 (Nextion HMI, NVS reçeteleri, otobüs koordinasyonu).
- **Slave Nodes:** STM32G474RE (Her bir yıkama tankı için bağımsız gerçek zamanlı donanım sürücüsü).
- **Physical Communication Layer:**
  - **Masaüstü Prototip:** ESP32 <-> UART TTL <-> 1 STM32 (Noktadan noktaya).
  - **Nihai Üretim:** ESP32 <-> RS485 Bus <-> N STM32 Slaves (Half-duplex multi-drop). Donanımsal parametreler **[HARDWARE VERIFICATION PENDING]**.

---

## 2. Independent Control Loops & Hardware Mapping

```
1. HEATER CONTROL LOOP:
   PT100 -> OPAMP3 + ADC2 -> Hysteresis Controller (±1.0°C) -> PB15 Relay -> RESISTANCE HEATER
   * Isıtıcı rezistanslar Ultrasonik Güç Kartı'ndan BESLENMEZ. Bağımsız şebeke röle hattıdır.

2. ULTRASONIC POWER CONTROL LOOP (Control Axis A):
   PC7 ZC EXTI9_5 -> TIM15 OPM -> PC6 TRIAC Gate -> POWER CARD -> ULTRASONIC MOTOR
   * Triyak kontrolü yalnızca ÇIKIŞ GÜCÜNÜ (%0 - %100) belirler. X9C potansiyometresi bu hatta DÂHİL DEĞİLDİR.

3. ULTRASONIC FREQUENCY CONTROL LOOP (Control Axis B):
   HMI (28/40kHz) -> ESP32 -> STM32 -> PB12/13/14 Bit-Bang -> X9C103S Pot -> HYBRID CARD -> POWER CARD FREQUENCY
   * X9C103S Hibrit Kart üzerindeki mekanik trimpotların yerine geçer. ÇIKIŞ FREKANSINI (28/40kHz) belirler.
```

---

## 3. Device Provisioning Architecture

- **Factory ID = 1:** Yeni/programlanmamış kartın ilk bağlantı varsayılan adresidir (Bug değildir).
- **Commissioning Mode:** HMI üzerinden devreye alma modu açılarak yeni karta benzersiz kalıcı ID (`SET_ID:<N>`) atanır ve Flash Page 127'ye kaydedilir.

---

## 4. Safe Process Shutdown Architecture

Süre dolduğunda (`remaining_seconds == 0`), `STOP` basıldığında veya `SYS_MODE_FAULT` oluştuğunda `SAFE_PROCESS_STOP()` fonksiyonu tetiklenir:
1. Triyak gate kapatılır (`PC6 LOW`, `TIM15 Stop`).
2. Isıtıcı rölesi kapatılır (`PB15 LOW`).
3. ProcessTimer durdurulur.
4. Sistem `SYS_MODE_IDLE` moduna çekilir.

---

## 5. Phase 5 Readiness Assessment (12 Soruda Hazırlık Değerlendirmesi)

1. **Fiziksel mimari kesin mi?** EVET (Prototip UART, Üretim RS485).
2. **Heater ve ultrasonic power birbirinden ayrıldı mı?** EVET (Isıtıcı rezistans Güç Kartı'ndan bağımsızdır).
3. **TRIAC ve X9C ayrıldı mı?** EVET (TRIAC = Güç %, X9C = Frekans kHz).
4. **RS485/UART ayrımı doğru mu?** EVET (Uygulama protokolü aynı, donanım donatımı UNKNOWN).
5. **ID provisioning mimarisi net mi?** EVET (Factory ID=1 + HMI Commissioning Mode).
6. **Process timeout davranışı net mi?** EVET (`ProcessTimer = 0` $\rightarrow$ `SYS_MODE_IDLE`).
7. **Heater shutdown davranışı net mi?** EVET (`SAFE_PROCESS_STOP()` ile Isıtıcı + Triyak kapalı).
8. **Temperature algorithm doğrulandı mı?** EVET (Hysteresis Bang-Bang ±1.0°C, PID V2'ye ertelendi).
9. **X9C scope net mi?** EVET (Step 40 = 28kHz, Step 90 = 40kHz; ADC direnç ölçümü prototip kalibrasyonu).
10. **Test stratejisi yeterince tanımlı mı?** EVET (6 seviyeli test mimarisi).
11. **Safety-critical belirsizlik kaldı mı?** HAYIR (1000ms IWDG ve timeout fail-safe tanımlandı).
12. **İnsan olarak onaylamam gereken tasarım kararları var mı?** EVET (Aşağıdaki liste).

### 🟢 NİHAİ KARAR: `PHASE 5 STATUS: [READY]`

---

## 6. PHASE 5'E GEÇMEDEN ÖNCE KULLANICININ ONAYLAMASI GEREKEN TASARIM KARARLARI (DESIGN FREEZE APPROVAL CHECKLIST)

- [ ] **1. Mimarilerin Ayrımı:** Prototipte 1 STM32 + UART, üretimde N STM32 + RS485 bus mimarisinin kabul edilmesi.
- [ ] **2. Devreye Alma Yöntemi:** Üretimde RS485 otobüs çakışmalarını önlemek için "HMI Üzerinden Tekil Devreye Alma (Commissioning Mode)" yönteminin kullanılması.
- [ ] **3. Sıcaklık Kontrolü:** Sıcaklık denetiminde şimdilik PID yerine mevcut Hysteresis (Bang-Bang ±1.0°C) algoritmasıyla devam edilmesi.
- [ ] **4. X9C ADC Ölçümü & Scope:** X9C direncini ADC ile ölçme fikrinin sadece prototip/teşhis aşamasında kalması, Frekans ölçümünün V2 (İleri Sürüm) olarak ertelenmesi.
- [ ] **5. İnsan Onayı (Human Gate):** Güvenlik kritik kod değişikliklerinde otomatik test sonrası nihai "İnsan Onayı (Human Gate)" adımıyla production branch'e merge yapılması.
