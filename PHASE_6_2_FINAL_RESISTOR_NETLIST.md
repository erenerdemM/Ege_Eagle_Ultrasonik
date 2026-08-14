# PHASE 6.2 FINAL RESISTOR NETLIST

> [!CAUTION]
> Bu doküman **TEK VE NİHAİ** fiziksel direnç listesidir. 
> Lütfen montaj yaparken uç noktalarına (Terminal A ve Terminal B) birebir uyunuz.

| ID | Değer | Terminal A | Terminal B | Nerede Kullanılıyor | Mod |
|:---|:---|:---|:---|:---|:---|
| **R-HTR-FB** | 1kΩ | STM32 PB15 (CN10-26) | STM32 PA4 (CN7-32) | Heater output physical loopback diagnostik geri besleme | Bench |
| **R-TRC-FB** | 1kΩ | STM32 PC6 (CN10-4) | STM32 PA6 (CN7-34) | Triyak gate pulse physical loopback diagnostik geri besleme | Bench |
| **R-ZC-SIM** | 1kΩ | ESP32 GPIO4 | STM32 PC7 (CN5-2) | Zero-cross 100Hz hardware simülasyon bağlantısı | Bench |
| **R-X9C-VW** | 1kΩ | X9C Pin 5 (VW) | STM32 PA0 (CN7-28) | ADC1_IN1 girişi için akım sınırlayıcı / okuma direnci | Ortak |
| **R-DIV-TOP** | 10kΩ | MAX485 #2 Pin 1 (RO) | ESP32 GPIO18 node | RS485 RO çıkışını (5V) ESP32 RX girişine (3.3V) düşürmek için voltaj bölücü üst direnç | Ortak |
| **R-DIV-BOT** | 18kΩ | ESP32 GPIO18 node | Ortak Logic GND | RS485 RO çıkışını (5V) ESP32 RX girişine (3.3V) düşürmek için voltaj bölücü alt direnç | Ortak |
| **R-TERM-1** | 120Ω | MAX485 #1 Pin 6 (A) | MAX485 #1 Pin 7 (B) | RS485 diferansiyel veri hattı terminasyon direnci (STM32 Master ucu) - *A ve B arasına paralel* | Ortak |
| **R-TERM-2** | 120Ω | MAX485 #2 Pin 6 (A) | MAX485 #2 Pin 7 (B) | RS485 diferansiyel veri hattı terminasyon direnci (ESP32 ucu) - *A ve B arasına paralel* | Ortak |

> [!IMPORTANT]
> - RS485 TX/RX data hatlarına seri 1kΩ direnç **EKLENMEYECEKTİR**. Mimari doğrudan bağlantıya dayanır.
> - **R-DIV-TOP** ve **R-DIV-BOT** bir voltaj bölücüdür. MAX485 RO 5V çıkarır, bu devreden geçtikten sonra ESP32 GPIO18 (RX) ucunda yaklaşık 3.2V - 3.3V seviyesine düşer.
> - **R-TERM-1** ve **R-TERM-2** seri BAĞLANMAZ. RS485 A ve B terminalleri arasına paralel (köprü şeklinde) bağlanır.
> - MOC3021 (Üretim kartındaki 150Ω) testte kullanılmaz. Üretim kartı basıldığında test loopbackleri (R-HTR-FB, R-TRC-FB, R-ZC-SIM) devreden kalkacaktır.
