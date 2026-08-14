# PHASE 6.2 FINAL PHYSICAL ASSEMBLY CHECKLIST

> [!CAUTION]
> Bu doküman **NİHAİ FİZİKSEL KURULUM VE MONTAJ SIRASI** listesidir. Lütfen kurulum yaparken aşağıdaki sıralamayı birebir takip edin. Kablolama tamamlandıktan sonra enerjiyi vermeden önce aşağıdaki kontrol listesini işaretleyerek teyit edin.

## A. FİZİKSEL MONTAJ SIRASI

1.  **Common GND (Ortak Şase) Bus Oluşturulması**
    - [ ] Breadboard üzerinde bir hattı baştan sona GND (mavi) olarak belirleyin.
    - [ ] NUCLEO-G474RE GND pinini bu hatta bağlayın.
    - [ ] ESP32-S3 GND pinini bu hatta bağlayın.
    - [ ] Nextion HMI GND pinini bu hatta bağlayın.
    - [ ] MAX485 #1 ve MAX485 #2 Pin 5 (GND) bacaklarını bu hatta bağlayın.
    - [ ] X9C103S Pin 4 (VSS) ve Pin 6 (VL) bacaklarını bu hatta bağlayın.
    - [ ] DIP Switch'in ortak terminalini (toprak hattını) bu hatta bağlayın.

2.  **Güç Dağıtımı**
    - [ ] NUCLEO 5V (CN7-18) pinini X9C Pin 8'e ve MAX485 #1 Pin 8'e bağlayın.
    - [ ] NUCLEO 3.3V (CN7-16) pinini X9C Pin 3'e bağlayın.
    - [ ] ESP32 5V (veya VBUS) pinini MAX485 #2 Pin 8'e bağlayın.
    - [ ] Nextion 5V beslemesini bağlayın (tercihen harici güçlü 5V veya ESP32 VBUS üzerinden).
    - [ ] **KONTROL:** NUCLEO 5V hattı ile ESP32 5V hattının BİRBİRİNE KESİNLİKLE KISA DEVRE OLMADIĞINDAN emin olun.

3.  **RS485 Veriyolu Bağlantısı**
    - [ ] MAX485 #1 Pin 6 (A) ile MAX485 #2 Pin 6 (A) uçlarını bağlayın.
    - [ ] MAX485 #1 Pin 7 (B) ile MAX485 #2 Pin 7 (B) uçlarını bağlayın.
    - [ ] 120Ω `R-TERM-1` direncini MAX485 #1'de A ve B pinleri *arasına* takın.
    - [ ] 120Ω `R-TERM-2` direncini MAX485 #2'de A ve B pinleri *arasına* takın.

4.  **MAX485 MCU Sinyalleri**
    - [ ] STM32 PB10 (CN7-25) -> MAX485 #1 Pin 4 (DI) bağlayın.
    - [ ] STM32 PB11 (CN10-18) -> MAX485 #1 Pin 1 (RO) bağlayın.
    - [ ] STM32 PB1 (CN7-24) -> MAX485 #1 Pin 2 (/RE) ve Pin 3 (DE) bağlayın (iki pin birbirine köprülenebilir).
    - [ ] ESP32 GPIO8 -> MAX485 #2 Pin 4 (DI) bağlayın.
    - [ ] ESP32 GPIO5 -> MAX485 #2 Pin 2 (/RE) ve Pin 3 (DE) bağlayın.
    - [ ] MAX485 #2 Pin 1 (RO) -> 10kΩ (`R-DIV-TOP`) -> **ESP32 GPIO18 node** -> 18kΩ (`R-DIV-BOT`) -> **GND** devresini kurun. (RO 5V'tan GPIO18'e korumalı 3.3V verir).

5.  **X9C103S SPI ve ADC Bağlantıları**
    - [ ] STM32 PB12 (CN10-16) -> X9C Pin 7 (CS) bağlayın.
    - [ ] STM32 PB13 (CN10-30) -> X9C Pin 2 (U/D) bağlayın.
    - [ ] STM32 PB14 (CN10-28) -> X9C Pin 1 (INC) bağlayın.
    - [ ] X9C Pin 5 (VW) -> 1kΩ (`R-X9C-VW`) -> STM32 PA0 (CN7-28) bağlayın.

6.  **Nextion HMI Bağlantısı**
    - [ ] ESP32 GPIO17 -> Nextion RX (Sarı) bağlayın.
    - [ ] ESP32 GPIO16 -> Nextion TX (Mavi) bağlayın.

7.  **DIP Switch (Adres Seçimi)**
    - [ ] DIP SW1 ucunu STM32 PC8 (CN10-2) pinine bağlayın.
    - [ ] DIP SW2 ucunu STM32 PC9 (CN10-1) pinine bağlayın.
    - [ ] DIP SW3 ucunu STM32 PC10 (CN7-1) pinine bağlayın.
    - [ ] DIP SW4 ucunu STM32 PC11 (CN7-2) pinine bağlayın.

8.  **Bench Loopbacks (Fiziksel Test Döngüleri)**
    - [ ] ESP32 GPIO4 -> 1kΩ (`R-ZC-SIM`) -> STM32 PC7 (CN5-2) bağlayın.
    - [ ] STM32 PB15 (CN10-26) -> 1kΩ (`R-HTR-FB`) -> STM32 PA4 (CN7-32) bağlayın.
    - [ ] STM32 PC6 (CN10-4) -> 1kΩ (`R-TRC-FB`) -> STM32 PA6 (CN7-34) bağlayın.

---

## B. SON KABLO KONTROL LİSTESİ

Enerjiyi vermeden önce fiziksel kurulumunuzla aşağıdaki kutucukları birebir çapraz kontrol edin. Tüm bağlantıların hatasız olması zorunludur.

- [ ] USB bağlantıları ve ortak GND (NUCLEO, ESP32, HMI, MAX485, X9C) tam ve güvenli.
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
- [ ] GPIO17 -> Nextion RX
- [ ] GPIO16 <- Nextion TX
- [ ] PC8, PC9, PC10, PC11 -> DIP Switch (Diğer uçları GND)
- [ ] GPIO4 -> 1kΩ -> PC7 (CN5-2)
- [ ] PB15 (CN10-26) -> 1kΩ -> PA4 (CN7-32)
- [ ] PC6 (CN10-4) -> 1kΩ -> PA6 (CN7-34)

> [!IMPORTANT]
> Kontrol listesini tamamladıysanız, donanım mimariniz `PHASE_6_2_FINAL_AUTHORITY.md` kurallarına %100 uymaktadır. Güvenle USB güçlerini verip `test_hil_uart.py` suite'ini çalıştırabilirsiniz.
