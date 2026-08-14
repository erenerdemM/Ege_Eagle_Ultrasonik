> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONİK — Mühendislik Değişim Planı (Engineering Change Plan)

> **Doküman Statüsü:** Lead Embedded Systems Architect Output  
> **Tarih:** 10 Ağustos 2026  
> **İlke:** Minimum Safe Change & Non-Breaking Architecture

---

## 1. Değişim Planı Detayları (Finding-by-Finding ECP)

### 📌 ECP-01: BUG-CRIT-01 — Geliştirici Modu ID Bayrağının Temizlenmesi
- **FINDING ID:** `BUG-CRIT-01`
- **TITLE:** Bench Development Mode ID Override Deactivation
- **SEVERITY:** `P0 (Critical)`
- **ROOT CAUSE:** `main.c:53` içerisinde `#define BENCH_DEV_MODE_ID 1` debug makrosunun üretim derlemesinde aktif bırakılması.
- **AFFECTED FILES:** [main.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c)
- **AFFECTED FUNCTIONS:** `main()`
- **CURRENT BEHAVIOR:** Makro `> 0` olduğu için boot esnasında DIP switch ve Flash override bypass edilir, `MY_TANK_ID` zorla `1` yapılır.
- **TARGET BEHAVIOR:** Üretim derlemesinde `BENCH_DEV_MODE_ID` `0` olmalı; kart boot sırasında önce Flash override'ı (`TankId_Load()`), yoksa DIP switch okumasını (`ReadDipSwitchId()`) kullanmalıdır.
- **PROPOSED CODE CHANGE:**
  ```c
  /* main.c:53 */
  #define BENCH_DEV_MODE_ID 0  // Set to 0 for production builds
  ```
- **SIDE EFFECTS:** Yok.
- **REGRESSION RISKS:** DIP switch lehimlenmemiş bench geliştirme kartlarında ID varsayılan 1 olarak açılacaktır (DIP switch pull-up ile okunduğunda 0x00 -> ID 1).
- **TEST REQUIREMENTS:** 2 farklı DIP switch konfigürasyonundaki STM32 kartının otobüste kendi ID'leriyle yayın yaptığı doğrulanmalıdır.
- **ROLLBACK STRATEGY:** Makroyu geçici olarak `1` değerine geri getirmek.

---

### 📌 ECP-02: SEC-001 — Donanımsal Watchdog (IWDG) Eklenmesi
- **FINDING ID:** `SEC-001`
- **TITLE:** Hardware Independent Watchdog (IWDG) Integration for Thermal Safety
- **SEVERITY:** `P0 (Critical Safety)`
- **ROOT CAUSE:** STM32CubeMX `.ioc` ve `main.c` içerisinde `IWDG` biriminin aktifleştirilmemiş olması.
- **AFFECTED FILES:** [main.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c), `main.h`, `Ultrasonik_G4_Master.ioc`
- **AFFECTED FUNCTIONS:** `main()`, `MX_IWDG_Init()`
- **CURRENT BEHAVIOR:** MCU kilitlendiğinde reset atılamaz. PB15 (ısıtıcı) HIGH konumdaysa rezistans sürekli açık kalır.
- **TARGET BEHAVIOR:** LSI clock (32 kHz) üzerinden 1000 ms zaman aşımlı IWDG kurulmalı ve ana süper-döngünün her turunda `HAL_IWDG_Refresh(&hiwdg)` ile beslenmelidir.
- **PROPOSED ARCHITECTURE:**
  - `MX_IWDG_Init()` eklenir (Prescaler 32, Reload 1000).
  - `while(1)` içinde `HAL_IWDG_Refresh(&hiwdg);` çağrılır.
- **SIDE EFFECTS:** Ana döngü süresi 1000 ms'yi aşan bloklayıcı işlemler (ör. uzun Flash erase) yapılırsa MCU reset atar.
- **REGRESSION RISKS:** Flash yazma esnasında IWDG beslenmezse sahte resetler oluşabilir.
- **TEST REQUIREMENTS:** Ana döngü içine yapay `while(1);` enjekte edilerek 1000 ms sonra MCU'nun Donanımsal Reset attığı doğrulanmalıdır.
- **ROLLBACK STRATEGY:** `HAL_IWDG_Refresh` çağrılarını pasife almak.

---

