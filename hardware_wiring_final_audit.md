# HARDWARE WIRING FINAL DATASHEET AUDIT & AUTHORIZATION REPORT
**Project:** EAGLEULTRASONİK  
**Phase:** 6.1 — Final Hardware Datasheet Audit + Wiring Authorization  
**Date:** 2026-08-11  
**Status:** PASS WITH CONDITIONS MET (Physical Wiring Authorized upon fulfilling mandatory conditions)

---

## 1. RESMİ DATASHEET VE DOKÜMAN KAYNAKLARI (DATASHEET SOURCES)

| Donanım Bileşeni | Üretici | Doküman Adı / Kodu | Sürüm / Revizyon |
| :--- | :--- | :--- | :--- |
| **STM32G474RE / NUCLEO-G474RE** | STMicroelectronics | UM2505 User Manual (STM32 Nucleo-64 boards) | Rev 5 |
| **STM32G474RE MCU** | STMicroelectronics | DS12288 Datasheet & RM0440 Reference Manual | Rev 6 |
| **ESP32-S3-N16R8** | Espressif Systems | ESP32-S3 Series Datasheet & Technical Reference Manual | v1.6 |
| **X9C103S Potansiyometre** | Renesas / Intersil | X9C102/103/104/503 Datasheet (FN8158) | Rev 4.00 |
| **Nextion NX4832T035** | ITEAD / Nextion | NX4832T035 Datasheet & Technical Specifications | v1.1 |

---

## 2. DONANIM MİMARİSİ İNCELEMESİ VE VERİFİKASYON (HARDWARE AUDIT)

### 2.1 Renesas/Intersil X9C103S İncelemesi
- **Fiziksel Pin Dizilimi (Pinout):**
  - Pin 1 = `INC` (Increment Control Input, Düşen kenar tetiklemeli)
  - Pin 2 = `U/D` (Up/Down Direction Input, HIGH = Artış, LOW = Azalış)
  - Pin 3 = `VH/RH` (Potansiyometre Üst Uç Terminali)
  - Pin 4 = `VSS` (GND / 0V Besleme)
  - Pin 5 = `VW/RW` (Potansiyometre Silecek / Wiper Çıkış Terminali)
  - Pin 6 = `VL/RL` (Potansiyometre Alt Uç Terminali)
  - Pin 7 = `CS` (Chip Select Input, Aktif LOW)
  - Pin 8 = `VCC` (+5V Besleme Voltajı)

- **Elektriksel Karakteristikler (Datasheet Doğrulaması):**
  - $V_{CC}$ Çalışma Aralığı: **4.5V — 5.5V** (Nominal 5.0V).
  - Kontrol Giriş Seviyeleri ($CS, U/\bar{D}, INC$):
    - $V_{IH}$ (Minimum Giriş Yüksek Voltajı): **2.0 V** (Maksimum: $V_{CC} + 1.0\text{V}$)
    - $V_{IL}$ (Maksimum Giriş Düşük Voltajı): **0.8 V** (Minimum: $-1.0\text{V}$)
  - Potansiyometre Terminal Sınırları ($V_H, V_L$):
    - $V_{SS} \le V_H, V_L \le V_{CC}$ (GND ile 5.0V arası gerilimler uygulanmalıdır).
  - Silecek Akım Sınırı ($I_W$): Maksimum continuous $\pm 3\text{ mA}$ ($\pm 4.4\text{ mA}$ mutlak maksimum).

- **STM32 3.3V GPIO — X9C 5V Lojik Uyumluluk Değerlendirmesi:**
  - STM32G474RE 3.3V GPIO çıkışında $V_{OH} \ge 2.9\text{V}$ seviyesindedir.
  - X9C103S $V_{CC} = 5.0\text{V}$ iken minimum yüksek lojik seviyesi $V_{IH,\text{min}} = 2.0\text{V}$'dir.
  - **Sonuç:** STM32 3.3V lojik seviyesi ($2.9\text{V} \dots 3.3\text{V}$), X9C'nin $2.0\text{V}$ eşiğinin oldukça üzerindedir. Dolayısıyla STM32 PB12, PB13, PB14 pinleri X9C entegresini seviye dönüştürücü (level shifter) veya pull-up direnci olmadan **doğrudan ve güvenle tetikleyebilir**.

