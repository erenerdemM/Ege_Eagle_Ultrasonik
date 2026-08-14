# PHASE 6.2 FINAL PIN-TO-PIN WIRING

> [!CAUTION]
> Bu doküman **TEK VE NİHAİ** fiziksel kablolama tablosudur. Başka hiçbir dokümana veya varsayıma başvurmayınız.
> Tüm pin numaraları ST UM2505 ve gerçek datasheet referansları ile doğrulanmıştır.

| ID | Kaynak Kart | Kaynak GPIO | Kaynak Connector | Kaynak Pin | Sinyal | Direnç | Direnç Değeri | Direnç A Ucu | Direnç B Ucu | Hedef Eleman | Hedef Pin | Hedef Connector | Yön | Voltaj | Amaç | Mod |
|:---|:---|:---|:---|:---|:---|:---|:---|:---|:---|:---|:---|:---|:---|:---|:---|:---|
| **W01** | NUCLEO-G474RE | PA0 | CN7 | Pin 28 | ADC_IN | R-X9C-VW | 1kΩ | X9C Pin 5 (VW) | STM32 PA0 (CN7-28) | X9C103S | Pin 5 (VW) | Doğrudan (R üzerinden) | X9C -> STM32 | 0-3.3V | Dijital potansiyometre okuma | Ortak |
| **W02** | NUCLEO-G474RE | PA4 | CN7 | Pin 32 | LOOP_IN | R-HTR-FB | 1kΩ | STM32 PB15 (CN10-26) | STM32 PA4 (CN7-32) | NUCLEO-G474RE | PB15 | CN10 Pin 26 | STM32 -> STM32 | 3.3V | Isıtıcı geri besleme | Bench |
| **W03** | NUCLEO-G474RE | PA6 | CN7 | Pin 34 | LOOP_IN | R-TRC-FB | 1kΩ | STM32 PC6 (CN10-4) | STM32 PA6 (CN7-34) | NUCLEO-G474RE | PC6 | CN10 Pin 4 | STM32 -> STM32 | 3.3V | Triyak geri besleme | Bench |
| **W04** | NUCLEO-G474RE | PA1 | CN7 | Pin 30 | ADC_IN | Yok | N/A | N/A | N/A | PT100 Devresi | V_OUT | Doğrudan | PT100 -> STM32 | 0-3.3V | Sıcaklık okuma | Ortak |
| **W05** | ESP32-S3 | GPIO4 | Header | N/A | ZC_SIM | R-ZC-SIM | 1kΩ | ESP32 GPIO4 | STM32 PC7 (CN5-2) | NUCLEO-G474RE | PC7 | CN5 Pin 2 | ESP32 -> STM32 | 3.3V | Zero-cross 100Hz sim | Bench |
| **W06** | NUCLEO-G474RE | PB12 | CN10 | Pin 16 | X9C_CS | Yok | N/A | N/A | N/A | X9C103S | Pin 7 (CS) | Doğrudan | STM32 -> X9C | 3.3V | X9C SPI Chip Select | Ortak |
| **W07** | NUCLEO-G474RE | PB13 | CN10 | Pin 30 | X9C_UD | Yok | N/A | N/A | N/A | X9C103S | Pin 2 (U/D) | Doğrudan | STM32 -> X9C | 3.3V | X9C SPI Up/Down | Ortak |
| **W08** | NUCLEO-G474RE | PB14 | CN10 | Pin 28 | X9C_INC | Yok | N/A | N/A | N/A | X9C103S | Pin 1 (INC) | Doğrudan | STM32 -> X9C | 3.3V | X9C SPI Increment | Ortak |
| **W09** | NUCLEO-G474RE | PB15 | CN10 | Pin 26 | HEATER | Yok | N/A | N/A | N/A | SSR | V_IN | Doğrudan (Bench: Yok) | STM32 -> SSR | 3.3V | Isıtıcı röle kontrolü | Üretim |
| **W10** | NUCLEO-G474RE | PC6 | CN10 | Pin 4 | TRIAC | Yok | N/A | N/A | N/A | MOC3021 | Pin 1 | Doğrudan (Bench: Yok) | STM32 -> MOC | 3.3V | Triyak gate PWM | Üretim |
| **W11** | NUCLEO-G474RE | PC8 | CN10 | Pin 2 | SW1 | Yok | N/A | N/A | N/A | DIP Switch | SW1 | Doğrudan (GND'ye) | DIP -> STM32 | 0V | Adres Seçimi | Ortak |
| **W12** | NUCLEO-G474RE | PC9 | CN10 | Pin 1 | SW2 | Yok | N/A | N/A | N/A | DIP Switch | SW2 | Doğrudan (GND'ye) | DIP -> STM32 | 0V | Adres Seçimi | Ortak |
| **W13** | NUCLEO-G474RE | PC10 | CN7 | Pin 1 | SW3 | Yok | N/A | N/A | N/A | DIP Switch | SW3 | Doğrudan (GND'ye) | DIP -> STM32 | 0V | Adres Seçimi | Ortak |
| **W14** | NUCLEO-G474RE | PC11 | CN7 | Pin 2 | SW4 | Yok | N/A | N/A | N/A | DIP Switch | SW4 | Doğrudan (GND'ye) | DIP -> STM32 | 0V | Adres Seçimi | Ortak |
| **W15** | NUCLEO-G474RE | PB10 | CN7 | Pin 25 | TX | Yok | N/A | N/A | N/A | MAX485 #1 | Pin 4 (DI) | Doğrudan | STM32 -> MAX | 3.3V | RS485 İletim | Ortak |
| **W16** | NUCLEO-G474RE | PB11 | CN10 | Pin 18 | RX | Yok | N/A | N/A | N/A | MAX485 #1 | Pin 1 (RO) | Doğrudan | MAX -> STM32 | 5V* (FT) | RS485 Alım | Ortak |
| **W17** | NUCLEO-G474RE | PB1 | CN7 | Pin 24 | DE/RE | Yok | N/A | N/A | N/A | MAX485 #1 | Pin 2+3 | Doğrudan | STM32 -> MAX | 3.3V | RS485 Yön Kontrolü | Ortak |
| **W18** | ESP32-S3 | GPIO8 | Header | N/A | TX | Yok | N/A | N/A | N/A | MAX485 #2 | Pin 4 (DI) | Doğrudan | ESP32 -> MAX | 3.3V | RS485 İletim | Ortak |
| **W19** | ESP32-S3 | GPIO18 | Header | N/A | RX | R-DIV | 10k/18k | MAX Pin 1 | GND | MAX485 #2 | Pin 1 (RO) | Bölücü üzerinden | MAX -> ESP32 | 5V -> 3.3V | RS485 Alım | Ortak |
| **W20** | ESP32-S3 | GPIO5 | Header | N/A | DE/RE | Yok | N/A | N/A | N/A | MAX485 #2 | Pin 2+3 | Doğrudan | ESP32 -> MAX | 3.3V | RS485 Yön Kontrolü | Ortak |
| **W21** | ESP32-S3 | GPIO17 | Header | N/A | TX | Yok | N/A | N/A | N/A | Nextion HMI | RX (Sarı) | Doğrudan | ESP32 -> HMI | 3.3V | HMI Veri Gönderme | Ortak |
| **W22** | ESP32-S3 | GPIO16 | Header | N/A | RX | Yok | N/A | N/A | N/A | Nextion HMI | TX (Mavi) | Doğrudan | HMI -> ESP32 | 3.3V | HMI Veri Alma | Ortak |
| **W23** | MAX485 #1 | A | Pin 6 | N/A | RS485_A| Yok | N/A | N/A | N/A | MAX485 #2 | Pin 6 (A) | Doğrudan | MAX <-> MAX | Diff | RS485 Diferansiyel | Ortak |
| **W24** | MAX485 #1 | B | Pin 7 | N/A | RS485_B| Yok | N/A | N/A | N/A | MAX485 #2 | Pin 7 (B) | Doğrudan | MAX <-> MAX | Diff | RS485 Diferansiyel | Ortak |
