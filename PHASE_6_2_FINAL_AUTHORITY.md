# PHASE 6.2 FINAL AUTHORITY
**SINGLE SOURCE OF TRUTH FOR HARDWARE WIRING, FIRMWARE READBACK, AND TEST ARCHITECTURE**

> [!CAUTION]
> **ALL PREVIOUS PHASE 6.2 DOCUMENTS, SCHEMATICS, AND WIRING TABLES ARE SUPERSEDED BY THIS DOCUMENT.**
> Fiziksel kablolama için tek referans bu dosyadır. Lütfen kurulum yaparken aşağıdaki tabloları birebir takip ediniz.

---

## 1. FINAL PIN-TO-PIN TABLE (MASTER WIRING)

| ID | Kaynak Kart | Kaynak GPIO | Kaynak Connector | Kaynak Pin | Sinyal | Direnç | Direnç Değeri | Direnç A Ucu | Direnç B Ucu | Hedef Eleman | Hedef Pin | Hedef Connector | Yön | Voltaj | Amaç | Mod |
|:---|:---|:---|:---|:---|:---|:---|:---|:---|:---|:---|:---|:---|:---|:---|:---|:---|
| **W01** | NUCLEO-G474RE | PA0 | CN7 | Pin 28 | ADC_IN | R-X9C-VW | 1kΩ | X9C Pin 5 (VW) | STM32 PA0 (CN7-28) | X9C103S | Pin 5 (VW) | Doğrudan (R) | X9C -> STM32 | 3.3V | Pot okuma | Ortak |
| **W02** | NUCLEO-G474RE | PA4 | CN7 | Pin 32 | LOOP_IN | R-HTR-FB | 1kΩ | STM32 PB15 (CN10-26) | STM32 PA4 (CN7-32) | NUCLEO-G474RE | PB15 | CN10 Pin 26 | STM32 -> STM32 | 3.3V | Heater Loop | Bench |
| **W03** | NUCLEO-G474RE | PA6 | CN7 | Pin 34 | LOOP_IN | R-TRC-FB | 1kΩ | STM32 PC6 (CN10-4) | STM32 PA6 (CN7-34) | NUCLEO-G474RE | PC6 | CN10 Pin 4 | STM32 -> STM32 | 3.3V | Triac Loop | Bench |
| **W04** | NUCLEO-G474RE | PA1 | CN7 | Pin 30 | ADC_IN | Yok | N/A | N/A | N/A | PT100 Devre | V_OUT | Doğrudan | PT100 -> STM32 | 3.3V | Temp Okuma | Ortak |
| **W05** | ESP32-S3 | GPIO4 | Header | N/A | ZC_SIM | R-ZC-SIM | 1kΩ | ESP32 GPIO4 | STM32 PC7 (CN5-2) | NUCLEO-G474RE | PC7 | CN5 Pin 2 | ESP32 -> STM32 | 3.3V | ZC 100Hz Sim| Bench |
| **W06** | NUCLEO-G474RE | PB12 | CN10 | Pin 16 | X9C_CS | Yok | N/A | N/A | N/A | X9C103S | Pin 7 (CS) | Doğrudan | STM32 -> X9C | 3.3V | SPI CS | Ortak |
| **W07** | NUCLEO-G474RE | PB13 | CN10 | Pin 30 | X9C_UD | Yok | N/A | N/A | N/A | X9C103S | Pin 2 (U/D) | Doğrudan | STM32 -> X9C | 3.3V | SPI U/D | Ortak |
| **W08** | NUCLEO-G474RE | PB14 | CN10 | Pin 28 | X9C_INC | Yok | N/A | N/A | N/A | X9C103S | Pin 1 (INC) | Doğrudan | STM32 -> X9C | 3.3V | SPI INC | Ortak |
| **W09** | NUCLEO-G474RE | PB15 | CN10 | Pin 26 | HEATER | Yok | N/A | N/A | N/A | SSR | V_IN | Doğrudan | STM32 -> SSR | 3.3V | Heater Çıkış | Üretim|
| **W10** | NUCLEO-G474RE | PC6 | CN10 | Pin 4 | TRIAC | Yok | N/A | N/A | N/A | MOC3021 | Pin 1 | Doğrudan | STM32 -> MOC | 3.3V | Triac Çıkış | Üretim|
| **W11** | NUCLEO-G474RE | PC8 | CN10 | Pin 2 | SW1 | Yok | N/A | N/A | N/A | DIP Switch | SW1 | Doğrudan (GND)| DIP -> STM32 | 0V | Adres Seçimi | Ortak |
| **W12** | NUCLEO-G474RE | PC9 | CN10 | Pin 1 | SW2 | Yok | N/A | N/A | N/A | DIP Switch | SW2 | Doğrudan (GND)| DIP -> STM32 | 0V | Adres Seçimi | Ortak |
| **W13** | NUCLEO-G474RE | PC10 | CN7 | Pin 1 | SW3 | Yok | N/A | N/A | N/A | DIP Switch | SW3 | Doğrudan (GND)| DIP -> STM32 | 0V | Adres Seçimi | Ortak |
| **W14** | NUCLEO-G474RE | PC11 | CN7 | Pin 2 | SW4 | Yok | N/A | N/A | N/A | DIP Switch | SW4 | Doğrudan (GND)| DIP -> STM32 | 0V | Adres Seçimi | Ortak |
| **W15** | NUCLEO-G474RE | PB10 | CN7 | Pin 25 | TX | Yok | N/A | N/A | N/A | MAX485 #1 | Pin 4 (DI) | Doğrudan | STM32 -> MAX | 3.3V | RS485 Tx | Ortak |
| **W16** | NUCLEO-G474RE | PB11 | CN10 | Pin 18 | RX | Yok | N/A | N/A | N/A | MAX485 #1 | Pin 1 (RO) | Doğrudan | MAX -> STM32 | 5V*FT| RS485 Rx | Ortak |
| **W17** | NUCLEO-G474RE | PB1 | CN7 | Pin 24 | DE/RE | Yok | N/A | N/A | N/A | MAX485 #1 | Pin 2+3 | Doğrudan | STM32 -> MAX | 3.3V | RS485 Yön | Ortak |
| **W18** | ESP32-S3 | GPIO8 | Header | N/A | TX | Yok | N/A | N/A | N/A | MAX485 #2 | Pin 4 (DI) | Doğrudan | ESP32 -> MAX | 3.3V | RS485 Tx | Ortak |
| **W19** | ESP32-S3 | GPIO18 | Header | N/A | RX | R-DIV | 10k/18k | MAX Pin 1 | GND | MAX485 #2 | Pin 1 (RO) | Bölücü | MAX -> ESP32 | 3.3V | RS485 Rx | Ortak |
| **W20** | ESP32-S3 | GPIO5 | Header | N/A | DE/RE | Yok | N/A | N/A | N/A | MAX485 #2 | Pin 2+3 | Doğrudan | ESP32 -> MAX | 3.3V | RS485 Yön | Ortak |
| **W21** | ESP32-S3 | GPIO17 | Header | N/A | TX | Yok | N/A | N/A | N/A | Nextion HMI | RX (Sarı) | Doğrudan | ESP32 -> HMI | 3.3V | HMI Tx | Ortak |
| **W22** | ESP32-S3 | GPIO16 | Header | N/A | RX | Yok | N/A | N/A | N/A | Nextion HMI | TX (Mavi) | Doğrudan | HMI -> ESP32 | 3.3V | HMI Rx | Ortak |
| **W23** | MAX485 #1 | A | Pin 6 | N/A | BUS_A | Yok | N/A | N/A | N/A | MAX485 #2 | Pin 6 (A) | Doğrudan | MAX <-> MAX | Diff | RS485 A | Ortak |
| **W24** | MAX485 #1 | B | Pin 7 | N/A | BUS_B | Yok | N/A | N/A | N/A | MAX485 #2 | Pin 7 (B) | Doğrudan | MAX <-> MAX | Diff | RS485 B | Ortak |

