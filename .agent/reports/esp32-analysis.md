> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# ESP32-S3 Firmware Derinlemesine İnceleme ve Güvenlik Analiz Raporu

> **Doküman Statüsü:** Lead Embedded Systems Engineer Firmware Audit Output  
> **Tarih:** 10 Ağustos 2026  
> **Hedef Platform:** ESP32-S3-N16R8 (16MB Flash, 8MB PSRAM, Dual-Core LX7 @ 240MHz)

---

## 1. Alt Sistem Bazlı Analiz Özeti

### 1.1. Framework ve Donanım Yapılandırması (`ekran_kontrol.ino`)
- **Çalışma Zamanı:** ESP32 Arduino Core v2.x / FreeRTOS altyapısı.
- **Pin Tahsisleri:**
  - `Serial1` (STM32 Multi-drop Bus): `GPIO18` (RXD), `GPIO8` (TXD).
  - `Serial2` (Nextion HMI): `GPIO16` (RXD2), `GPIO17` (TXD2).
  - `ZC_SIM_PIN` (Zero-cross Simülatör): `GPIO4`.
- **Pin Seçim Rasyoneli:** ESP32-S3 chip'inde `GPIO26` ve `GPIO27` pinleri dahili Octal SPI Flash/PSRAM hatlarına ayrıldığından, varsayılan UART pinleri yerine donanımsal olarak GPIO18/8 seçilmiştir.

### 1.2. NVS Kalıcı Bellek Yönetimi (`Preferences`)
- **Namespace:** `"ultra"`
- **Saklanan Parametreler:**
  - `pS1..pS3`, `pT1..pT3`: P1, P2, P3 reçete süre ve sıcaklık değerleri.
  - `guc`: Genel triyak güç sınırı (%).
  - `kartid`: Servis menüsünden atanan yeni kart ID'si.
  - `maxgoz`: Aktif tank/göz sayısı (1..10).
- **Yükleme/Kaydetme:** Boot esnasında `nvsYukle()` ile okunur; HMI sayfasında SAVE basıldığında `nvsKaydet()` ile yazılır.

### 1.3. Zero-Cross Simülatör Modülü (`esp_timer`)
- **Problem & Çözüm:** ESP32-S3 LEDC donanımı 100Hz frekansta 8-bit çözünürlük için geçerli clock bölücü üretememekte (`div_param=0` hatası), LEDC çökmektedir.
- **Uygulama:** `esp_timer` altyapısı kullanılarak 5000 µs (`ZC_SIM_HALF_PERIOD_US`) periyotlu donanımsal kesme callbacksiz olarak `GPIO4` pinini toggle eder. Tamamen non-blocking çalışır.

### 1.4. Bağlantı Emniyeti ve Kör Başlatma Engeli (`isKartBagli`)
- **Zaman Aşımı:** `STM_BAGLANTI_TIMEOUT = 3000` ms (3 saniye).
- **Mantık:** Her tank/göz için en son telemetri alma zamanı `stm_son_veri_zamani[goz_id]` değişkeninde tutulur.
- **Emniyet Kilidi:** `baslatmaEngelliMi()` fonksiyonu, o an seçili gözün kartı son 3000ms içinde canlı telemetri atmadıysa `START`, `P_HIZLI`, `P1_SEL` gibi çalıştırma komutlarının STM32'ye iletilmesini donanımsal olarak **engeller**.

---

## 2. Tespit Edilen Firmware Riskleri ve Problemler

### 2.1. String Manipülasyonları ve Bellek Şişmesi / Bölünmesi (Heap Fragmentation)

- **Problem:** `ekran_kontrol.ino` dosyası genelinde yoğun olarak Arduino `String` sınıfı dinamik bellek tahsisleri yapılmaktadır:
  ```cpp
  String gelenMesaj = "";
  String stmMesaj = "";
  String usbMesaj = "";
  String log = adresli;
  ```
- **Risk:** Sürekli `String` birleştirmeleri (`+` operatörleri) ve `substring()` çağrıları, ESP32 heap belleğinde uzun süreli çalışmada bellek bölünmesine (fragmentation) yol açabilir.

### 2.2. Bloklayıcı Delay Kullanımı (`delay()` in HMI Handlers)

- **Problem:** Servis kaydı ve sayfa geçişlerinde `delay()` çağrıları bulunmaktadır:
  ```cpp
  // Line 484:
  nextionGonder("b_save.bco=2016");
  delay(400);
  nextionGonder("b_save.bco=50712");
  ```
- **Risk:** 400ms - 600ms'lik `delay()` çağrıları ana `loop()` döngüsünü bloklar. Bu esnada gelen UART mesajları FIFO arabelleğinde birikebilir veya zaman aşımı tetiklenebilir.

### 2.3. HMI Komut Ayrıştırma Taraması Sırasındaki Sınır Aşımı (Out of Bounds Array Access)

- **Problem (`ekran_kontrol.ino:257`)**:
  ```cpp
  if (tank_id < 1 || tank_id >= MAX_GOZ) return;
  ```
  Sınır kontrolü yapılmış olsa da `MAX_GOZ` 11 tanımlıdır. Ancak servis menüsünden `max_goz_sayisi` 10 üzerine çıkarılmak istendiğinde dizinin 11. indeksine erişim riski mevcuttur.
