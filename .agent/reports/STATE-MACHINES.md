> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONİK — Durum Makineleri ve Operasyon Şemaları (State Machines)

> **Doküman Statüsü:** Technical Master Specification  
> **Tarih:** 10 Ağustos 2026

---

## 1. STM32 Firmware State Machine

STM32 firmware'i `SystemMode_t` enum'ı (`system_state.h`) üzerinden 3 ana durumla yönetilir:

```mermaid
stateDiagram-v8
    [*] --> SYS_MODE_IDLE: Boot / Initialization

    SYS_MODE_IDLE --> SYS_MODE_RUNNING: RX 'T<ID>:START' (Eğer fault_flags == 0)
    SYS_MODE_RUNNING --> SYS_MODE_IDLE: ProcessTimer remaining_seconds == 0
    SYS_MODE_RUNNING --> SYS_MODE_IDLE: RX 'T<ID>:STOP'

    SYS_MODE_RUNNING --> SYS_MODE_FAULT: PT100 Kopuk/Kısa Devre (0x01 / 0x02)
    SYS_MODE_RUNNING --> SYS_MODE_FAULT: Zero-Cross Kaybı > 500ms (0x04)
    SYS_MODE_IDLE --> SYS_MODE_FAULT: PT100 Kopuk/Kısa Devre

    SYS_MODE_FAULT --> SYS_MODE_IDLE: RX 'T<ID>:STOP' (Manual Fault Reset/Ack)
```

### 1.1. STM32 Durum Geçiş Tablosu (Transition Table)

| Current State | Event | Condition | Next State | Side Effect / Action |
| --- | --- | --- | --- | --- |
| `SYS_MODE_IDLE` | RX `T<ID>:START` | `fault_flags == 0` | `SYS_MODE_RUNNING` | `remaining_seconds` yüklenir, soft-start başlar. |
| `SYS_MODE_IDLE` | RX `T<ID>:START` | `fault_flags != 0` | `SYS_MODE_FAULT` | Komut reddedilir, IDLE kalınır/FAULT tetiklenir. |
| `SYS_MODE_RUNNING` | Timer Expiry | `remaining_seconds == 0` | `SYS_MODE_IDLE` | Triyak ve röle kapatılır, süreç tamamlandı. |
| `SYS_MODE_RUNNING` | RX `T<ID>:STOP` | None | `SYS_MODE_IDLE` | Triyak ve röle anında kapatılır. |
| `SYS_MODE_RUNNING` | Sensör Arızası | PT100 Kopuk/Kısa | `SYS_MODE_FAULT` | `g_system_state.mode = FAULT`, yükler kapatılır. |
| `SYS_MODE_RUNNING` | ZC Kaybı | $t_{\text{zc\_gap}} > 500\text{ms}$ | `SYS_MODE_FAULT` | `FAULT_ZERO_CROSS_LOST` seti verilir, triyak kapatılır. |
| `SYS_MODE_FAULT` | RX `T<ID>:STOP` | None | `SYS_MODE_IDLE` | `fault_flags = FAULT_NONE`, arıza onaylandı/resetlendi. |

---

## 2. ESP32 Master State Machine & HMI Senkronizasyonu

ESP32 tarafında her tank için bağımsız `makine_calisiyor[MAX_GOZ]` ve `stm_bagli[MAX_GOZ]` durumları tutulur.

### 2.1. ESP32 Durum Geçiş Tablosu

| Current State | Event / Input | Condition | Next State | Action / Communication |
| --- | --- | --- | --- | --- |
| Offline | $t_{\text{gap}} < 3000\text{ms}$ (Telemetri Alındı) | `tank_id` Geçerli | Online (`stm_bagli = true`) | Reconnect senkronizasyonu `stmSetpointleriGonder()` tetiklenir. |
| Online | $t_{\text{gap}} > 3000\text{ms}$ | None | Offline (`stm_bagli = false`) | HMI uyarısı, tank pasife çekilir. |
| Idle | HMI `P_HIZLI` / `CMD_START` | `isKartBagli() == true` | Running | Setpointler yollanır, `stmStart()` gönderilir. |
| Idle | HMI `P_HIZLI` / `CMD_START` | `isKartBagli() == false` | Idle (Blocked) | `baslatmaEngelliMi()` tetiklenir, HMI'ya "Kart Yok!" yazılır. |
| Running | HMI `CMD_STOP` | None | Idle | `stmStop()` yollanır. |
| Any | STM32 Telemetri `FAULT` | `fault > 0` | Fault | HMI'ya "HATA! KOD:x" ve `--.-` basılır. |

---

## 3. Reçete Sistemi ve NVS Saklama (P1, P2, P3)

ESP32 Flash NVS hafızasında 3 varsayılan reçete şablonu tutulur:
- **P1 (Hızlı/Hassas Yıkama):** Varsayılan 15 Dk / 40 °C.
- **P2 (Standart Yıkama):** Varsayılan 20 Dk / 50 °C.
- **P3 (Yoğun Yıkama):** Varsayılan 25 Dk / 60 °C.

Kullanıcı Page 2 ekranından bu değerleri değiştirebilir. `P_SAVE` butonuna basıldığında NVS belleğine (`Preferences`) yazılır. `P1_SEL`/`P2_SEL`/`P3_SEL` seçildiğinde ilgili değerler seçili gözün `hedef_sure` ve `hedef_sicaklik` dizisine aktarılır ve STM32'ye iletilir.
