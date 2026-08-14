> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# STM32 ↔ ESP32 Protokol ve İletişim Analiz Raporu

> **Doküman Statüsü:** Lead Embedded Systems Engineer Protocol Audit Output  
> **Tarih:** 10 Ağustos 2026  
> **Kapsam:** ASCII Line Protocol Over Shared Multi-Drop UART Bus

---

## 1. Fiziksel Katman ve Parametreler

| Parametre | ESP32-S3 Master | STM32G4 Slave | Uyum / Durum |
|---|---|---|---|
| **Donanım Portu** | `Serial1` (GPIO18=RX, GPIO8=TX) | `USART3` (PC10=TX, PC11=RX) | **Uyumlu** |
| **Baud Rate** | 115200 Baud (`STM_BAUD`) | 115200 Baud (`huart3.Init.BaudRate`) | **%100 Uyumlu** |
| **Çerçeve Yapısı** | 8 Data, No Parity, 1 Stop (8N1) | 8 Data, No Parity, 1 Stop (8N1) | **%100 Uyumlu** |
| **Sonlandırıcı Karakter** | `\n` (`\r` yok sayılır/temizlenir) | `\n` (`\r` temizlenir) | **%100 Uyumlu** |
| **Topoloji** | Multi-Drop Bus (1 Master, N Slave) | Multi-Drop Bus (Adres Okuma `T<ID>:`) | **%100 Uyumlu** |

---

## 2. Paket Biçimleri ve Karşılaştırma Matrisi

### 2.1. ESP32 → STM32 Komut Yönü

| Komut | ESP32 Gönderim Söz Dizimi | STM32 İşleme Söz Dizimi | Kırpma / Doğrulama Kuralları |
|---|---|---|---|
| Süre Ayarı | `T<id>:SET_TIME:<min>\n` | `strncmp(cmd, "SET_TIME:", 9)` | $[0, 100]$ dk aralığına kırpılır (`TIME_MINUTES_MAX`). |
| Sıcaklık Ayarı | `T<id>:SET_TEMP:<degC>\n` | `strncmp(cmd, "SET_TEMP:", 9)` | $[0.0, 90.0]$ °C aralığına kırpılır (`strtof`). |
| Güç Ayarı | `T<id>:SET_POWER:<pct>\n` | `strncmp(cmd, "SET_POWER:", 10)` | $[0, 100]$ % aralığına kırpılır. |
| Frekans Ayarı | `T<id>:SET_FREQ:<freq>\n` | `strncmp(cmd, "SET_FREQ:", 9)` | Yalnızca `28` veya `40` kabul edilir. |
| Başlat | `T<id>:START\n` | `strcmp(cmd, "START")` | `mode != SYS_MODE_FAULT` ise `RUNNING` yapılır. |
| Durdur / Ack | `T<id>:STOP\n` | `strcmp(cmd, "STOP")` | `mode = SYS_MODE_IDLE`, `fault_flags` sıfırlanır. |
| Kart ID Atama | `T0:SET_ID:<new_id>\n` | `strncmp(cmd, "SET_ID:", 7)` | Yalnızca $[1, 10]$ kabul edilir, Flash'a yazılır. |

### 2.2. STM32 → ESP32 Telemetri Paketi Ayrıştırması

STM32 paket formatı (`esp32_uart.c:219`):
$$\text{STAT,}\langle\text{TankID}\rangle\text{,}\langle\text{mode}\rangle\text{,}\langle\text{remaining\_sec}\rangle\text{,}\langle\text{temp\_x10}\rangle\text{,}\langle\text{relay}\rangle\text{,}\langle\text{power\_pct}\rangle\text{,}\langle\text{frequency\_khz}\rangle\text{,}\langle\text{fault\_flags}\rangle\backslash\text{n}$$

| Eleman | STM32 Veri Tipi | ESP32 Okuma & Ayrıştırma | Uyum |
|---|---|---|---|
| `TankID` | `uint8_t` (`MY_TANK_ID`) | `rest.substring(0, p1).toInt()` | **Uyumlu** ($1..10$) |
| `mode` | `const char*` ("RUNNING"/"FAULT"/"IDLE") | `rest.substring(p1 + 1, p2)` | **Uyumlu** |
| `remaining_sec` | `uint16_t` | `rest.substring(p2 + 1, p3).toInt()` | **Uyumlu** |
| `temp_x10` | `int` (`current_temp_c * 10.0f`) | `rest.substring(p3 + 1, p4).toInt() / 10.0` | **Uyumlu** |
| `relay` | `uint8_t` (`relay_state`) | `rest.substring(p4 + 1, p5).toInt()` | **Uyumlu** |
| `power_pct` | `uint8_t` (`actual_power_pct`) | `rest.substring(p5 + 1, p6).toInt()` | **Uyumlu** |
| `frequency_khz`| `uint8_t` (`frequency_khz`) | `rest.substring(p6 + 1, p7).toInt()` | **Uyumlu** |
| `fault_flags` | `uint8_t` (`fault_flags` bitmask) | `rest.substring(p7 + 1).toInt()` | **Uyumlu** |

---

## 3. Protokol Seviyesinde Tespit Edilen Uyuşmazlıklar ve Riskler

1. **CRC / Checksum Eksikliği**:
   - ASCII satır haberleşmesinde CRC veya Checksum mekanizması **yoktur**. Otobüsteki endüstriyel gürültü nedeniyle 1 karakter bozulursa paket düşürülür veya yanlış değer ayrıştırılabilir.
2. **ACK / NACK Cevap Eksikliği**:
   - ESP32'nin gönderdiği komutlara (`SET_TIME`, `START` vb.) STM32 tarafında onay cevabı (ACK) dönülmemektedir. ESP32 komutun ulaşıp ulaşmadığını ancak bir sonraki telemetri paketinden (500ms sonra) anlayabilir.
