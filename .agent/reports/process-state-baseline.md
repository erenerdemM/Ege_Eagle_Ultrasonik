# EAGLEULTRASONİK — Süreç Durumları ve Güvenli Kapanış Mimarisi (Process State Baseline)

> **Doküman Statüsü:** Process State Machine & Safe Shutdown Specification  
> **Tarih:** 10 Ağustos 2026  
> **Aşama:** Phase 4.6 Baseline Adjustment

---

## 1. Proses Bitiş ve Kapanış Gereksinimi (Process Shutdown Requirements)

Operatör HMI'dan `STOP` verdiğinde, süre dolduğunda (`remaining_seconds == 0`) veya bir arıza (`SYS_MODE_FAULT`) oluştuduğunda sistemin **güvenli kapatma (safe shutdown)** yapması gerekmektedir.

### 1.1. İstenen Güvenli Kapanış Sırası (Target Safe Shutdown Sequence)
1. Ultrasonik güç çıkısı kesilmeli (Triyak kapalı).
2. TRIAC kontrolü güvenli OFF durumuna geçmeli (`PC6 LOW`).
3. Isıtıcı röle kesilmeli (`PB15 LOW`).
4. Rezistanslar fiziksel olarak kapanmalı.
5. Süreç zamanlayıcısı durdurulmalı.
6. Sistem durumu `SYS_MODE_IDLE` durumuna geçmeli.

---

## 2. Mevcut Kaynak Kod Doğrulaması (Source Code Trace)

### 2.1. Süre Dolduğunda (`ProcessTimer` Expiry - [process_timer.c:38](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/process_timer.c#L38))
```c
if (g_system_state.remaining_seconds == 0U) {
    g_system_state.mode = SYS_MODE_IDLE; // Mod IDLE yapılır
}
```
- Mod `SYS_MODE_IDLE` yapıldığında:
  - `UltrasonicPWM_Process()` döngüsünde `g_system_state.mode != SYS_MODE_RUNNING` olduğu için `TriacForceOff()` çağrılır $\rightarrow$ **Ultrasonik KAPATILIR.**
  - `HeaterRelay_Process()` döngüsünde `g_system_state.mode != SYS_MODE_RUNNING` olduğu için `RelaySet(0)` çağrılır $\rightarrow$ **Isıtıcı KAPATILIR.**

### 2.2. STOP Komutu Geldiğinde ([esp32_uart.c:135](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L135))
```c
if (strcmp(cmd, "STOP") == 0) {
    g_system_state.mode = SYS_MODE_IDLE;
}
```
- Yan Etki: Süre sıfırlanır, yükler ana süper-döngü turlamasında anında kapatılır.

---

## 3. Ortak Emniyet Fonksiyonu Önerisi: `SAFE_PROCESS_STOP()`

Mevcut koddaki dağınık mod değişimlerini tek bir emniyet noktasında toplamak için Phase 5'te aşağıdaki ortak fonksiyonun kullanılması mimari olarak standartlaştırılmıştır:

```c
void SAFE_PROCESS_STOP(void) {
    g_system_state.mode = SYS_MODE_IDLE;
    TriacForceOff();                   // PC6 LOW, TIM15 Stop
    RelaySet(0);                       // PB15 LOW
    g_system_state.remaining_seconds = 0U;
}
```
Bu fonksiyon `STOP` komutu, `ProcessTimer` sıfırlaması, RX Zaman Aşımı ve `SYS_MODE_FAULT` geçişlerinin tamamında ortak çağrılacaktır.