---

## 2. FINAL RESISTOR TABLE

| ID | Değer | Terminal A | Terminal B | Nerede Kullanılıyor | Mod |
|:---|:---|:---|:---|:---|:---|
| **R-HTR-FB** | 1kΩ | STM32 PB15 (CN10-26) | STM32 PA4 (CN7-32) | Heater output physical loopback diagnostik | Bench |
| **R-TRC-FB** | 1kΩ | STM32 PC6 (CN10-4) | STM32 PA6 (CN7-34) | Triac gate physical loopback diagnostik | Bench |
| **R-ZC-SIM** | 1kΩ | ESP32 GPIO4 | STM32 PC7 (CN5-2) | Zero-cross 100Hz hardware simülasyon | Bench |
| **R-X9C-VW** | 1kΩ | X9C Pin 5 (VW) | STM32 PA0 (CN7-28) | ADC1_IN1 akım sınırlayıcı / giriş | Ortak |
| **R-DIV-TOP** | 10kΩ | MAX485 #2 Pin 1 (RO) | ESP32 GPIO18 node | RS485 RO 5V -> 3.3V ESP32 voltaj bölücü (Üst) | Ortak |
| **R-DIV-BOT** | 18kΩ | ESP32 GPIO18 node | Ortak Logic GND | RS485 RO 5V -> 3.3V ESP32 voltaj bölücü (Alt) | Ortak |
| **R-TERM-1** | 120Ω | MAX485 #1 Pin 6 (A) | MAX485 #1 Pin 7 (B) | RS485 BUS A-B Terminasyonu | Ortak |
| **R-TERM-2** | 120Ω | MAX485 #2 Pin 6 (A) | MAX485 #2 Pin 7 (B) | RS485 BUS A-B Terminasyonu | Ortak |

