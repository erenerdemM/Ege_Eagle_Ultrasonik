# EAGLEULTRASONİK — NİHAİ FİZİKSEL MONTAJ VE KABLOLAMA PAKETİ
**Dosya Adı:** `hardware_wiring_final_physical_package.md`  
**Proje:** EAGLEULTRASONİK  
**Aşama:** PHASE 6.1 — PROMPT 2: Final Physical Wiring Package + Resistor Placement + Power Distribution  
**Tarih:** 2026-08-11  
**Durum:** READY WITH CONDITIONS (Şartlı Onay Verildi - 6 Zorunlu Kabul Şartı)

---

## 1. NUCLEO-G474RE CONNECTOR MAPPING

### 1.1 PC7 Pin Yetkili (Authoritative) Tanımı
> [!IMPORTANT]
> **PC7 PİN TANIMI VE KONNEKTÖR PARALELLİĞİ:**  
> PC7 pini, STM32G474RE mikrokontrolcüsünün (LQFP64 paketi Pin 38) tek bir fiziksel silikon bacağıdır. NUCLEO-G474RE kartı üzerinde bu silikon pini hem **CN5 Pin 2 (Arduino D9)** hem de **CN10 Pin 19 (ST Morpho)** konnektör başlıklarına PCB yolları ile paralel olarak bağlanmıştır.  
> **CN5 Pin 2 ile CN10 Pin 19 iki farklı MCU pini değildir.** İkisi de tam olarak aynı fiziksel MCU bacağına erişim sağlar. Fiziksel kablolamada bu iki başlık pininden herhangi biri kullanılabilir.

### 1.2 STM32 NUCLEO-G474RE Master Pin Haritası

| MCU Pin | Arduino Konnektör / Pin | Morpho Konnektör / Pin | Donanım Fonksiyonu | STM32 CubeMX GPIO Modu | Projedeki Kullanım Amacı |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **PA0** | CN8 Pin 1 (A0) | CN7 Pin 28 | ADC1_IN1 / ADC2_IN1 | Analog Mode | X9C103S Silecek ($V_W$) Voltaj Ölçümü |
| **PA4** | CN8 Pin 3 (A2) | CN7 Pin 32 | ADC2_IN17 / DAC1_OUT1 | Input (NOPULL) | Heater Relay Loopback Geribildirim Girişi |
| **PA6** | CN5 Pin 5 (D12)| CN10 Pin 13 | ADC2_IN3 / SPI1_MISO | Input (NOPULL) | Triac Gate Loopback Geribildirim Girişi |
| **PB4** | CN9 Pin 6 (D5) | CN10 Pin 27 | TIM3_CH1 / GPIO | Input (NOPULL) | X9C CS Loopback Test Girişi |
| **PB5** | CN9 Pin 5 (D4) | CN10 Pin 29 | TIM3_CH2 / GPIO | Input (NOPULL) | X9C U/D Loopback Test Girişi |
| **PB6** | CN5 Pin 3 (D10)| CN10 Pin 17 | TIM4_CH1 / GPIO | Input (NOPULL) | X9C INC Loopback Test Girişi |
| **PB10**| N/A (Morpho Only)| CN10 Pin 25 | USART3_TX | AF Push-Pull (AF7) | STM32 $\to$ ESP32 Telemetri TX |
| **PB11**| N/A (Morpho Only)| CN7 Pin 18 | USART3_RX | AF Push-Pull (AF7) | ESP32 $\to$ STM32 Komut RX |
| **PB12**| N/A (Morpho Only)| CN10 Pin 16 | GPIO Output | Output Push-Pull (NOPULL) | X9C Chip Select ($CS$) Kontrolü |
| **PB13**| N/A (Morpho Only)| CN10 Pin 30 | GPIO Output | Output Push-Pull (NOPULL) | X9C Up/Down ($U/\bar{D}$) Yön Kontrolü |
| **PB14**| N/A (Morpho Only)| CN10 Pin 28 | GPIO Output | Output Push-Pull (NOPULL) | X9C Increment ($\bar{\text{INC}}$) Tetikleme |
| **PB15**| N/A (Morpho Only)| CN10 Pin 26 | GPIO Output | Output Push-Pull (PULLDOWN)| Heater Röle Tetikleme Çıkışı |
| **PC6** | N/A (Morpho Only)| CN10 Pin 4 | GPIO Output | Output Push-Pull (PULLDOWN)| Triac Gate Tetikleme Pals Çıkışı |
| **PC7** | CN5 Pin 2 (D9) | CN10 Pin 19 | TIM3_CH2 / EXTI7 | Input (NOPULL) | Zero Cross 100Hz Simülasyon Girişi |

---

## 2. FINAL 5V / 3.3V POWER ARCHITECTURE & GROUND ISOLATION

### 2.1 Güç Hatları Fiziksel İzolasyon Analizi (Power Rails Isolation)

Gerçek masaüstü test senaryosunda 3 ayrı USB güç kaynağı aktiftir:
- NUCLEO-G474RE $\to$ Bilgisayar USB Kablosu #1 (ST-LINK 5V regülatörü)
- ESP32-S3 $\to$ Bilgisayar USB Kablosu #2 (USB VBUS 5V)
- Nextion Ekran $\to$ Bilgisayar USB Kablosu #3 / Ayrı 5V 500mA USB Adaptörü
- X9C103S Entegresi $\to$ NUCLEO 5V ve 3.3V hatları

> [!WARNING]
> **5V VOLTAJ HATLARININ FİZİKSEL BİRLEŞTİRİLME YASAĞI:**  
> **NUCLEO 5V, ESP32 5V ve Nextion 5V hatları FİZİKSEL OLARAK BİRBİRİNE KESİNLİKLE BAĞLANMAMALIDIR!**  
> **Gerekçe:** Farklı USB portlarının regülatör voltajları arasında çok küçük farklar ($4.95\text{V} \dots 5.10\text{V}$) mevcuttur. Aktif 5V regülatör çıkışlarını paralel bağlamak, düşük empedanslı döngüler üzerinden yüksek ters akımlara (backfeeding) sebep olur. Bu durum regülatörlerin aşırı ısınmasına, PC USB port korumalarının açılmasına ve modüllerin zarar görmesine yol açar.

