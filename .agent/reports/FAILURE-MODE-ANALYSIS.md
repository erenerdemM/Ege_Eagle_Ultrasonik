> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONİK — Hata Senaryoları ve Emniyet Analizi (Failure Mode Analysis)

> **Doküman Statüsü:** Safety & Failure Mode Engineering Analysis  
> **Tarih:** 10 Ağustos 2026

---

## 16 Hata Senaryosu Matrisi (16 Failure Scenarios)

| Scenario # | Failure Scenario | Detection Mechanism | System State | System Action / Response | Is Safe State? |
| --- | --- | --- | --- | --- | --- |
| **1** | **PT100 Disconnected (Açık Devre)** | ADC Raw $\ge 4090$ veya Temp $> 110^\circ\text{C}$ | `SYS_MODE_FAULT` | `g_system_state.mode = FAULT`, `fault_flags |= 0x01`, Röle ve Triyak Kapatılır | **Evet** |
| **2** | **PT100 Shorted (Kısa Devre)** | ADC Raw $\le 5$ veya Temp $< -10^\circ\text{C}$ | `SYS_MODE_FAULT` | `g_system_state.mode = FAULT`, `fault_flags |= 0x02`, Röle ve Triyak Kapatılır | **Evet** |
| **3** | **ESP32 Disconnected (UART Kablo Kopması)** | Yok (STM32 Tarafında Timeout Yok) | `SYS_MODE_RUNNING` | Süreç bitene kadar (100 dk'ya kadar) Isıtıcı ve Ultrasonik çalışmaya devam eder | ❌ **Hayır (Kritik Emniyet Riski)** |
| **4** | **STM32 Disconnected (Slave Yanıt Vermiyor)** | ESP32 `isKartBagli()` ($t_{\text{gap}} > 3000\text{ms}$) | ESP32 Offline | ESP32 tankı offline yapar, HMI'ya "Kart Yok!" basar, yeni START vermeyi engeller | **Evet** |
| **5** | **UART Noise (Gürültülü Otobüs)** | STM32 / ESP32 Parser (`T<ID>:` eşleşmez) | Existing State | Karakter düşürülür veya paket sessizce yok sayılır | **Kısmen (Gürültü komut ezebilir)** |
| **6** | **UART Packet Corruption** | Delimiter ve Format Okuma Hatası | Existing State | Bozuk paket parse edilemez, atlanır | **Kısmen (CRC yok)** |
| **7** | **Zero-Cross Missing (Şebeke Kesintisi)** | STM32 $t_{\text{zc}} > 500\text{ms}$ Kontrolü | `SYS_MODE_FAULT` | `fault_flags |= 0x04`, Triyak zorla kapatılır (`TriacForceOff`) | **Evet** |
| **8** | **X9C Potansiyometre Adım Kaçırma** | Yok (Açık Çevrim Bit-Bang) | Existing State | Yanlış adımlama frekansı (örn 28kHz yerine 32kHz), güç verimsizliği | ❌ **Hayır (Güç Kaybı)** |
| **9** | **STM32 MCU Reset (Kahverengi Karartma / Noise)** | Boot Sequence (`main.c`) | `SYS_MODE_IDLE` | MCU varsayılan IDLE açılır, röle LOW başlar, ESP32 reconnect algılayıp setpoint yollar | **Evet** |
| **10** | **ESP32 MCU Reset (WDT / OOM Crash)** | Boot Sequence (`setup()`) | Reset / Boot | ESP32 yeniden başlar, NVS okur, STM32'ye setpoint gönderir | **Evet** |
| **11** | **Power Loss (Şebeke Elektrik Kesintisi)** | Donanımsal Güç Düşüşü | Power Off | Tüm röle ve triyaklar fiziksel olarak kapanır, voltaj gidince reset | **Evet** |
| **12** | **Duplicate Tank ID (Otobüste Aynı ID'li Kartlar)**| Yok (`BENCH_DEV_MODE_ID=1` riski) | Otobüs Çakışması | İki kart aynı anda TX yapar, veriler çöp olur, haberleşme kilitlenir | ❌ **Hayır (Sistem Kilitlenir)** |
| **13** | **Invalid Tank ID (`T99:START`)** | `ProcessLine()` ID Kontrolü | Existing State | Adres eşleşmediği için STM32 komutu sessizce yutar, tepki vermez | **Evet** |
| **14** | **Invalid Command (`T1:XYZ`)** | `ProcessLine()` Komut Ayrıştırma | Existing State | Tanınmayan komut if/else dallarından düşer, yok sayılır | **Evet** |
| **15** | **Malformed Packet (`T1:SET_TEMP:ABC`)** | `strtof` / `strtol` Parsing | Standard Clamping | Geçersiz nümerik değer `0` veya mevcut değer olarak yorumlanır | **Evet** |
| **16** | **Timer Expiry (Süreç Bitişi)** | `ProcessTimer_Process` `remaining_seconds == 0` | `SYS_MODE_IDLE` | Otomatik durdurma: `g_system_state.mode = SYS_MODE_IDLE`, yükler kapatılır | **Evet** |

---

## 2. Güvenlik Mimarisi Değerlendirmesi

| Protection Mechanism | Implementation Location | Trigger Condition | Action Taken | Current Limitations |
| --- | --- | --- | --- | --- |
| **PT100 Rail & Range Guard** | `pt100_adc.c` | ADC raw $\ge 4090$, $\le 5$ veya $< -10^\circ\text{C}$, $> 110^\circ\text{C}$ | Mod `FAULT` yapılır, röle ve triyak anında kapatılır | Dijital filtre yok; gürültü spike'ları sahte trip yapabilir |
| **Zero-Cross Loss Guard** | `ultrasonic_pwm.c` | $t_{\text{zc\_gap}} > 500\text{ms}$ | Mod `FAULT` yapılır, `TriacForceOff()` çağrılır | Bench modunda (`ZC_BENCH_TEST_MODE=1`) bypass edilebilir |
| **Soft-Start Power Ramp** | `ultrasonic_pwm.c` | RUNNING moda geçişte | Delay 20µs adımlarla hedef güce çekilir | Süper-döngü hızına bağlı olduğu için çok hızlı rampalar |
| **ESP32 Offline Guard** | `ekran_kontrol.ino` | $t_{\text{gap}} > 3000\text{ms}$ | Kart offline yapılır, START komutları engellenir | Boot'taki ilk 3 saniyede 0 başlatma hatası var |
| **Hardware Watchdog (IWDG)**| **EKSİK** | MCU Kilitlenmesi | Donanımsal Reset Atma | **KODDA YOKtur. MCU donarsa ısıtıcı açık kalabilir!** |