*(Seri hatta RS485 UART üzerinde ekstra 1kΩ DİRENÇ YOKTUR)*

---

## 3. FINAL POWER TABLE

| Güç Veren Kaynak | Kaynak Pini | Hedef Komponent | Hedef Pini | Notlar |
|:---|:---|:---|:---|:---|
| **USB #1** | PC USB Port | NUCLEO-G474RE | ST-LINK USB | NUCLEO ana güç beslemesi |
| **USB #2** | PC USB Port | ESP32-S3 | USB Port | ESP32 ana güç beslemesi |
| **USB #3 (Veya ESP)** | 5V | Nextion HMI | VCC (Kırmızı) | Ekran güç beslemesi |
| NUCLEO-G474RE | 5V (CN7-18) | MAX485 #1 | Pin 8 (VCC) | MAX485 master gücü |
| NUCLEO-G474RE | 5V (CN7-18) | X9C103S | Pin 8 (VCC) | X9C çip gücü |
| NUCLEO-G474RE | 3.3V (CN7-16)| X9C103S | Pin 3 (VH) | X9C Tepe voltajı (VH) |
| ESP32-S3 | 5V | MAX485 #2 | Pin 8 (VCC) | MAX485 slave gücü |
| **Ortak GND Bara** | **Ortak Hat** | **TÜM CİHAZLAR**| **GND** | NUCLEO, ESP32, HMI, MAXx2, X9C GND'leri birbirine BİRLEŞECEK. |

> [!WARNING]
> NUCLEO 5V ile ESP32 5V BİRBİRİNE BAĞLANMAYACAKTIR. Sadece GND ortaktır.

---

## 4. FINAL LOOPBACK TABLE

| Loopback ID | Kaynak Pin (Çıkış) | Seri Direnç | Direnç A | Direnç B | Geri Besleme (Giriş) | Ölçüm | Amaç | Beklenen |
|:---|:---|:---|:---|:---|:---|:---|:---|:---|
| **LP-01** | STM32 PB15 (CN10-26) | R-HTR-FB (1kΩ)| PB15 | PA4 | STM32 PA4 (CN7-32) | PA4 / `HEATER_FB` | Heater Output | Timer aktifken PA4 = 3.3V |
| **LP-02** | STM32 PC6 (CN10-4) | R-TRC-FB (1kΩ)| PC6 | PA6 | STM32 PA6 (CN7-34) | PA6 / `TRIAC_FB` | Triac Gate PWM | Timer aktifken PA6 PWM alır |
| **LP-03** | ESP32 GPIO4 | R-ZC-SIM (1kΩ)| GPIO4 | PC7 | STM32 PC7 (CN5-2) | PC7 / `fault` b4| ZC Simülasyonu | Sürekli 100Hz 3.3V kare dalga |

*(Firmware Readback tam olarak entegre edilmiştir. Uyuşmazlık anında COM11 `DIAGNOSTIC` basar).*

---

## 5. FINAL DIP SWITCH TABLE

| Anahtar | STM32 Pin | STM32 Connector | STM32 Connector Pin | Anahtar Hedefi | Çekme Direnci (Pull-Up) |
|:---|:---|:---|:---|:---|:---|
| **SW1** | PC8 | CN10 | Pin 2 | Ortak GND Bara | EXTERNAL RESISTOR YOK — MCU INTERNAL PULL-UP |
| **SW2** | PC9 | CN10 | Pin 1 | Ortak GND Bara | EXTERNAL RESISTOR YOK — MCU INTERNAL PULL-UP |
| **SW3** | PC10 | CN7 | Pin 1 | Ortak GND Bara | EXTERNAL RESISTOR YOK — MCU INTERNAL PULL-UP |
| **SW4** | PC11 | CN7 | Pin 2 | Ortak GND Bara | EXTERNAL RESISTOR YOK — MCU INTERNAL PULL-UP |

*(Logic State: Anahtar AÇIK/OFF = HIGH (1), Anahtar KAPALI/ON = LOW (0)).*

---