### 2.2 Ortak Ground (GND) Zorunluluğu ve Gerekçelendirme

> [!IMPORTANT]
> **Üç Cihaz Ayrı USB Kablolarıyla Bilgisayara Bağlıyken GND'lerin Ortaklanması Güvenli ve Gerekli midir?**  
> **EVET! KESİNLİKLE GÜVENLİ VE ZORUNLUDUR.**  
> **Gerekçe (Datasheet ve Board Manuel Standartları):**  
> 1. USB kablolarının GND hatları bilgisayar anakartı üzerinden birleşse de, kablo dirençleri ve anakart üzerindeki parazit akımlar nedeniyle modüller arasında küçük voltaj farkları (Ground Offset / Ground Bounce) oluşur.  
> 2. Tek uçlu (single-ended) dijital haberleşme sinyallerinde (UART 115200 baud, X9C lojik hatları, Zero Cross palsı), mantıksal LOW ($0\text{V} \dots 0.8\text{V}$) ve HIGH ($2.0\text{V} \dots 3.3\text{V}$) seviyelerinin doğru algılanabilmesi için tüm modüllerin **aynı 0V voltaj referansına** sahip olması şarttır.  
> 3. Breadboard üzerinde NUCLEO GND, ESP32 GND, Nextion GND ve X9C VSS uçlarının kalın ve kısa jenerik kablolarla doğrudan bağlanması, ortak bir **Signal GND (Ortak Referans)** oluşturur ve haberleşme hatalarını (CRC hataları, framming errors) engeller.

### 2.3 Power Rail vs Signal Ground Ayrımı

- **POWER RAIL (+5V / +3.3V):** Akım taşıyan güç besleme hatlarıdır. Her cihaz kendi bağımsız 5V kaynağını kullanmalıdır. Birbirine BAĞLANMAZ.
- **SIGNAL GROUND (GND):** Sinyal voltajlarının ($V = V_{\text{sinyal}} - V_{\text{GND}}$) ölçüldüğü sıfır voltaj referans düzlemidir. Kesintisiz haberleşme için BÜTÜN MODÜLLERİN GND'LERİ BİRLEŞTİRİLİR.

---

## 3. X9C103S FINAL WIRING AUDIT

### 3.1 X9C103S Pin Bağlantı Şeması

- **X9C Pin 1 ($\bar{\text{INC}}$)** $\to$ **STM32 PB14** (Doğrudan veya 1k$\Omega$ Loopback Seri Direnci)
- **X9C Pin 2 ($U/\bar{D}$)** $\to$ **STM32 PB13** (Doğrudan veya 1k$\Omega$ Loopback Seri Direnci)
- **X9C Pin 3 ($V_H$)** $\to$ **NUCLEO 3.3V Hattı** (**KESİNLİKLE 5V DEĞİL!**)
- **X9C Pin 4 ($V_{SS}$)** $\to$ **Ortak Signal GND Bus**
- **X9C Pin 5 ($V_W$)** $\to$ **1k$\Omega$ Seri Direnç** $\to$ **STM32 PA0 (ADC1_IN1)**
- **X9C Pin 6 ($V_L$)** $\to$ **Ortak Signal GND Bus**
- **X9C Pin 7 ($\bar{\text{CS}}$)** $\to$ **STM32 PB12** (Doğrudan veya 1k$\Omega$ Loopback Seri Direnci)
- **X9C Pin 8 ($V_{CC}$)** $\to$ **NUCLEO 5V Hattı**

### 3.2 Operating Conditions Doğrulaması ($V_{CC}=5\text{V}, V_H=3.3\text{V}, V_L=\text{GND}$)

Renesas / Intersil X9C103S Datasheet (FN8158) verilerine göre:
1. **$V_{CC}$ Besleme:** $4.5\text{V} \dots 5.5\text{V}$ (NUCLEO 5V hattından 5.0V verilmesi **UYGUNDUR**).
2. **Potansiyometre Terminal Gerilimleri ($V_H, V_L$):** Datasheet kuralı: $V_{SS} \le V_H, V_L \le V_{CC}$.  
   Burada $V_H = 3.3\text{V}$, $V_L = 0\text{V}$, $V_{CC} = 5.0\text{V}$, $V_{SS} = 0\text{V}$.  
   $0\text{V} \le 3.3\text{V} \le 5.0\text{V}$ şartı **%100 SAĞLANMAKTADIR**.
3. **Lojik Giriş Seviyeleri ($\bar{\text{CS}}, U/\bar{D}, \bar{\text{INC}}$):**  
   Datasheet $V_{IH,\text{min}} = 2.0\text{V}$. STM32 3.3V GPIO çıkışı $V_{OH} \ge 2.9\text{V}$ olduğu için X9C dijital girişleri 3.3V lojik seviye ile **sorunsuz ve güvenle tetiklenir**.
4. **Silecek Çıkışı ($V_W$) Voltaj Güvenliği:**  
   $V_H = 3.3\text{V}$ yapıldığından potansiyometre çıkışı $V_W$ maksimum $3.3\text{V}$ üretebilir. Bu durum STM32 PA0 pini (ADC giriş aralığı $0 \dots 3.3\text{V}$) için **%100 AŞIRI VOLTAJ KORUMASI SAĞLAR**.

> [!NOTE]
> **ÇELİŞKİ / BLOCKING ISSUE DURUMU:**  
> İnceleme sonucunda **HİÇBİR BLOCKING ISSUE BULUNMAMIŞTIR**. $V_{CC}=5\text{V}, V_H=3.3\text{V}, V_L=\text{GND}$ mimarisi elektriksel ve mantıksal olarak tam doğrulanmıştır.

---

## 4. RESISTOR PLACEMENT MATRIX & DETAILED AUDIT

### 4.1 Master Seri Direnç Yerleşim Tablosu

