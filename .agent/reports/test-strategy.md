> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONİK — Doğrulama ve Test Stratejisi (Test Strategy)

> **Doküman Statüsü:** Lead Embedded Systems Architect Output  
> **Tarih:** 10 Ağustos 2026

---

## 1. Test Düzeyleri ve Metodoloji

Projedeki düzeltmelerin doğrulanması için 5 kademeli bir test altyapısı tanımlanmıştır:

```
[ Unit Tests ] ──> [ Integration Tests ] ──> [ HIL Automation (Pytest) ] ──> [ Hardware Fault Injection ] ──> [ Bench Thermal Test ]
```

---

## 2. Test Senaryoları ve Emniyet Matrisi

| Test ID | Test Türü | Test Adı ve Senaryo | Doğrulama Kriteri / Geçme Koşulu |
|---|---|---|---|
| **TEST-01** | HIL Test | **Multi-Drop ID Çakışma Testi**<br>İki STM32 kartına da yeni yazılım atılır. DIP switch 1 ve 2 yapılır. | Her iki kart otobüste sırasıyla `STAT,1,...` and `STAT,2,...` yayınlamalı; çakışma olmamalıdır. |
| **TEST-02** | Hardware Fault | **IWDG CPU Freeze Testi**<br>Yıkama esnasında `main.c` içerisine enjekte edilen `while(1);` tetiklenir. | Donanımsal Watchdog 1000 ms içinde MCU'ya reset atmalı, PB15 rölesi ve PC6 triyağı kapanmalıdır. |
| **TEST-03** | HIL Test | **X9C103S Frekans Değişimi & UART Overrun Testi**<br>Yıkama sürerken peş peşe `SET_FREQ:40` gönderilir. | LPUART1/COM11 üzerinde UART Overrun Error oluşmamalı, triyak ateşlemesinde faz kayması yaşanmamalıdır. |
| **TEST-04** | Integration | **ESP32 Boot Emniyet Testi**<br>STM32 sökülür, ESP32'ye güç verilir, 500. ms'de HMI'dan START basılır. | Ekranda "Kart Yok!" uyarısı verilmeli, hayalet süreç başlatılmamalıdır. |
| **TEST-05** | Hardware Fault | **UART Kablo Kopma / İletişim Kaybı Testi**<br>Süreç `RUNNING` modundayken UART kablosu çekilir. | STM32 5000 ms sonra kendiliğinden `SYS_MODE_IDLE`'a geçmeli, röle ve triyağı kapatmalıdır. |
| **TEST-06** | Fault Injection| **T0 Broadcast Sabotaj Testi**<br>Süreç `RUNNING` modundayken `T0:SET_ID:2` gönderilir. | STM32 komutu reddetmeli (`RUNNING` modunda Flash yazmamalı), süreç kesintisiz devam etmelidir. |
| **TEST-07** | Bench Thermal | **PT100 Gürültü ve Röle Chatter Testi**<br>Ultrasonik transdüserler %100 güçte çalıştırılırken röle durumu izlenir. | Isıtıcı rölesi saniyede defalarca açılıp kapanmamalı (chatter olmamalı), histerezis bandı korunmalıdır. |

---

## 3. Otomatik HIL Test Suite Güncellemesi (`test_hil_uart.py`)

Mevcut `test_hil_uart.py` dosyasına aşağıdaki test fonksiyonları eklenecektir:
- `test_06_communication_loss_timeout()`: 5 saniye veri kesilince STM32'nin IDLE'a düştüğünü COM11'den doğrular.
- `test_07_t0_broadcast_rejection_on_running()`: RUNNING modunda `T0:SET_ID` gönderildiğinde reddedildiğini kontrol eder.