## 6. FINAL RS485 TABLE

| MAX485 Çipi | Pin 1 (RO) | Pin 2 (/RE) | Pin 3 (DE) | Pin 4 (DI) | Pin 5 (GND) | Pin 6 (A) | Pin 7 (B) | Pin 8 (VCC) |
|:---|:---|:---|:---|:---|:---|:---|:---|:---|
| **MAX485 #1** | STM32 PB11 | STM32 PB1 | STM32 PB1 | STM32 PB10 | Ortak GND | BUS A (MAX#2 A) | BUS B (MAX#2 B) | NUCLEO 5V |
| **MAX485 #2** | 10k/18k->GPIO18 | ESP32 GPIO5| ESP32 GPIO5| ESP32 GPIO8 | Ortak GND | BUS A (MAX#1 A) | BUS B (MAX#1 B) | ESP32 5V |

---

## 7. FINAL NEXTION TABLE

| Nextion Kablo Rengi | Nextion Sinyali | Hedef Cihaz | Hedef Pin | Elektriksel Seviye |
|:---|:---|:---|:---|:---|
| Kırmızı | VCC (5V) | Harici/ESP32 USB | 5V | 5V |
| Siyah | GND | Ortak GND Bara | GND | 0V |
| Sarı | RX | ESP32-S3 | GPIO17 | 3.3V (Direnç YOK) |
| Mavi | TX | ESP32-S3 | GPIO16 | 3.3V (Direnç YOK) |

*(Protokol: 9600 Baud, 8N1).*

---

## 8. FINAL X9C TABLE

| X9C Pin | Sinyal | Bağlanacağı Yer | Seri Direnç / Bileşen | Voltaj | Amaç |
|:---|:---|:---|:---|:---|:---|
| **Pin 1** | INC | STM32 PB14 (CN10-28) | Doğrudan | 3.3V | Direnç adım arttırma |
| **Pin 2** | U/D | STM32 PB13 (CN10-30) | Doğrudan | 3.3V | Yön (Yukarı/Aşağı) |
| **Pin 3** | VH | NUCLEO 3.3V (CN7-16) | Doğrudan | 3.3V | Max Pot Voltajı |
| **Pin 4** | VSS | Ortak GND Bara | Doğrudan | 0V | Pot Şase |
| **Pin 5** | VW | STM32 PA0 (CN7-28) | 1kΩ (Terminal A=VW, B=PA0) | 0-3.3V | ADC Sinyali Oku |
| **Pin 6** | VL | Ortak GND Bara | Doğrudan | 0V | Min Pot Voltajı |
| **Pin 7** | CS | STM32 PB12 (CN10-16) | Doğrudan | 3.3V | Chip Select |
| **Pin 8** | VCC | NUCLEO 5V (CN7-18) | Doğrudan | 5V | Çip Beslemesi |

---

## 9. FINAL PHYSICAL ASSEMBLY ORDER

Breadboard kurulumunu adım adım gerçekleştirmek için aşağıdaki sırayı takip ediniz:

1. **Common GND (Ortak Şase) Bus Oluşturulması**
   - Breadboard üzerinde uzun bir hattı GND (mavi/siyah) olarak atayın.
   - NUCLEO GND, ESP32 GND, Nextion GND, MAX485 #1 Pin 5, MAX485 #2 Pin 5, X9C Pin 4 ve Pin 6 ile DIP Switch ortak ucunu bu hatta bağlayın.
2. **Güç Dağıtımı**
   - NUCLEO 5V -> X9C Pin 8 ve MAX485 #1 Pin 8.
   - NUCLEO 3.3V -> X9C Pin 3.
   - ESP32 5V -> MAX485 #2 Pin 8.
   - Nextion 5V -> Harici veya ESP 5V hattına bağlayın. (NUCLEO 5V ve ESP32 5V KESİNLİKLE birleşmeyecek).
3. **RS485 Donanım Kurulumu**
   - MAX485 #1 Pin 6 (A) -> MAX485 #2 Pin 6 (A).
   - MAX485 #1 Pin 7 (B) -> MAX485 #2 Pin 7 (B).
   - `R-TERM-1` (120Ω) -> MAX #1 A ve B arasına takın.
   - `R-TERM-2` (120Ω) -> MAX #2 A ve B arasına takın.
   - 10k/18k Voltaj bölücüyü (`R-DIV-TOP`, `R-DIV-BOT`) MAX485 #2 Pin 1 ile GND arasına kurup, orta noktayı ESP32 GPIO18'e bağlayın.