### 📌 ECP-03: BUG-NEW-01 — X9C103S Kesme Karartmasının Kaldırılması
- **FINDING ID:** `BUG-NEW-01`
- **TITLE:** Removal of Global Interrupt Disable in Microsecond Digital Pot Stepping
- **SEVERITY:** `P1 (Critical Real-Time)`
- **ROOT CAUSE:** `x9c103s.c` içinde `__disable_irq()` bloğunun tüm 100 adımlık silecek döngüsünü (600µs) sarmalaması.
- **AFFECTED FILES:** [x9c103s.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/x9c103s.c)
- **AFFECTED FUNCTIONS:** `X9C103S_Init()`, `X9C103S_SetStep()`
- **CURRENT BEHAVIOR:** Silecek adımlanırken kesmeler 600µs boyunca kapatılır. UART alımında Overrun Error oluşur ve zero-cross kesmeleri kaçırılır.
- **TARGET BEHAVIOR:** Global `__disable_irq()` kaldırılmalı; pin geçişleri atomik `HAL_GPIO_WritePin` seviyesinde tutulmalı veya sadece darbe genişliği ($3\mu s$) kadar mikro-kritik alan oluşturulmalıdır.
- **PROPOSED CODE CHANGE:**
  ```c
  /* Global __disable_irq() ve __set_PRIMASK() kaldırılır.
   * Adımlama döngüsü kesmeler açık şekilde çalışır. */
  ```
- **SIDE EFFECTS:** Kesmeler açıkken adımlama süresi 1-2 µs uzayabilir ancak X9C103S entegresi minimum $t_{INC} = 1\mu s$ şartını sağladığı sürece bu bir sorun teşkil etmez.
- **TEST REQUIREMENTS:** `SET_FREQ:40` gönderilirken UART alımında Overrun Error oluşmadığı ve triyak ateşlemesinin tıkırramadığı doğrulanmalıdır.

---