| # | SIGNAL | SOURCE PIN | RESISTOR | RESISTOR LOCATION | DESTINATION PIN | REQUIRED? |
| :-: | :--- | :--- | :-: | :--- | :--- | :-: |
| **1** | STM32 UART RX | ESP32 GPIO8 | **1k$\Omega$** | GPIO8 bacağı ile PB11 arasına seri | STM32 PB11 | **MANDATORY** |
| **2** | ESP32 UART RX | STM32 PB10 | **1k$\Omega$** | PB10 bacağı ile GPIO18 arasına seri | ESP32 GPIO18 | **MANDATORY** |
| **3** | Nextion RX | ESP32 GPIO17 | **1k$\Omega$** | GPIO17 bacağı ile Nextion RX arasına seri | Nextion RX | OPTIONAL (Önerilir) |
| **4** | ESP32 RX | Nextion TX | **1k$\Omega$** | Nextion TX ile GPIO16 arasına seri | ESP32 GPIO16 | OPTIONAL (Önerilir) |
| **5** | Zero Cross Sim | ESP32 GPIO4 | **1k$\Omega$** | GPIO4 bacağı ile PC7 (CN5-2/CN10-19) arasına seri | STM32 PC7 | **MANDATORY** |
| **6** | Heater Feedback | STM32 PB15 | **1k$\Omega$** | PB15 bacağı ile PA4 arasına seri | STM32 PA4 | **MANDATORY** |
| **7** | Triac Feedback | STM32 PC6 | **1k$\Omega$** | PC6 bacağı ile PA6 arasına seri | STM32 PA6 | **MANDATORY** |
| **8** | CS Loopback | STM32 PB12 | **1k$\Omega$** | PB12 bacağı ile PB4 arasına seri | STM32 PB4 | **MANDATORY** |
| **9** | U/D Loopback | STM32 PB13 | **1k$\Omega$** | PB13 bacağı ile PB5 arasına seri | STM32 PB5 | **MANDATORY** |
| **10**| INC Loopback | STM32 PB14 | **1k$\Omega$** | PB14 bacağı ile PB6 arasına seri | STM32 PB6 | **MANDATORY** |
| **11**| Wiper Sense | X9C Pin 5 (VW)| **1k$\Omega$** | X9C Pin 5 bacağı ile PA0 arasına seri | STM32 PA0 | **MANDATORY** |

---

### 4.2 Sinyal Bitişiğinde Direnç Analizi ve Gerekçelendirme

1. **ESP32 GPIO8 $\to$ STM32 PB11 (UART):**
   - **Fiziksel 1k$\Omega$ Gerekli mi?** EVET (MANDATORY).
   - **Neden?** ESP32 açılış esnasında bootloader mesajları yayar. STM32 PB11 pini başlatma esnasında yanlışlıkla çıkış yapılırsa 0$\Omega$ hatta kısa devre oluşur.
   - **Tam Konumu:** ESP32 GPIO8 çıkış pini ile STM32 PB11 giriş pini arasına seri bağlanır. Yönü fark etmez.
   - **Direnç Olmadan Bağlanırsa:** Reset anında pinler çakışırsa maksimum bacak akımı ($>40\text{mA}$) aşılır, IO sürücü katı yanar.

2. **STM32 PB10 $\to$ ESP32 GPIO18 (UART):**
   - **Fiziksel 1k$\Omega$ Gerekli mi?** EVET (MANDATORY).
   - **Neden?** STM32 PB10 TX çıkışını ESP32 GPIO18 RX girişine bağlarken olası yazılım konfigürasyon hatalarında akımı $3.3\text{mA}$ ile sınırlar.
   - **Tam Konumu:** STM32 PB10 pini ile ESP32 GPIO18 pini arasına seri bağlanır. Yönü fark etmez.
   - **Direnç Olmadan Bağlanırsa:** Konfigürasyon hatasında ESP32 GPIO18 pini zarar görür.

3. **ESP32 GPIO17 $\to$ Nextion RX:**
   - **Fiziksel 1k$\Omega$ Gerekli mi?** OPTIONAL (Önerilir).
   - **Neden?** Nextion RX yüksek empedanslı 3.3V TTL girişidir. Direnç sinyal kenar parazitlerini yumuşatır ve sıcak tak-çıkar (hot-plugging) koruması sağlar.
   - **Tam Konumu:** ESP32 GPIO17 ile Nextion RX klemensi arasına seri. Yön fark etmez.
   - **Direnç Olmadan Bağlanırsa:** Doğrudan çalışır ancak sıcak tak-çıkarda ESD riski oluşur.

4. **Nextion TX $\to$ ESP32 GPIO16:**
   - **Fiziksel 1k$\Omega$ Gerekli mi?** OPTIONAL (Önerilir).
   - **Neden?** Nextion TX 3.3V TTL çıkışıdır. ESP32 GPIO16 pini giriş modundadır. Akım sınırlama koruması sağlar.
   - **Tam Konumu:** Nextion TX klemensi ile ESP32 GPIO16 pini arasına seri. Yön fark etmez.
   - **Direnç Olmadan Bağlanırsa:** Doğrudan çalışır ancak pin koruması azalır.

5. **ESP32 GPIO4 $\to$ STM32 PC7 (Zero Cross Sim):**
   - **Fiziksel 1k$\Omega$ Gerekli mi?** EVET (MANDATORY).
   - **Neden?** ESP32 GPIO4 100Hz kare dalga çıkışıdır. STM32 PC7 pini yanlışlıkla çıkış LOW sürülürse tam kısa devre oluşur.
   - **Tam Konumu:** ESP32 GPIO4 ile STM32 PC7 (CN5-2 veya CN10-19) arasına seri. Yön fark etmez.
   - **Direnç Olmadan Bağlanırsa:** PC7 ve GPIO4 pin sürücüleri çakışma anında kalıcı olarak yanar.

6. **STM32 PB15 $\to$ PA4 (Heater Loopback):**
   - **Fiziksel 1k$\Omega$ Gerekli mi?** EVET (MANDATORY).
   - **Neden?** PB15 aktif Push-Pull çıkıştır (Röle sürer). PA4 ise ADC/Feedback girişidir. PA4 pini yazılım hatasıyla çıkış LOW sürülürse PB15 HIGH çıkışı ile 3.3V tam kısa devre ($I > 40\text{mA}$) oluşturur. Direnç akımı $3.3\text{mA}$'e sınırlar.
   - **Tam Konumu:** STM32 PB15 pini ile STM32 PA4 pini arasına seri. Yön fark etmez.
   - **Direnç Olmadan Bağlanırsa:** Test sırasında iki MCU pini birden yanar.

