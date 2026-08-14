# PHASE 6.2 FINAL POWER WIRING

> [!CAUTION]
> Güç bağlantılarında en ufak bir hata MCU ve komponentlerin yanmasına sebep olabilir.
> USB VBUS veya NUCLEO 5V raylarını KESİNLİKLE ESP32 5V veya harici adaptör 5V ile kısa devre YAPMAYINIZ. Her kart kendi gücünü (USB vb.) kullanarak çalışmalı, ancak sadece **ORTAK GND** hattı ile referanslanmalıdır.

## 1. TEMEL GÜÇ KAYNAKLARI (BENCH SETUP)
- **NUCLEO-G474RE**: Mini-USB kablosu ile bilgisayara bağlı. Kendi üzerindeki ST-LINK regülatörü aracılığıyla board'a güç sağlar.
- **ESP32-S3-N16R8**: Mikro-USB / Type-C (board'a göre değişir) kablosu ile bilgisayara bağlı.
- **Nextion HMI**: Ayrı ve güçlü bir USB adaptör veya stabil ESP32 5V çıkışından beslenmeli (Ekranlar fazla akım çeker).

## 2. COMMON LOGIC GND (ORTAK ŞASE) BARA BAĞLANTILARI
Sistemdeki *tüm* veri iletiminin düzgün çalışabilmesi için tek bir sağlam GND hattı çekilmelidir. Aşağıdakilerin tümü birbirine (örneğin breadboard üzerindeki tek bir mavi hatta) BAĞLANMALIDIR:

1. NUCLEO-G474RE `GND` (CN7 veya CN10 üzerindeki GND pinlerinden herhangi biri)
2. ESP32-S3 `GND` pini
3. Nextion HMI `GND` (Siyah kablo)
4. X9C103S Pin 4 (`VSS`)
5. X9C103S Pin 6 (`VL`)
6. MAX485 #1 Pin 5 (`GND`)
7. MAX485 #2 Pin 5 (`GND`)
8. DIP Switch Common / Toprak hattı (Switch'lerin ana ayağı)
9. Voltaj Bölücü Alt Direnci (`R-DIV-BOT` Terminal B)

## 3. 5V VE 3.3V DAĞITIMI
MCU'ların ürettiği 5V/3.3V pinleri komponentleri beslemek için aşağıdaki şekilde kablolanmalıdır:

| Güç Veren Kaynak | Kaynak Pini | Hedef Komponent | Hedef Pini | Amaç |
|:---|:---|:---|:---|:---|
| NUCLEO-G474RE | 5V (CN7 Pin 18) | MAX485 #1 | Pin 8 (VCC) | RS485 Master çipini beslemek (5V şart) |
| NUCLEO-G474RE | 5V (CN7 Pin 18) | X9C103S | Pin 8 (VCC) | X9C çipini beslemek |
| NUCLEO-G474RE | 3.3V (CN7 Pin 16)| X9C103S | Pin 3 (VH) | X9C Potansiyometre tepe gerilimini (VH) 3.3V max limitine ayarlamak |
| ESP32-S3 | 5V (VBUS / 5V) | MAX485 #2 | Pin 8 (VCC) | RS485 Slave çipini beslemek (5V şart) |
| Harici Adaptör | 5V | Nextion HMI | VCC (Kırmızı) | Ekran için izole ve yeterli akım kaynağı (ESP32 USB gücü yetersiz kalırsa tavsiye edilir) |

> [!WARNING]
> NUCLEO 5V (CN7-18) çıkışını, ESP32'nin 5V çıkışına BAĞLAMAYINIZ.
> Her MCU kendi bağlı olduğu MAX485'i beslemelidir. X9C, Nucleo'dan; HMI ise kendi beslemesinden veya ESP32'den beslenmelidir.