### 📌 ECP-04: ESP32-BUG-01 — ESP32 Boot Watchdog Zaman Aşımı Düzeltmesi
- **FINDING ID:** `ESP32-BUG-01`
- **TITLE:** ESP32 Boot Time Slave Availability Logic Correction
- **SEVERITY:** `P1 (High Safety)`
- **ROOT CAUSE:** `stm_son_veri_zamani[]` başlangıç değerinin `0` olması ve `isKartBagli()` fonksiyonunun `stm_bagli[]` bayrağını kontrol etmemesi.
- **AFFECTED FILES:** [ekran_kontrol.ino](file:///C:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino)
- **AFFECTED FUNCTIONS:** `isKartBagli()`, `setup()`
- **CURRENT BEHAVIOR:** Boot anında t=0..3000ms aralığında `isKartBagli()` yanlışlıkla `true` döner ve hayalet karta START gönderilebilir.
- **TARGET BEHAVIOR:** `isKartBagli()`, hem canlı `stm_bagli[goz_id]` bayrağını hem de `STM_BAGLANTI_TIMEOUT` süresini doğrulamalıdır.
- **PROPOSED CODE CHANGE:**
  ```cpp
  bool isKartBagli(uint8_t goz_id) {
    if (goz_id == 0 || goz_id >= MAX_GOZ) return false;
    return stm_bagli[goz_id] && ((millis() - stm_son_veri_zamani[goz_id]) < STM_BAGLANTI_TIMEOUT);
  }
  ```
- **SIDE EFFECTS:** Yok.
- **TEST REQUIREMENTS:** Bağlı olmayan tank için boot'un 1. saniyesinde START basıldığında ekranda "Kart Yok!" yazdığı doğrulanmalıdır.

---

### 📌 ECP-05: SEC-004 — STM32 İletişim Kaybı Zaman Aşımı (Heartbeat Timeout)
- **FINDING ID:** `SEC-004`
- **TITLE:** STM32 Slave Process Fail-Safe Master Heartbeat Timeout
- **SEVERITY:** `P1 (High Safety)`
- **ROOT CAUSE:** STM32 tarafında ESP32'den gelen iletişim kesildiğinde sistemi güvenli moda düşürecek bir RX zaman aşımı mekanizmasının bulunmaması.
- **AFFECTED FILES:** [esp32_uart.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c), `system_state.h`
- **AFFECTED FUNCTIONS:** `ESP32_UART_Process()`, `ESP32_UART_Init()`
- **CURRENT BEHAVIOR:** Kablo koptuğunda STM32 ayarlanan yıkama süresi bitene kadar (100 dakikaya kadar) ısıtıcıyı ve ultrasoniği çalıştırmaya devam eder.
- **TARGET BEHAVIOR:** Son geçerli komut/paket alımından bu yana 5000 ms geçtiğinde ve mod `SYS_MODE_RUNNING` iken sistem otomatik olarak `SYS_MODE_IDLE`'a çekilmeli ve yükler kapatılmalıdır.
- **PROPOSED CODE CHANGE:**
  ```c
  /* esp32_uart.c */
  static uint32_t last_rx_tick_ms = 0;
  // ProcessLine içinde geçerli pakette: last_rx_tick_ms = HAL_GetTick();
  // ESP32_UART_Process içinde:
  if (g_system_state.mode == SYS_MODE_RUNNING && (HAL_GetTick() - last_rx_tick_ms) > 5000u) {
    g_system_state.mode = SYS_MODE_IDLE; // fail-safe stop
  }
  ```
- **TEST REQUIREMENTS:** Çalışma anında UART kablosu çekilmeli, STM32'nin 5 saniye içinde ısıtıcıyı ve triyağı kapattığı doğrulanmalıdır.

---

### 📌 ECP-06: BUG-HIGH-01 — STM32 UART Dairesel Arabellek (Ring Buffer)
- **FINDING ID:** `BUG-HIGH-01`
- **TITLE:** STM32 USART3 RX Ring Buffer & Overrun Resilience
- **SEVERITY:** `P1 (Reliability)`
- **ROOT CAUSE:** Tek satırlık `rx_line` tamponu kullanılması ve `line_ready == 1` iken yeni gelen karakterlerin düşürülmesi.
- **AFFECTED FILES:** [esp32_uart.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c)
- **AFFECTED FUNCTIONS:** `HAL_UART_RxCpltCallback()`, `ESP32_UART_Process()`
- **PROPOSED SHORT-TERM FIX:**
  - `HAL_UART_RxCpltCallback` içinde 2 elemanlı ping-pong line tamponu kullanılması.
- **PROPOSED LONG-TERM ARCHITECTURE:**
  - 256-byte Dairesel Ring Buffer (Head/Tail indeksli) veya UART Idle Line Interrupt + DMA alım altyapısı.

---

### 📌 ECP-07: BUG-HIGH-02 — ESP32 HMI İşleyici Non-Blocking Dönüşümü
- **FINDING ID:** `BUG-HIGH-02`
- **TITLE:** ESP32 Non-Blocking UI Feedback State Machine
- **SEVERITY:** `P1 (Reliability)`
- **ROOT CAUSE:** `delay(400)` ve `delay(600)` bloklayıcı bekleme çağrıları.
- **AFFECTED FILES:** [ekran_kontrol.ino](file:///C:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino)
- **PROPOSED FIX:** `delay()` çağrıları kaldırılıp `save_button_reset_tick` timestamp değişkeni ile `loop()` içerisinde non-blocking renk geri çevirme yapılması.

---

### 📌 ECP-08: SEC-003 — Broadcast `T0:SET_ID` Kısıtlaması
- **FINDING ID:** `SEC-003`
- **TITLE:** Restriction of Broadcast SET_ID Command to IDLE Mode Only
- **SEVERITY:** `P1 (Safety)`
- **ROOT CAUSE:** `ProcessLine()` içinde `T0:SET_ID` komutunun mod kontrolü yapılmadan doğrudan Flash erase çalıştırması.
- **AFFECTED FILES:** [esp32_uart.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c)
- **PROPOSED FIX:** `SET_ID` komutu sadece `g_system_state.mode == SYS_MODE_IDLE` durumundayken kabul edilmelidir.

---

### 📌 ECP-09: BUG-MED-01 — PT100 Dijital Moving Average Filtresi
- **FINDING ID:** `BUG-MED-01`
- **TITLE:** PT100 Temperature ADC Exponential Moving Average Filter
- **SEVERITY:** `P2 (Reliability)`
- **AFFECTED FILES:** [pt100_adc.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/pt100_adc.c)
- **PROPOSED FIX:** `adc_filtered = (adc_filtered * 7 + adc_raw) / 8;` filtresi uygulanarak gürültü spikeları engellenmelidir.