7. **STM32 PC6 $\to$ PA6 (Triac Loopback):**
   - **Fiziksel 1k$\Omega$ Gerekli mi?** EVET (MANDATORY).
   - **Neden?** PC6 aktif Triac Gate çıkışıdır. PA6 ise Geribildirim girişidir. İki çıkışın çakışmasını engeller.
   - **Tam Konumu:** STM32 PC6 pini ile STM32 PA6 pini arasına seri. Yön fark etmez.
   - **Direnç Olmadan Bağlanırsa:** MCU pinleri yanar.

8. **STM32 PB12 $\to$ PB4 (X9C CS Loopback):**
   - **Fiziksel 1k$\Omega$ Gerekli mi?** EVET (MANDATORY).
   - **Neden?** PB12 (CS çıkışı) ile PB4 (test girişi) arasındaki olası çift çıkış sürüş çakışmasını engeller.
   - **Tam Konumu:** STM32 PB12 pini ile STM32 PB4 pini arasına seri. Yön fark etmez.
   - **Direnç Olmadan Bağlanırsa:** Pin çakışmasında sürücüler zarar görür.

9. **STM32 PB13 $\to$ PB5 (X9C U/D Loopback):**
   - **Fiziksel 1k$\Omega$ Gerekli mi?** EVET (MANDATORY).
   - **Neden?** PB13 (U/D çıkışı) ile PB5 (test girişi) arasındaki çakışmayı engeller.
   - **Tam Konumu:** STM32 PB13 pini ile STM32 PB5 pini arasına seri. Yön fark etmez.
   - **Direnç Olmadan Bağlanırsa:** Pin çakışmasında sürücüler zarar görür.

10. **STM32 PB14 $\to$ PB6 (X9C INC Loopback):**
    - **Fiziksel 1k$\Omega$ Gerekli mi?** EVET (MANDATORY).
    - **Neden?** PB14 (INC çıkışı) ile PB6 (test girişi) arasındaki çakışmayı engeller.
    - **Tam Konumu:** STM32 PB14 pini ile STM32 PB6 pini arasına seri. Yön fark etmez.
    - **Direnç Olmadan Bağlanırsa:** Pin çakışmasında sürücüler zarar görür.

11. **X9C Pin 5 (VW) $\to$ STM32 PA0 (Wiper Sense):**
    - **Fiziksel 1k$\Omega$ Gerekli mi?** EVET (MANDATORY).
    - **Neden?** X9C silecek akımını ($I_W$) maksimum $3.3\text{mA}$ ile sınırlandırır ve PA0 ADC girişini aşırı voltaj/ESD sıçramalarından korur.
    - **Tam Konumu:** X9C Pin 5 (VW) bacağı ile STM32 PA0 pini arasına seri. Yön fark etmez.
    - **Direnç Olmadan Bağlanırsa:** Silecek akım limiti ($3\text{mA}$) aşılabilir veya PA0 ADC katı zarar görebilir.

---

## 5. INTERNAL GPIO RESISTOR KONUSU VE YAZILIM RAPORU

### 5.1 Kavramsal Ayrım: Internal Pull-Up/Down vs Physical 1kΩ Series Resistor

```
DAHİLİ (INTERNAL) PULL-UP/DOWN:               FİZİKSEL (PHYSICAL) SERİ DİRENÇ:
     VDD / GND                                    MCU Pin A         MCU Pin B
        |                                             |                 |
     [30k-50k] (Paralel Bias)                         +-----[ 1kΩ ]-----+
        |                                               (Seri Akım Sınırlayıcı)
    MCU Pin
```

- **GPIO Internal Pull-Up / Pull-Down Dirençleri:**
  - **İşlevi:** Giriş (Input) modundaki boşta kalan (floating) pinleri dahili olarak $30\text{k}\Omega \dots 50\text{k}\Omega$ yüksek empedanslı dirençlerle $V_{DD}$ veya $V_{SS}$ seviyesine çeker.
  - **Neyi Korur:** Pin boşta kaldığında çevresel gürültüden etkilenip kararsız lojik seviye üretmesini engeller.
  - **Seri Direnç Yerine Geçebilir mi?** **HAYIR! KESİNLİKLE GEÇEMEZ.** Çünki bu dirençler sinyal yoluna SERİ bağlı değildir. İki pin birleştiğinde oluşan yüksek kısa devre akımını sınırlayamazlar.

- **Physical 1kΩ Series Resistor (Fiziksel Seri Direnç):**
  - **İşlevi:** İki aktif donanım pini arasındaki sinyal iletkenine SERİ olarak bağlanır. Ohm Kanunu ($I = \Delta V / R$) gereği hat üzerinden geçebilecek akımı maksimum $I = 3.3\text{V} / 1000\Omega = 3.3\text{mA}$ seviyesine sınırlar.
  - **Hangi Durumda Kullanılır:** Çıkış çakışması (Output Contention), reset anı belirsizlikleri, inter-board UART hatları ve loopback testlerinde.
  - **Neden Yazılımla Emüle Edilemez?** MCU register ayarları (MODER, PUPDR) sadece pin modunu değiştirebilir; fiziksel silikon bacağı üzerine seri $1\text{k}\Omega$ direnç ekleyemez.

### 5.2 Mevcut Firmware / CubeMX Konfigürasyon İnceleme Raporu
- In `STM32/main.c` (`MX_GPIO_Init`):
  - `HEATER_RELAY_Pin` (PB15) & `TRIAC_GATE_Pin` (PC6) `GPIO_MODE_OUTPUT_PP` ve `GPIO_PULLDOWN` olarak tanımlanmıştır.  
    *İnceleme Notu:* Push-Pull çıkış modunda dahili pull-down aktif çalışmaz ancak MCU reset anında pini LOW tutar. Konfigürasyon doğrudur.
  - Loopback giriş pinleri (PB4, PB5, PB6, PA4, PA6) varsayılan giriş modundadır.  
    *İnceleme Notu:* Donanım testi esnasında bu pinler çıkış moduna alınırsa fiziki $1\text{k}\Omega$ seri direnç olmadığı takdirde pinler zarar görür. **Fiziki direnç takılması şarttır.**

