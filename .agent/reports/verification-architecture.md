# EAGLEULTRASONİK — Doğrulama ve Test Mimarisi (Verification Architecture)

> **Doküman Statüsü:** Test & Verification Architecture Specification  
> **Tarih:** 10 Ağustos 2026  
> **Aşama:** Phase 4.6 Baseline Adjustment

---

## 1. 6 Seviyeli Test Mimarisi (Six Test Levels)

```
LEVEL 1: Static Analysis & Code Linting (MISRA C, Cppcheck)
LEVEL 2: Unit Tests (Host-based C/C++ Test Frameworks)
LEVEL 3: Integration Tests (ESP32 Master <-> STM32 Slave Pipeline)
LEVEL 4: Hardware-in-the-Loop (HIL Pytest Automation via test_hil_uart.py)
LEVEL 5: Fault Injection & Safety Edge-Case Testing
LEVEL 6: System Regression Testing (Long-duration 48h soak test)
```

---

## 2. Yapay Zeka Destekli Doğrulama ve İnsan Onayı Döngüsü (AI Verification Loop + Human Gate)

Sistem emniyet kritik (Safety-Critical) yapıda olduğu için AI ajanlarının üretim kodunu insan denetimi olmadan otomatik değiştirmesine izin verilmez.

```mermaid
graph TD
    CODE[Mevcut Kodbase] --> TEST[Automated Test Suite (HIL / Pytest)]
    TEST -->|Fail / Bug Detect| RCA[AI Root Cause Analysis]
    RCA --> PATCH[AI Patch Proposal / Engineering Change Plan]
    PATCH --> RETEST[Automated Re-Test in Isolated Sandbox]
    RETEST -->|Pass| HUMAN_GATE{HUMAN APPROVAL GATE\n(İnsan Mimar Onayı)}
    HUMAN_GATE -->|Approved| MERGE[Production Branch Merge]
    HUMAN_GATE -->|Rejected| RCA
    RETEST -->|Fail| RCA
```

### 2.1. İnsan Onayı (Human Gate) Neden Şarttır?
Gömülü güvenlik yazılımlarında (Embedded Safety), testlerin %100 geçmesi kodu güvenli yapmaya yetmeyebilir. Yanlış donanım pin varsayımı veya beklenmeyen bir donanım durumu yangın veya fiziksel yaralanmaya yol açabileceğinden nihai merge öncesi **İnsan Onayı (Human Gate)** zorunlu kılınmıştır.

---

## 3. Test Edilebilirlik Matrisi (Testability Coverage)

| Component / Function | Level 1 (Static) | Level 2 (Unit) | Level 4 (HIL) | Level 5 (Fault Injection) |
| --- | :---: | :---: | :---: | :---: |
| **UART Line Parser (`ProcessLine`)** | Yes | Yes | Yes | Malformed strings (`T1:XYZ`) |
| **RS485 Address Routing (`T<ID>:`)** | Yes | Yes | Yes | Duplicate ID injection |
| **Process Timer & Expiry** | Yes | Yes | Yes | Timeout acceleration |
| **Heater Relay Hysteresis** | Yes | Yes | Yes | PT100 ADC out of range (-10°C..110°C) |
| **TRIAC Power Soft-Start** | Yes | No | Yes | Zero-cross loss injection (>500ms) |
| **X9C103S Stepping** | Yes | No | Yes | Frequency change during active wash |
