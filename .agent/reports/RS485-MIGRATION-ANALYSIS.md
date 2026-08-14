> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONİK — RS485 Geçiş ve Çoklu Tank Analizi (RS485 Migration Analysis)

> **Doküman Statüsü:** Protocol & Multi-Tank Architecture Analysis  
> **Tarih:** 10 Ağustos 2026  
> **Revizyon:** Rev.2 Design Freeze Challenge

---

## 1. Uygulama Katmanı vs Fiziksel Katman Ayrımı

```
APPLICATION LAYER (Uygulama Katmanı):
- ASCII Plaintext String Formatı: "T<ID>:<CMD>:<PARAM>\n"
- Telemetri Formatı: "STAT,<ID>,<MODE>,<TEMP>,<TIME>,<SET_TEMP>,<SET_POWER>,<FREQ>,<FAULT>\n"
- Baud Rate: 115200 Baud 8N1
- Bu katman RS485 gecisine %100 UYUMLUDUR.

PHYSICAL LAYER (Fiziksel Katman):
- Prototip: UART TTL (0V - 3.3V) Doğrudan Kablo
- Nihai Sistem: RS485 Diferansiyel Voltaj (A - B hatları)
- Transceiver Eklemesi Uygulama Protokolünü Değiştirmez.
```

---

## 2. Çoklu Tank Adresleme ve Otobüs Yönlendirme (Multi-Tank Address Routing)

Nihai üretim sisteminde 1 adet ESP32 Master, RS485 veriyolu üzerindeki n adet (1..10) STM32 Slave ile haberleşecektir.

### 2.1. Komut Yönlendirme Akışı
1. ESP32 bir tanka komut göndereceğinde adres öneki ekler: `T1:START`, `T2:SET_TEMP:50` vb.
2. Tüm STM32 kartları RS485 veriyolundaki bu paketi alır ve `USART3_IRQHandler` kesmesinde `rx_line` tamponuna yazar.
3. `ProcessLine()` fonksiyonunda adres ayrıştırılır:
   ```c
   /* esp32_uart.c:98 */
   if (tank_id != 0 && (uint8_t)tank_id != MY_TANK_ID) return;
   ```
4. Eğer komuttaki `tank_id` kartın kendi `MY_TANK_ID` değerine eşitse veya evrensel yayın `T0:` ise komut işlenir; aksi halde paket sessizce düşürülür.

### 2.2. Telemetri Çakışma Yönetimi (Bus Contention & Collision)
- **Problem:** Birden fazla kartın aynı anda telemetri basması RS485 veriyolunu çökertir (Bus Collision).
- **Mevcut Çözüm:** Her STM32 kartı 500 ms'de bir telemetri basar. `MY_TANK_ID` değerine göre telemetri zamanlayıcısına offset eklenmeli veya ESP32 Polling (Sorgu-Yanıt) mimarisine geçilmelidir.
- **Duplicate ID Riski:** Eğer iki kart aynı ID'ye ayarlanırsa (`BENCH_DEV_MODE_ID = 1` hatası gibi) her ikisi de aynı anda TX yapar ve veriler çöp olur.

---

## 3. Donanımsal RS485 Bilinmeyenleri (Hardware UNKNOWNs)

Aşağıdaki elektriksel parametreler kaynak koddan öğrenilemez ve donanım şeması incelemesi gerektirir:
- Transceiver entegre modeli (MAX485, SN65HVD72, ADM2483 vb.)
- Otomatik Yön Kontrolü (Auto-Direction Sense) varlığı veya DE/RE pini seçimi
- Uç 120 Ω diferansiyel terminasyon dirençlerinin varlığı
- Hat boşta (Idle) iken A/B voltaj kararlılığı için Fail-Safe Bias dirençleri