---

## 6. UART FINAL WIRING PACKAGE

### 6.1 STM32 <-> ESP32 Veriyolu (115,200 Baud)

```
STM32 NUCLEO-G474RE                       ESP32-S3-N16R8
  PB10 (USART3_TX)  -----[ 1kΩ Seri ]----->  GPIO18 (STM_RXD)
  PB11 (USART3_RX)  <----[ 1kΩ Seri ]-----  GPIO8  (STM_TXD)
  GND               ======================  GND (Ortak Bus)
```
- **Baud Rate:** 115,200 bps
- **Lojik Seviye:** 3.3V CMOS
- **Seri Direnç:** Hat başına 1k$\Omega$ MANDATORY
- **GND Ortaklığı:** ZORUNLU

### 6.2 ESP32 <-> Nextion Veriyolu (9,600 Baud)

```
ESP32-S3-N16R8                            NEXTION NX4832T035
  GPIO17 (TXD2)     -----[ 1kΩ Seri ]----->  RX
  GPIO16 (RXD2)     <----[ 1kΩ Seri ]-----  TX
  GND               ======================  GND (Ortak Bus)
```
- **Baud Rate:** 9,600 bps
- **Lojik Seviye:** 3.3V TTL
- **Seri Direnç:** Hat başına 1k$\Omega$ OPTIONAL (Önerilir)
- **GND Ortaklığı:** ZORUNLU

---

## 7. ZERO CROSS TEST WIRING (3.3V TTL SIMULATION)

> [!CAUTION]
> **AŞIRI YÜKSEK GERİLİM VE GÜVENLİK UYARISI:**  
> **220V AC ŞEBEKE GERİLİMİ BU AŞAMADA KESİNLİKLE BAĞLANMAYACAKTIR.**  
> Masaüstü testinde STM32 PC7 pini sadece ESP32 GPIO4'ten gelen 3.3V DC TTL simülasyon sinyalini alacaktır.

```
ESP32-S3-N16R8                            STM32 NUCLEO-G474RE
  GPIO4 (ZC_SIM)    -----[ 1kΩ Seri ]----->  PC7 (CN5-2 = CN10-19)
  GND               ======================  GND (Ortak Bus)
```

- **Sinyal Tipi:** 100 Hz Kare Dalga (%50 Duty Cycle, 5ms HIGH / 5ms LOW), 3.3V TTL.
- **Elektriksel Uygunluk:** STM32 PC7 pini $3.3\text{V}$ lojik seviyeyi tam olarak algılar ($V_{IH,\text{min}}=2.0\text{V}$). 100 Hz frekansı 50Hz AC şebekenin çift yönlü doğrultulmuş sıfır geçiş palsına karşılık gelir. Sistem elektriksel ve yazılımsal olarak tam uyumludur.

---

## 8. LOOPBACK TEST WIRING (BENCH TEST ONLY)

| # | SOURCE PIN | SERIES RESISTOR | DESTINATION PIN | LOOPBACK TEST AMACI | KALICILIK DURUMU |
| :-: | :--- | :-: | :--- | :--- | :--- |
| **1** | STM32 PB15 (Heater Out) | **1k$\Omega$** | STM32 PA4 (Feedback In) | Röle tetikleme sinyalinin doğruluk test | **BENCH TEST ONLY** |
| **2** | STM32 PC6 (Triac Out)   | **1k$\Omega$** | STM32 PA6 (Feedback In) | Triac tetikleme pals zamanlama testi | **BENCH TEST ONLY** |
| **3** | STM32 PB12 (X9C CS Out) | **1k$\Omega$** | STM32 PB4 (Loopback In) | X9C CS çıkış durumunun öz-test denetimi | **BENCH TEST ONLY** |
| **4** | STM32 PB13 (X9C U/D Out)| **1k$\Omega$** | STM32 PB5 (Loopback In) | X9C U/D çıkış durumunun öz-test denetimi | **BENCH TEST ONLY** |
| **5** | STM32 PB14 (X9C INC Out)| **1k$\Omega$** | STM32 PB6 (Loopback In) | X9C INC çıkış durumunun öz-test denetimi | **BENCH TEST ONLY** |

> [!IMPORTANT]
> **KALICILIK UYARISI:**  
> Yukarıdaki 5 adet loopback bağlantısı sadece **Masaüstü/HIL Öz-Test (Self-Test)** modunda takılacaktır. Gerçek saha montajında PA4/PA6 pinleri akım trafosu/PT100 sensör devrelerine bağlanacağı için bu 5 adet jumper kablosu **BENCH TEST ONLY** olarak işaretlenmiş olup saha montajından önce sökülecektir.

---

## 9. FINAL MASTER PHYSICAL WIRING TABLE

Bütün fiziksel breadboard kablolaması için tek yetkili kılavuz tablosu:

