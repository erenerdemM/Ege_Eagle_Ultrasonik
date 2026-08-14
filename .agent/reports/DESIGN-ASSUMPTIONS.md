> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONİK — Tasarım Soruları ve Varsayımlar (Design Assumptions)

> **Doküman Statüsü:** Design Freeze & Engineering Assumptions Audit  
> **Tarih:** 10 Ağustos 2026

---

## 1. Koddan Kesin Cevaplanamayan Tasarım Soruları Matrisi

| Question / Design Issue | Status Tag | Verified Source Evidence | Required Action / Decision |
| --- | --- | --- | --- |
| **1. AC Şebeke Frekansı Gerçekten 50Hz mi?** | `KNOWN` (Code) / `REQUIRES HARDWARE VERIFICATION` | `ultrasonic_pwm.c:16` `#define AC_HALF_CYCLE_US 10000UL` 50Hz varsayar. | 60Hz şebekeler için donanımsal ZC periyot ölçümü adaptif yapılmalıdır. |
| **2. Isıtıcı Röle Donanımı Active-High mı?** | `KNOWN` (Code) | `main.h:88` `HEATER_RELAY_Pin PB15`, `HAL_GPIO_WritePin(..., SET)` röleyi açar. | Donanım kartında N-channel MOSFET/Röle sürücünün High ile tetiklendiği teyit edilmelidir. |
| **3. X9C103S Zamanlama Şartları Nedir?** | `KNOWN` (Datasheet) / `REQUIRES DESIGN DECISION` | Datasheet $t_{INC} \ge 1\mu s, t_{CPH} \ge 10\mu s$. Kodda `X9C_DelayUs(3U)` kullanılmış. | `__disable_irq()` kesme karartması kaldırılıp donanımsal timer adımlamasına geçilmelidir. |
| **4. UART Fiziksel Katmanı TTL mi RS485 mi?** | `REQUIRES HARDWARE VERIFICATION` | Kod seviyesinde `huart3` standart UART 115200 8N1'dir. RS485 DE/RE yön pin kontrolü **yoktur**. | Eğer fiziksel katman RS485 ise Otomatik Yön Kontrollü (Auto-Direction) transceiver çipi kullanıldığı teyit edilmelidir. |
| **5. Maksimum Tank Sayısı Gerçekten 10 mu?** | `KNOWN` (Code) | `ekran_kontrol.ino` `#define MAX_GOZ 11` (1..10) ve DIP SW 4-bit (`raw > 10 return 10`). | 10 tank üst sınırdır. |
| **6. Harici Donanımsal Termik / Interlock Var mı?** | `REQUIRES HARDWARE VERIFICATION` | Kodda yazılımsal bimetal/termostat kesme girişi yoktur. | Sıvıız/kuru ısıtmaya karşı rezistans serisinde fiziksel donanımsal termik sigorta olduğu doğrulanmalıdır. |

---

## 2. Mevcut Uygulama vs Amaçlanan Sistem Karşılaştırması

```
CURRENT IMPLEMENTATION:
- ASCII Plaintext UART Protocol (No Checksum, No ACK)
- Single-line UART Buffer (Burst command drops)
- Hardcoded BENCH_DEV_MODE_ID = 1 (Bus collision risk)
- Missing Hardware IWDG Watchdog (Thermal runaway risk)
- Unfiltered PT100 ADC conversion (Relay chatter risk)
- 600us Global Interrupt Blackout during X9C pot stepping

INTENDED PRODUCTION SYSTEM:
- Robust Multi-Drop Protocol (CRC8 Checksum + Command ACK/Retry)
- Ring Buffer / DMA UART Pipeline (Zero packet drops)
- Automatic DIP Switch / Flash Override ID Resolution
- Hardware IWDG Activated (1000ms Fail-Safe Reset)
- Moving Average Filtered PT100 ADC (Stable relay hysteresis)
- Asynchronous Non-Blocking X9C Stepping (Zero IRQ blackout)
```