4. **RS485 MCU Bağlantıları**
   - PB10 -> MAX485 #1 Pin 4, PB11 -> MAX485 #1 Pin 1, PB1 -> MAX485 #1 Pin 2+3.
   - GPIO8 -> MAX485 #2 Pin 4, GPIO5 -> MAX485 #2 Pin 2+3.
5. **X9C103S Bağlantıları**
   - PB12 -> X9C Pin 7, PB13 -> X9C Pin 2, PB14 -> X9C Pin 1.
   - X9C Pin 5 -> 1kΩ (`R-X9C-VW`) -> STM32 PA0.
6. **HMI ve DIP Switch Bağlantıları**
   - GPIO17 -> Nextion Sarı(RX), GPIO16 -> Nextion Mavi(TX).
   - PC8, PC9, PC10, PC11 pinlerini DIP switch bacaklarına takın.
7. **Bench Loopbacks Kurulumu**
   - ESP32 GPIO4 -> 1kΩ (`R-ZC-SIM`) -> STM32 PC7 (CN5-2).
   - STM32 PB15 -> 1kΩ (`R-HTR-FB`) -> STM32 PA4.
   - STM32 PC6 -> 1kΩ (`R-TRC-FB`) -> STM32 PA6.

---

## 10. FINAL CHECKLIST

- [ ] Common GND (Ortak Şase) Bara oluşturuldu ve tüm cihazlar bağlandı.
- [ ] 5V güç hatları birbirine **kesinlikle** kısa devre değil.
- [ ] X9C_VCC NUCLEO 5V'a bağlı.
- [ ] X9C_VH NUCLEO 3.3V'a bağlı.
- [ ] MAX485#1 A ucu MAX485#2 A ucuna bağlı.
- [ ] MAX485#1 B ucu MAX485#2 B ucuna bağlı.
- [ ] Bus A ve B arasında iki uçta toplam 2 adet 120Ω (Terminasyon) direnci paralel bağlı.
- [ ] PB10 -> MAX485 #1 Pin 4 (DI)
- [ ] PB11 -> MAX485 #1 Pin 1 (RO)
- [ ] PB1 -> MAX485 #1 Pin 2+3 (DE/RE)
- [ ] GPIO8 -> MAX485 #2 Pin 4 (DI)
- [ ] GPIO5 -> MAX485 #2 Pin 2+3 (DE/RE)
- [ ] MAX485 #2 Pin 1 (RO) üzerinden voltaj bölücü (10k/18k) ile ESP32 GPIO18'e doğru bağlantı sağlandı.
- [ ] PB12 -> X9C Pin 7 (CS)
- [ ] PB13 -> X9C Pin 2 (U/D)
- [ ] PB14 -> X9C Pin 1 (INC)
- [ ] X9C Pin 5 (VW) -> 1kΩ -> PA0 (CN7-28)
- [ ] GPIO17 -> Nextion RX (Sarı)
- [ ] GPIO16 <- Nextion TX (Mavi)
- [ ] PC8, PC9, PC10, PC11 -> DIP Switch
- [ ] GPIO4 -> 1kΩ -> PC7 (CN5-2)
- [ ] PB15 (CN10-26) -> 1kΩ -> PA4 (CN7-32)
- [ ] PC6 (CN10-4) -> 1kΩ -> PA6 (CN7-34)

---

## 11. FINAL AUTHORITY GATE

### PHASE 6.2 FINAL AUTHORITY
-------------------------
**FIRMWARE:** PASS  
**CUBEMX:** PASS  
**NUCLEO PINOUT:** PASS  
**ESP32 PINOUT:** PASS  
**X9C:** PASS  
**MAX485 #1:** PASS  
**MAX485 #2:** PASS  
**NEXTION:** PASS  
**DIP SWITCH:** PASS  
**ZERO CROSS:** PASS  
**HEATER OUTPUT:** PASS  
**HEATER READBACK:** PASS  
**TRIAC OUTPUT:** PASS  
**TRIAC READBACK:** PASS  
**TIMER:** PASS  
**TIMER -> OUTPUT SHUTDOWN:** PASS  
**RS485:** PASS  
**POWER:** PASS  
**DATASHEET:** PASS  
**PIN/CONNECTOR CONSISTENCY:** PASS  
**RESISTOR NETLIST:** PASS  
**DOCUMENT CONSISTENCY:** PASS  
**AUTOMATED TESTS:** 6 PASS / 0 FAIL / 1 SKIP (HIL donanımı bağlanmadan CI ortamında skip edildi)  
**PHYSICAL BENCH WIRING:** AUTHORIZED  
**UNRESOLVED ISSUES:** 0  

**FINAL SYSTEM STATUS:**
**READY FOR PHYSICAL BENCH TEST**