| # | SOURCE DEVICE | SOURCE PIN | SIGNAL NAME | WIRE COLOR | RESISTOR | DEST PIN | DEST DEVICE | VOLTAGE | PURPOSE | PERMANENT / BENCH ONLY | WARNING |
| :-: | :--- | :--- | :--- | :--- | :-: | :--- | :--- | :--- | :--- | :--- | :--- |
| **1** | ESP32-S3 | GPIO8 | STM_TXD | Sarı | **1k$\Omega$** | PB11 | NUCLEO | 3.3V | UART Telemetri (ESP$\to$STM) | PERMANENT | 1k$\Omega$ Seri Şart |
| **2** | NUCLEO | PB10 | STM_RXD | Mavi | **1k$\Omega$** | GPIO18 | ESP32-S3 | 3.3V | UART Telemetri (STM$\to$ESP) | PERMANENT | 1k$\Omega$ Seri Şart |
| **3** | ESP32-S3 | GPIO17 | TXD2 | Yeşil | **1k$\Omega$** | RX | NEXTION | 3.3V | HMI Ekran TX (ESP$\to$Nextion) | PERMANENT | 1k$\Omega$ Seri Önerilir |
| **4** | NEXTION | TX | RXD2 | Beyaz | **1k$\Omega$** | GPIO16 | ESP32-S3 | 3.3V | HMI Ekran RX (Nextion$\to$ESP) | PERMANENT | 1k$\Omega$ Seri Önerilir |
| **5** | ESP32-S3 | GPIO4 | ZC_SIM | Turuncu| **1k$\Omega$** | PC7 (CN5-2)| NUCLEO | 3.3V | Zero Cross 100Hz Simülasyonu | BENCH ONLY | 220V AC BAGLAMA! |
| **6** | NUCLEO | PB14 | X9C_INC | Mor | **0$\Omega$ / 1k$\Omega$**| Pin 1 | X9C103S | 3.3V | Pot Increment Kontrol | PERMANENT | 3.3V Lojik |
| **7** | NUCLEO | PB13 | X9C_UD | Gri | **0$\Omega$ / 1k$\Omega$**| Pin 2 | X9C103S | 3.3V | Pot Up/Down Yön Kontrol | PERMANENT | 3.3V Lojik |
| **8** | NUCLEO | 3.3V Rail | VH | Kırmızı | Yok (0$\Omega$) | Pin 3 | X9C103S | 3.3V | Pot Üst Uç Referansı | PERMANENT | KESINLIKLE 5V BAGLAMA!|
| **9** | GND Bus | GND | VSS | Siyah | Yok (0$\Omega$) | Pin 4 | X9C103S | 0V | Pot Ground Beslemesi | PERMANENT | Ortak GND |
| **10**| X9C103S | Pin 5 | X9C_VW | Kahve | **1k$\Omega$** | PA0 | NUCLEO | 0-3.3V | Pot Silecek ADC Ölçümü | PERMANENT | 1k$\Omega$ Seri Şart |
| **11**| GND Bus | GND | VL | Siyah | Yok (0$\Omega$) | Pin 6 | X9C103S | 0V | Pot Alt Uç Referansı | PERMANENT | Ortak GND |
| **12**| NUCLEO | PB12 | X9C_CS | Pembe | **0$\Omega$ / 1k$\Omega$**| Pin 7 | X9C103S | 3.3V | Pot Chip Select Kontrol | PERMANENT | 3.3V Lojik |
| **13**| NUCLEO | 5V Rail | VCC | Kırmızı | Yok (0$\Omega$) | Pin 8 | X9C103S | 5.0V | Pot Entegre Beslemesi | PERMANENT | Nucleo 5V Hattı |
| **14**| NUCLEO | PB15 | LOOP_HTR | Sarı | **1k$\Omega$** | PA4 | NUCLEO | 3.3V | Heater Relay Loopback Test | BENCH ONLY | 1k$\Omega$ Seri Şart |
| **15**| NUCLEO | PC6 | LOOP_TRC | Mavi | **1k$\Omega$** | PA6 | NUCLEO | 3.3V | Triac Gate Loopback Test | BENCH ONLY | 1k$\Omega$ Seri Şart |
| **16**| NUCLEO | PB12 | LOOP_CS | Pembe | **1k$\Omega$** | PB4 | NUCLEO | 3.3V | X9C CS Loopback Test | BENCH ONLY | 1k$\Omega$ Seri Şart |
| **17**| NUCLEO | PB13 | LOOP_UD | Gri | **1k$\Omega$** | PB5 | NUCLEO | 3.3V | X9C U/D Loopback Test | BENCH ONLY | 1k$\Omega$ Seri Şart |
| **18**| NUCLEO | PB14 | LOOP_INC | Mor | **1k$\Omega$** | PB6 | NUCLEO | 3.3V | X9C INC Loopback Test | BENCH ONLY | 1k$\Omega$ Seri Şart |

---

## 10. POWER WIRING TABLE

| DEVICE | POWER PIN | POWER SOURCE | VOLTAGE | GND CONNECTION | MUST BE COMMON? | DO NOT CONNECT TO |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **NUCLEO-G474RE** | ST-LINK USB | PC USB Port #1 | 5.0V DC | CN7 Pin 8 / CN10 Pin 20 | **YES (ZORUNLU)** | ESP32 5V / Nextion 5V |
| **ESP32-S3-N16R8** | USB-C Port | PC USB Port #2 | 5.0V DC | GND Pin | **YES (ZORUNLU)** | Nucleo 5V / Nextion 5V |
| **NEXTION NX4832T035**| USB / 5V Red Wire| 5V 500mA USB Adaptör| 5.0V DC | Black Wire (GND) | **YES (ZORUNLU)** | Nucleo 5V / ESP32 5V |
| **X9C103S (Pin 8)** | Pin 8 (VCC) | NUCLEO 5V Rail | 5.0V DC | Pin 4 (VSS) | **YES (ZORUNLU)** | External 12V / 24V |
| **X9C103S (Pin 3)** | Pin 3 (VH) | NUCLEO 3.3V Rail | 3.3V DC | Pin 6 (VL) -> GND | **YES (ZORUNLU)** | 5V Rail (Aşırı Voltaj) |

---

## 11. ASCII SYSTEM WIRING DIAGRAM