- **Mevcut Devre Mimarisi Değerlendirmesi:**
  - $V_{CC} = \text{NUCLEO 5V}$ (ST-Link / Nucleo 5V regülatör hattı)
  - $V_{SS} = \text{GND}$ (Ortak Sistem Ground)
  - $V_H = 3.3\text{V}$ (NUCLEO 3.3V hattına bağlı)
  - $V_L = \text{GND}$ (Ortak GND hattına bağlı)
  - $VW = \text{STM32 PA0}$ (ADC1_IN1 / ADC2_IN1)
  - $CS = \text{PB12}$, $U/\bar{D} = \text{PB13}$, $INC = \text{PB14}$

- **Elektriksel Geçerlilik Raporu:**
  > [!IMPORTANT]
  > **Mimarinin Elektriksel Değerlendirmesi:** **GEÇERLİ (PASS WITH CONDITION)**  
  > 1. $V_H = 3.3\text{V}$ ve $V_L = 0\text{V}$ olarak bağlanması, silecek çıkış voltajının ($V_W$) $0\text{V} \dots 3.3\text{V}$ aralığında kalmasını garanti eder. Bu durum STM32 PA0 ADC giriş limitleri ($0\text{V} \dots 3.3\text{V}$) ile tam uyumludur ve ADC pinine aşırı voltaj gelmesini engeller.  
  > 2. Kontrol pinleri (CS, U/D, INC) 3.3V lojik seviye ile sorunsuz çalışmaktadır.  
  > 3. **Koşul:** VW pininden STM32 PA0 pini arasına $1\text{k}\Omega$ seri koruma direnci konulması zorunludur.

---

### 2.2 STM32 NUCLEO-G474RE Pin Mapping & Connector Audit (UM2505)
STMicroelectronics UM2505 User Manual ve NUCLEO-G474RE şematiğine göre pin haritalaması doğrulanmıştır:

| STM32 Pin | Bağlı Olduğu Konnektör Pinleri | İşlev / Sinyal Adı | Doğrulama Durumu |
| :--- | :--- | :--- | :--- |
| **PA0** | CN7-28 & CN8-1 (Arduino A0) | X9C Wiper ADC Girişi (ADC1_IN1) | **DOĞRULANDI** |
| **PA4** | CN7-32 & CN8-3 (Arduino A2) | Heater Feedback / Loopback Girişi | **DOĞRULANDI** |
| **PA6** | CN10-13 & CN5-5 (Arduino D12) | Triac Feedback / Loopback Girişi | **DOĞRULANDI** |
| **PB4** | CN10-27 & CN9-6 (Arduino D5) | X9C CS Loopback Test Girişi | **DOĞRULANDI** |
| **PB5** | CN10-29 & CN9-5 (Arduino D4) | X9C U/D Loopback Test Girişi | **DOĞRULANDI** |
| **PB6** | CN10-17 & CN5-3 (Arduino D10) | X9C INC Loopback Test Girişi | **DOĞRULANDI** |
| **PB10** | CN10-25 | USART3_TX (ESP32 RX'e giden) | **DOĞRULANDI** |
| **PB11** | CN7-18 | USART3_RX (ESP32 TX'den gelen) | **DOĞRULANDI** |
| **PB12** | CN10-16 | X9C CS Çıkışı | **DOĞRULANDI** |
| **PB13** | CN10-30 | X9C U/D Çıkışı | **DOĞRULANDI** |
| **PB14** | CN10-28 | X9C INC Çıkışı | **DOĞRULANDI** |
| **PB15** | CN10-26 | Heater Relay Çıkışı | **DOĞRULANDI** |
| **PC6** | CN10-4 | Triac Gate Çıkışı | **DOĞRULANDI** |
| **PC7** | CN5-2 (Arduino D9) & CN10-19 | Zero Cross Simülasyon Girişi | **DOĞRULANDI** |

> [!NOTE]
> **PC7 Çift Konnektör Açıklaması:**  
> PC7 pini, STM32G474RE entegresinin (LQFP64 paket 38 numaralı pini) tek bir fiziksel silikon bacağıdır. NUCLEO PCB üzerinde bu tek pin hem Arduino uyumlu **CN5 pin 2 (D9)** başlığına hem de ST Morpho **CN10 pin 19** başlığına paralel olarak yollanmıştır. Kablolama yapılırken bu iki konnektör pininden herhangi birinin kullanılması fiziksel olarak tamamen aynı MCU pinine ulaşır.

---

### 2.3 ESP32-S3-N16R8 GPIO & Memory Audit
ESP32-S3-N16R8 modülü dahili **16 MB Octal SPI Flash** ve **8 MB Octal SPI PSRAM** içermektedir. Octal bellek veriyolu dahili olarak GPIO33, GPIO34, GPIO35, GPIO36, GPIO37 ve GPIO26-32 hatlarını kullanır.

| ESP32 GPIO | Kullanım Amacı | Strapping Pin mi? | Octal Flash/PSRAM Çakışması Var mı? | Durum |
| :--- | :--- | :--- | :--- | :--- |
| **GPIO4** | Zero Cross 100Hz Simülatör Çıkışı | Hayır (Strapping pinler: 0, 3, 45, 46) | Yok (Tamamen Serbest) | **PASS** |
| **GPIO8** | STM32 UART TX (`STM_TXD`) | Hayır | Yok (Tamamen Serbest) | **PASS** |
| **GPIO16**| Nextion UART RX (`RXD2`) | Hayır (Power-up'ta ~60µs glitch var, zararsız) | Yok (Tamamen Serbest) | **PASS** |
| **GPIO17**| Nextion UART TX (`TXD2`) | Hayır (Power-up'ta ~60µs glitch var, zararsız) | Yok (Tamamen Serbest) | **PASS** |
| **GPIO18**| STM32 UART RX (`STM_RXD`) | Hayır | Yok (Tamamen Serbest) | **PASS** |

---

### 2.4 Nextion NX4832T035 HMI Ekran İncelemesi
- **Güç Gereksinimleri:**
  - $V_{CC}$: 5.0 V DC (Çalışma aralığı 4.75V – 7.0V). %100 parlaklıkta akım çekimi: ~145 mA.
  - Besleme Kaynağı: Harici USB / 5V Adaptör (5V, min 500mA).
- **UART Lojik Seviyeleri:**
  - Lojik Standart: 3.3V TTL.
  - $V_{IH,\text{min}}$ (RX Giriş Yüksek Eşiği): 2.0 V.
  - $V_{OH,\text{min}}$ (TX Çıkış Yüksek Seviyesi): 2.4V – 3.0V (3.3V TTL).
- **Bağlantı Doğrulaması:**
  - ESP32 GPIO17 (TX) $\to$ Nextion RX (Çıkıştan Girişe — DOĞRU)
  - Nextion TX $\to$ ESP32 GPIO16 (RX) (Çıkıştan Girişe — DOĞRU)

---

### 2.5 UART Haberleşme Hatları Audit

1. **STM32 <-> ESP32 Veriyolu (115,200 Baud):**
   - **ESP32 GPIO8 (TX)** $\to$ **STM32 PB11 (USART3_RX)** (Yön: ESP32 Çıkış $\to$ STM32 Giriş)
   - **STM32 PB10 (USART3_TX)** $\to$ **ESP32 GPIO18 (RX)** (Yön: STM32 Çıkış $\to$ ESP32 Giriş)
   - Lojik Seviye: 3.3V / 3.3V CMOS uyumlu.
   - Yönler: TX $\to$ RX çaprazlaması doğru.

2. **ESP32 <-> Nextion Veriyolu (9,600 Baud):**
   - **ESP32 GPIO17 (TX)** $\to$ **Nextion RX** (Yön: ESP32 Çıkış $\to$ Nextion Giriş)
   - **Nextion TX** $\to$ **ESP32 GPIO16 (RX)** (Yön: Nextion Çıkış $\to$ ESP32 Giriş)
   - Lojik Seviye: 3.3V TTL uyumlu.
   - Yönler: TX $\to$ RX çaprazlaması doğru.

---

### 2.6 Zero Cross Sinyal Simülasyonu & Güvenlik Auditi
- **Sinyal Yolu:** ESP32 GPIO4 $\to$ STM32 PC7 (CN5-2 / CN10-19)
- **Sinyal Tipi:** 3.3V TTL Kare Dalga, 100 Hz (%50 duty cycle, ESP32 `esp_timer` ile üretilir).
- **Amaç:** Masa testlerinde 220V AC şebeke bağlantısı olmadan STM32 yazılımının "Zero Cross Missing" (Fault Bit 0x04) hatasına düşmesini engellemek.

> [!CAUTION]
> **AŞIRI YÜKSEK GERİLİM VE GÜVENLİK UYARISI:**  
> Bu aşamada **220V AC ŞEBEKE GERİLİMİ KESİNLİKLE BAĞLANMAYACAKTIR**. PC7 pini sadece ESP32 GPIO4'ten gelen 3.3V DC TTL simülasyon sinyalini alacaktır. GPIO4 $\to$ PC7 hattına $1\text{k}\Omega$ seri koruma direnci takılması zorunludur.

---

## 3. GÜÇ MİMARİSİ VE GROUND TOPOLOJİSİ (POWER ARCHITECTURE)

### 3.1 Besleme Hatları Bağımsızlık Analizi
Masaüstü test düzeninde 3 ayrı USB güç kaynağı mevcuttur:
1. **NUCLEO-G474RE:** PC USB Port #1 üzerinden beslenir (ST-LINK ve STM32 5V/3.3V hatları).
2. **ESP32-S3:** PC USB Port #2 / USB Hub üzerinden beslenir.
3. **Nextion Ekran:** Ayrı USB / 5V 500mA adaptör üzerinden beslenir.
4. **X9C103S Entegresi:** NUCLEO 5V ve 3.3V hatlarından beslenir.

> [!WARNING]
> **PARALEL 5V BESLEME YASAĞI (NO PARALLEL 5V RAILS):**  
> NUCLEO 5V hattı, ESP32 5V hattı ve Nextion 5V hattı **FİZİKSEL OLARAK KESİNLİKLE BİRLEŞTİRİLMEYECEKTİR!** Farklı USB portlarının 5V regülatörlerini paralel bağlamak veriyolu üzerinde ters akımlara (backfeeding), voltaj dengesizliklerine ve USB portlarının zarar görmesine yol açar.

### 3.2 Ortak Ground Topolojisi (Common Logic GND Bus)
Farklı güç kaynaklarından beslenen tüm modüllerin lojik sinyal seviyelerinin (UART, Zero-Cross, X9C kontrol) doğru referanslanabilmesi için **Ortak GND Bus (Logic GND)** oluşturulmalıdır.

```
       +-------------------------------------------------------+
       |                  ORTAK LOGIC GND BUS                  |
       +-------+---------------+---------------+---------------+
               |               |               |
        +------+------+ +------+------+ +------+------+ +------+------+
        | NUCLEO GND  | |  ESP32 GND  | | NEXTION GND | | X9C VSS(P4) |
        +-------------+ +-------------+ +-------------+ +-------------+
```

- NUCLEO GND $\longleftrightarrow$ ESP32 GND
- NUCLEO GND $\longleftrightarrow$ Nextion GND
- NUCLEO GND $\longleftrightarrow$ X9C VSS (Pin 4) & VL (Pin 6)

---

## 4. SERİ DİRENÇ AUDITI VE TEKNİK GEREKÇELENDİRME (SERIES RESISTOR AUDIT)

### 4.1 Masa Testi Loopback Hatları
Aşağıdaki 5 test loopback hattının her birinde **fiziksel $1\text{k}\Omega$ seri direnç** kullanılması zorunludur:
1. **PB15 (Heater Relay Out) $\to$ PA4 (Feedback In)**
2. **PC6 (Triac Gate Out) $\to$ PA6 (Feedback In)**
3. **PB12 (X9C CS Out) $\to$ PB4 (Loopback In)**
4. **PB13 (X9C U/D Out) $\to$ PB5 (Loopback In)**
5. **PB14 (X9C INC Out) $\to$ PB6 (Loopback In)**

### 4.2 Lojik Konfigürasyon vs Fiziksel Seri Direnç Farkı
> [!IMPORTANT]
> **Neden MCU Dahili Pull-Up/Pull-Down Dirençleri Fiziksel Seri Direncin Yerine Geçemez?**  
> - STM32/ESP32 GPIO pinlerindeki dahili pull-up veya pull-down dirençleri ($30\text{k}\Omega \dots 50\text{k}\Omega$), pini yüksek empedanslı giriş (Input) modundayken belirli bir voltaj seviyesine bağlayan paralel bias elemanlarıdır.  
> - Eğer iki MCU pini birbiriyle doğrudan (0 $\Omega$) bağlanırsa ve yazılımsal bir hata veya başlatma anında pini çıkış (Output) yapılarak biri HIGH (3.3V), diğeri LOW (0V) sürülürse, iki pin arasında tam bir kısa devre oluşur. Çekilen akım $I_{out} > 40\text{mA}$ seviyesine çıkarak bacaklardaki çıkış sürücü MOSFET'lerini kalıcı olarak yakar.  
> - **Fiziksel $1\text{k}\Omega$ seri direnç**, sinyal hattı üzerine seri olarak yerleştirilir. Çakışma anında çekilecek maksimum akımı $I = \frac{3.3\text{V}}{1000\Omega} = 3.3\text{ mA}$ seviyesine sınırlandırarak MCU pinlerini kesin olarak korur. Dahili pull-up/down dirençlerinin bu akım sınırlama fonksiyonuyla hiçbir ilgisi yoktur.

---

## 5. KAPSAMLI MASTER DİRENÇ TABLOSU (MASTER RESISTOR MATRIX)

| SOURCE | DESTINATION | RESISTOR | RESISTOR VALUE | RESISTOR LOCATION | REASON | MANDATORY / OPTIONAL |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **ESP32 GPIO8 (TX)** | **STM32 PB11 (RX)** | Evet | 1 k$\Omega$ | Hat üzerinde seri | Boot glitch / çakışma akım sınırlama | **MANDATORY** |
| **STM32 PB10 (TX)** | **ESP32 GPIO18 (RX)** | Evet | 1 k$\Omega$ | Hat üzerinde seri | Boot glitch / çakışma akım sınırlama | **MANDATORY** |
| **ESP32 GPIO17 (TX)** | **Nextion RX** | Evet | 1 k$\Omega$ | Hat üzerinde seri | Sinyal kenarı yumuşatma / hat koruma | OPTIONAL (1kΩ önerilir) |
| **Nextion TX** | **ESP32 GPIO16 (RX)** | Evet | 1 k$\Omega$ | Hat üzerinde seri | Sinyal kenarı yumuşatma / hat koruma | OPTIONAL (1kΩ önerilir) |
| **ESP32 GPIO4 (ZC_SIM)**| **STM32 PC7 (ZC_IN)** | Evet | 1 k$\Omega$ | Hat üzerinde seri | Çıkış çakışması ve pini koruma | **MANDATORY** |
| **STM32 PB12 (X9C CS)** | **X9C Pin 7 (CS)** | Hayır | 0 $\Omega$ (Doğrudan) | N/A | Yüksek empedans giriş, doğrudan sürüş | OPTIONAL (Doğrudan) |
| **STM32 PB13 (X9C U/D)**| **X9C Pin 2 (U/D)** | Hayır | 0 $\Omega$ (Doğrudan) | N/A | Yüksek empedans giriş, doğrudan sürüş | OPTIONAL (Doğrudan) |
| **STM32 PB14 (X9C INC)**| **X9C Pin 1 (INC)** | Hayır | 0 $\Omega$ (Doğrudan) | N/A | Yüksek empedans giriş, doğrudan sürüş | OPTIONAL (Doğrudan) |
| **X9C Pin 5 (VW)** | **STM32 PA0 (ADC)** | Evet | 1 k$\Omega$ | Hat üzerinde seri | PA0 ADC girişini koruma & akım limitleme | **MANDATORY** |
| **STM32 PB15 (Heater)**| **STM32 PA4 (Feedback)**| Evet | 1 k$\Omega$ | Hat üzerinde seri | Loopback çıkış çakışma koruması | **MANDATORY** |
| **STM32 PC6 (Triac)** | **STM32 PA6 (Feedback)**| Evet | 1 k$\Omega$ | Hat üzerinde seri | Loopback çıkış çakışma koruması | **MANDATORY** |
| **STM32 PB12 (CS)** | **STM32 PB4 (Loopback)**| Evet | 1 k$\Omega$ | Hat üzerinde seri | Loopback çıkış çakışma koruması | **MANDATORY** |
| **STM32 PB13 (U/D)** | **STM32 PB5 (Loopback)**| Evet | 1 k$\Omega$ | Hat üzerinde seri | Loopback çıkış çakışma koruması | **MANDATORY** |
| **STM32 PB14 (INC)** | **STM32 PB6 (Loopback)**| Evet | 1 k$\Omega$ | Hat üzerinde seri | Loopback çıkış çakışma koruması | **MANDATORY** |
| **Nucleo 5V Hattı** | **X9C Pin 8 (VCC)** | Hayır | Doğrudan Kablo | N/A | X9C 5V besleme | **MANDATORY** |
| **Nucleo 3.3V Hattı**| **X9C Pin 3 (VH)** | Hayır | Doğrudan Kablo | N/A | Pot max gerilimi 3.3V'a sabitleme | **MANDATORY** |
| **Nucleo GND Hattı** | **X9C Pin 6 (VL)** | Hayır | Doğrudan Kablo | N/A | Pot min gerilimi 0V'a (GND) sabitleme | **MANDATORY** |
| **Nucleo GND** | **ESP32 GND** | Hayır | Doğrudan Kablo | GND Bus | Ortak referans ground | **MANDATORY** |
| **Nucleo GND** | **Nextion GND** | Hayır | Doğrudan Kablo | GND Bus | Ortak referans ground | **MANDATORY** |
| **Nucleo GND** | **X9C Pin 4 (VSS)** | Hayır | Doğrudan Kablo | GND Bus | Ortak referans ground | **MANDATORY** |

---

## 6. BAĞLANTI KARAR MATRİSİ VE KESİN KABLOLAMA ONAYI (DECISION MATRIX)

| Bağlantı Grubu / Sinyal | Durum | Şartlar / Açıklama |
| :--- | :--- | :--- |
| **1. X9C103S Bağlantısı** | **PASS WITH CONDITION** | 5V VCC / 3.3V VH / 0V VL mimarisi elektriksel olarak uygundur. VW $\to$ PA0 hattında 1k$\Omega$ seri direnç şarttır. |
| **2. NUCLEO-G474RE Pin Haritası** | **PASS** | Tüm pin konumları UM2505 dokümanından doğrulanmıştır. PC7 pininin CN5-2 ve CN10-19 paralelliği belgelenmiştir. |
| **3. ESP32-S3-N16R8 Pin Haritası** | **PASS** | GPIO4, 8, 16, 17, 18 pinlerinin tamamı serbesttir. Octal Flash/PSRAM hatlarıyla çakışma yoktur. |
| **4. Nextion NX4832T035 Bağlantısı** | **PASS** | 5V harici besleme ve 3.3V TTL seri haberleşme uyumludur. |
| **5. UART Hatları (STM32/ESP32/Nextion)** | **PASS** | Sinyal yönleri (TX $\to$ RX) ve baud hızları (115200 / 9600) doğrulanmıştır. |
| **6. Zero Cross Simülasyonu** | **PASS WITH CONDITION** | 220V AC şebeke bağlantısı KESİNLİKLE YAPILMAYACAKTIR. Sadece ESP32 GPIO4'ten 3.3V TTL 100Hz verilecek ve 1k$\Omega$ seri direnç takılacaktır. |
| **7. Güç Mimarisi ve Grounding** | **PASS WITH CONDITION** | Ayrı USB 5V kaynakları birleştirilmeyecek, tüm GND noktaları tek bir Ortak Logic GND bus üzerinde buluşturulacaktır. |
| **8. Seri Direnç Standartları** | **PASS WITH CONDITION** | Tüm 5 loopback hattında, UART hatlarında ve ZC hattında 1k$\Omega$ fiziksel direnç kullanımı zorunludur. |

---

## 7. FINAL KABLOLAMA ONAY KARARI (WIRING AUTHORIZATION DECISION)

> [!IMPORTANT]
> **FİZİKSEL KABLOLAMA ONAYI: ONAYLANDI (PASS WITH CONDITIONS MET)**  
> 
> EAGLEULTRASONİK projesinin Phase 6.1 kapsamındaki donanım ve pin haritalama denetimi tamamlanmıştır. Hata (FAIL) veya Bilinmeyen (UNKNOWN) durumunda hiçbir bağlantı kalmamıştır.  
> 
> Aşağıdaki **6 ZORUNLU KABUL ŞARITI** yerine getirildiği takdirde fiziksel kablolamanın yapılmasına **RESMEN ONAY VERİLMİŞTİR**:
> 1. 220V AC şebeke gerilimi kesinlikle devreye bağlanmayacaktır.
> 2. Farklı USB portlarından gelen 5V besleme hatları paralel birleştirilmeyecektir.
> 3. Bütün kartların GND pinleri (Nucleo GND, ESP32 GND, Nextion GND, X9C VSS) tek bir ortak GND hattında birleştirilecektir.
> 4. X9C VH pinine 3.3V, VL pinine GND bağlanacaktır (VW çıkışı 0-3.3V aralığında kalacaktır).
> 5. Belirtilen 5 loopback hattının her birine ve VW $\to$ PA0 hattına $1\text{k}\Omega$ fiziksel seri direnç takılacaktır.
> 6. ESP32 GPIO4 $\to$ STM32 PC7 hattına $1\text{k}\Omega$ seri direnç takılacaktır.