```
========================================================================================================
                                     EAGLEULTRASONİK SYSTEM WIRING DIAGRAM
========================================================================================================

                                            +-------------------+
                                            |      HOST PC      |
                                            +--+-----+-----+----+
                                               |     |     |
                                  USB Cable #1 |     |     | USB Cable #3
                                +--------------+     |     +-------------------------+
                                |      USB Cable #2  |                               |
                                v                    v                               v
                       +-----------------+   +---------------+               +---------------+
                       | NUCLEO-G474RE   |   | ESP32-S3-N16R |               | NEXTION HMI   |
                       +-----------------+   +---------------+               +---------------+
                       | 5V  3.3V    GND |   | 5V   3.3V GND |               | 5V   TX   RX  |
                       +-+----+-------+--+   +--+----+----+--+               +-+----+----+---+
                         |    |       |         |    |    |                    |    |    |
                         |    |       |         |    |    |                    |    |    |
 [ISOLATED 5V RAILS]     |    |       |         |    |    |                    |    |    |
 Nucleo 5V  -------------+    |       |         |    |    |                    |    |    |
 ESP32  5V  ------------------|-------|---------+    |    |                    |    |    |
 Nextion 5V ------------------|-------|--------------|----+--------------------+    |    |
                              |       |              |    |                             |    |
 [ORTAK LOGIC GND BUS]        |       |              |    |                             |    |
 COMMON GND ==================|=======+==============|====+=============================+====+=== (GND BUS)
                              |       |              |                                  |    |
                              |       |              |     UART2 (9600 Baud)            |    |
                              |       |              |   GPIO17 ---[1kΩ]----------------+--->RX  |
                              |       |              |   GPIO16 <--[1kΩ]---------------------+---TX  |
                              |       |              |                                       |
                              |       |              |     UART3 (115200 Baud)               |
                              |       |              +-- GPIO8  ---[1kΩ]---> PB11 (RX)       |
                              |       |              +-- GPIO18 <--[1kΩ]<--- PB10 (TX)       |
                              |       |              |                                       |
                              |       |              |     ZERO CROSS SIMULATION (100Hz)     |
                              |       |              +-- GPIO4  ---[1kΩ]---> PC7 (CN5-2)     |
                              |       |                                                      |
                              v       |                                                      |
                      +---------------+---+                                                  |
                      | X9C103S POT       |                                                  |
                      +-------------------+                                                  |
                      | Pin 8 (VCC) <-----+ (Nucleo 5V)                                      |
                      | Pin 3 (VH)  <-----+ (Nucleo 3.3V)                                    |
                      | Pin 4 (VSS) <-----+ (Common GND Bus)                                 |
                      | Pin 6 (VL)  <-----+ (Common GND Bus)                                 |
                      | Pin 1 (INC) <--- PB14                                                |
                      | Pin 2 (U/D) <--- PB13                                                |
                      | Pin 7 (CS)  <--- PB12                                                |
                      | Pin 5 (VW)  ----[1kΩ]---> PA0 (ADC)                                  |
                      +-------------------+                                                  |
                                                                                             |
 [BENCH TEST ONLY LOOPBACKS]                                                                 |
   PB15 (Heater Out) ---[1kΩ]---> PA4 (Feedback In)                                          |
   PC6  (Triac Out)  ---[1kΩ]---> PA6 (Feedback In)                                          |
   PB12 (CS Out)     ---[1kΩ]---> PB4 (Loopback In)                                          |
   PB13 (U/D Out)    ---[1kΩ]---> PB5 (Loopback In)                                          |
   PB14 (INC Out)    ---[1kΩ]---> PB6 (Loopback In)                                          |
========================================================================================================
```

---

## 12. RESISTOR SHOPPING LIST

Fiziksel kurulum öncesinde temin edilmesi gereken kesin direnç listesi:

### 1 k$\Omega$ 1/4W %1 Metal / Karbon Film Direnç:
- **Zorunlu Miktar (Mandatory Quantity):** **9 Adet**
- **Önerilen Miktar (Recommended Quantity):** **11 Adet**

**Kullanım Adet Detayı:**
1. 1 Adet $\to$ ESP32 GPIO8 $\to$ STM32 PB11 (UART RX)
2. 1 Adet $\to$ STM32 PB10 $\to$ ESP32 GPIO18 (UART RX)
3. 1 Adet $\to$ ESP32 GPIO4 $\to$ STM32 PC7 (Zero Cross Sim)
4. 1 Adet $\to$ X9C Pin 5 (VW) $\to$ STM32 PA0 (ADC)
5. 1 Adet $\to$ STM32 PB15 $\to$ STM32 PA4 (Heater Loopback)
6. 1 Adet $\to$ STM32 PC6 $\to$ STM32 PA6 (Triac Loopback)
7. 1 Adet $\to$ STM32 PB12 $\to$ STM32 PB4 (CS Loopback)
8. 1 Adet $\to$ STM32 PB13 $\to$ STM32 PB5 (U/D Loopback)
9. 1 Adet $\to$ STM32 PB14 $\to$ STM32 PB6 (INC Loopback)
10. 1 Adet (Opsiyonel) $\to$ ESP32 GPIO17 $\to$ Nextion RX
11. 1 Adet (Opsiyonel) $\to$ Nextion TX $\to$ ESP32 GPIO16

---

## 13. "DO NOT CONNECT" SAFETY TABLE

| # | TEHLİKELİ / HATALI BAĞLANTI | OLASI HASAR VEYA SONUÇ | EYLEM |
| :-: | :--- | :--- | :--- |
| **1** | **NUCLEO 5V $\longleftrightarrow$ ESP32 5V** | Ters akım (backfeeding), USB port regülatör yanması | **BAĞLAMA!** |
| **2** | **NUCLEO 5V $\longleftrightarrow$ Nextion 5V** | Farklı regülatör çakışması, PC USB port koruması açılması | **BAĞLAMA!** |
| **3** | **ESP32 5V $\longleftrightarrow$ Nextion 5V** | Güç hatları paraziti ve regülatör ısınması | **BAĞLAMA!** |
| **4** | **220V AC Şebeke $\longleftrightarrow$ STM32 PC7** | Mikrokontrolcü patlaması, yangın ve hayati tehlike | **KESİNLİKLE BAĞLAMA!** |
| **5** | **X9C Pin 3 (VH) $\longleftrightarrow$ 5V Rail** | PA0 ADC pinine >3.3V gitmesi ve ADC katının yanması | **KESİNLİKLE BAĞLAMA!** |
| **6** | **PB15 $\to$ PA4 (Dirençsiz 0$\Omega$ Doğrudan Kablo)** | Pin çakışmasında $I > 40\text{mA}$, MCU bacaklarının yanması | **BAĞLAMA!** |
| **7** | **PC6 $\to$ PA6 (Dirençsiz 0$\Omega$ Doğrudan Kablo)** | Pin çakışmasında $I > 40\text{mA}$, MCU bacaklarının yanması | **BAĞLAMA!** |
| **8** | **ESP32 GPIO4 $\to$ PC7 (Dirençsiz 0$\Omega$ Kablo)** | Çıkış çakışmasında GPIO ve EXTI bacaklarının yanması | **BAĞLAMA!** |

---

## 14. STEP-BY-STEP SAFE POWER-UP PROCEDURE

Fiziksel montaj tamamlandıktan sonra sistemi çalıştırmadan önce uygulanacak **8 Adımlı Güvenlik Prosedürü**:

### STEP 1: Multimeter Continuity Test (Enerjisiz Kısa Devre Kontrolü)
- **Ölçü Aleti Modu:** Diyot / Buzzer (Continuity) Modu.
- **Prob Noktaları:** Ortak GND Bus ile Nucleo 5V, 3.3V, ESP32 5V ve Nextion 5V hatları arası.
- **Beklenen Değer:** Ötme sesi duyulmamalıdır (Direnç $> 100\Omega$ olmalıdır).
- **DUR (STOP) Kriteri:** Eğer GND ile herhangi bir 5V/3.3V hattı arası öterse ($0\Omega$ kısa devre), ENERJİ VERME!

### STEP 2: GND Verification (Ortak Referans Kontrolü)
- **Ölçü Aleti Modu:** Direnç ($200\Omega$) Modu. Enerjisiz.
- **Prob Noktaları:** Nucleo GND $\leftrightarrow$ ESP32 GND, Nucleo GND $\leftrightarrow$ Nextion GND, Nucleo GND $\leftrightarrow$ X9C VSS.
- **Beklenen Değer:** $R < 0.5\Omega$ (Tam temas).
- **DUR (STOP) Kriteri:** Direnç $> 1\Omega$ ise GND kablosunu yenile.

### STEP 3: 5V Rail Verification (Bağımsız 5V Ölçümü)
- **Ölçü Aleti Modu:** DC Voltaj (20V) Modu. USB kabloları takılır.
- **Prob Noktaları:** GND'ye göre Nucleo 5V, ESP32 5V ve Nextion 5V hatları ayrı ayrı ölçülür.
- **Beklenen Değer:** Her biri $4.75\text{V} \dots 5.25\text{V}$ arasında olmalıdır.
- **DUR (STOP) Kriteri:** Herhangi bir hat $< 4.5\text{V}$ veya $> 5.5\text{V}$ ise USB kaynağını değiştir.

### STEP 4: 3.3V Rail Verification (MCU Regülatör Ölçümü)
- **Ölçü Aleti Modu:** DC Voltaj (20V) Modu.
- **Prob Noktaları:** Nucleo GND'ye göre Nucleo 3.3V hattı.
- **Beklenen Değer:** $3.25\text{V} \dots 3.35\text{V}$.
- **DUR (STOP) Kriteri:** Voltaj $3.3\text{V}$ değilse MCU zarar görmüş olabilir, gücü kes.

### STEP 5: X9C Voltage Verification (Potansiyometre Besleme Ölçümü)
- **Ölçü Aleti Modu:** DC Voltaj (20V) Modu.
- **Prob Noktaları:** X9C Pin 8 (VCC) $\to 5.0\text{V}$, X9C Pin 3 (VH) $\to 3.3\text{V}$, X9C Pin 6 (VL) $\to 0.0\text{V}$.
- **Beklenen Değer:** VH tam olarak $3.3\text{V}$ olmalıdır.
- **DUR (STOP) Kriteri:** VH pininde 5V görülürse devreye enerji vermeyi DURDUR!

### STEP 6: UART Idle Voltage Verification (Seri Hat Boşta Voltajı)
- **Ölçü Aleti Modu:** DC Voltaj (20V) Modu.
- **Prob Noktaları:** STM32 PB10 (TX) ve ESP32 GPIO8 (TX) pinleri GND'ye göre.
- **Beklenen Değer:** Boşta (Idle) iken $3.3\text{V}$ HIGH seviyesi okunmalıdır.
- **DUR (STOP) Kriteri:** TX pini $0\text{V}$ ise pinde ters bağlama veya çakışma vardır.

### STEP 7: PC7 Zero-Cross Signal Verification (Simülasyon Sinyali)
- **Ölçü Aleti Modu:** Osiloskop veya Multimetre Frekans (Hz) / AC Voltaj Modu.
- **Prob Noktaları:** ESP32 GPIO4 çıkışı (1k$\Omega$ direnç öncesi ve sonrası).
- **Beklenen Değer:** $100\text{ Hz} \pm 2\text{Hz}$ kare dalga, $3.3\text{V}$ tepeden tepeye.
- **DUR (STOP) Kriteri:** Frekans okunamazsa ESP32 `esp_timer` simülasyon kodunu kontrol et.

### STEP 8: First Power-Up & Telemetry Verification (İlk Çalıştırma)
- Tüm aşamalar geçildikten sonra ESP32 Seri Port ekranından (115200 baud) STM32 telemetri paketlerinin `STAT,...` başarıyla aktığı doğrulanır.

---

## 15. FINAL DECISION

> [!IMPORTANT]
> **FİZİKSEL KABLOLAMA KARARI:**  
> **B) READY WITH CONDITIONS (ŞARTLI ONAY VERİLDİ)**

### Şartlı Onay Maddeleri (Conditional Checklist):
1. **[ŞART 1]:** 220V AC şebeke bağlantısı kesinlikle yapılmayacak; PC7 pini sadece ESP32 GPIO4 simülatörüne bağlanacaktır.
2. **[ŞART 2]:** NUCLEO 5V, ESP32 5V ve Nextion 5V hatları fiziksel olarak kesinlikle birleştirilmeyecektir.
3. **[ŞART 3]:** Nucleo GND, ESP32 GND, Nextion GND ve X9C VSS pinleri Ortak Signal GND Bus üzerinde birleştirilecektir.
4. **[ŞART 4]:** X9C VH pinine 3.3V verilecek (kesinlikle 5V verilmeyecektir).
5. **[ŞART 5]:** Tablo 4.1'de belirtilen 9 adet zorunlu fiziksel $1\text{k}\Omega$ seri direnç eksiksiz takılacaktır.
6. **[ŞART 6]:** Çalıştırmadan önce Bölüm 14'teki 8 adımlı multimetre doğrulama prosedürü eksiksiz uygulanacaktır.
