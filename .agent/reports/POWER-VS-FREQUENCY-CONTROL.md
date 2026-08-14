> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONİK — Bağımsız İki Kontrol Ekseni (Power vs Frequency Control)

> **Doküman Statüsü:** Control Architecture & Hardware Modeling Specification  
> **Tarih:** 10 Ağustos 2026  
> **Revizyon:** Rev.2 Design Freeze Challenge

---

## 1. Mimari Kural: İki Bağımsız Kontrol Ekseni (Two Independent Control Axes)

Bu sistemde ultrasonik jeneratör kontrolü **tek bir döngü veya tek bir algoritma değildir**. İki ayrı fiziksel donanım kartını ve iki ayrı kontrol eksenini yöneten iki bağımsız alt sistem bulunmaktadır:

$$\begin{aligned}
\text{\textbf{CONTROL AXIS A (Güç Kontrolü):}} & \quad \text{STM32} \xrightarrow{\text{TIM15 + PC6 Triac Gate}} \text{Power Card} \xrightarrow{\text{Faz Açısı}} \text{Çıkış Gücü (\%0 - \%100)} \\
\text{\textbf{CONTROL AXIS B (Frekans Kontrolü):}} & \quad \text{HMI/ESP32/STM32} \xrightarrow{\text{PB12/13/14 Bit-Bang}} \text{X9C103S Pot} \xrightarrow{\text{Hybrid Card}} \text{Çıkış Frekansı (28kHz / 40kHz)}
\end{aligned}$$

---

## 2. CONTROL AXIS A — Güç Kontrol Mimarisi (Power Control Architecture)

### 2.1. İletim Zinciri
$$\text{STM32} \xrightarrow{\text{PC7 ZC EXTI9\_5}} \text{TIM15 OPM Timer} \xrightarrow{\text{PC6 TRIAC\_GATE}} \text{Power Card Interface} \xrightarrow{\text{Triyak Faz Açısı}} \text{Çıkış Gücü}$$

### 2.2. İşleyiş ve Kod Doğrulaması ([ultrasonic_pwm.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/ultrasonic_pwm.c))
- **Açma / Kapama (ON/OFF):** Mod `SYS_MODE_RUNNING` yapıldığında güç çıkışı aktif olur. `SYS_MODE_IDLE` veya `SYS_MODE_FAULT` modunda triyak ateşi kesilir (`TriacForceOff`).
- **Güç Yüzdesi (Power Percentage):** Operatör %0 ile %100 arasında güç seçer.
- **Ateşleme Gecikmesi (Firing Delay):**
  $$D_{\text{target}} = 9500 - \frac{9000 \times P_{\text{pct}}}{100} \quad (\mu s)$$
  - %0 Güç: 9500 µs gecikme (minimum faz açısı, neredeyse sıfır akım).
  - %100 Güç: 500 µs gecikme (maksimum faz açısı, tam iletim).
- **Soft-Start Rampalama:** `current_delay_us`, her zero-cross darbesinde `SOFTSTART_RAMP_STEP_US` (20 µs) adımlarla $D_{\text{target}}$ değerine doğru kademeli olarak düşürülür.
- **Kesin Kural:** **X9C103S dijital potansiyometresi bu zincire ve triyak zamanlamasına kesinlikle DÂHİL DEĞİLDİR.**

---

## 3. CONTROL AXIS B — Frekans Kontrol Mimarisi (Frequency Control Architecture)

### 3.1. İletim Zinciri
$$\text{USER} \xrightarrow{\text{HMI}} \text{ESP32} \xrightarrow{\text{UART}} \text{STM32} \xrightarrow{\text{PB12/13/14 Bit-Bang}} \text{X9C103S Pot} \xrightarrow{\text{Hybrid Card}} \text{Power Card} \xrightarrow{\text{Çıkış Frekansı}}$$

### 3.2. İşleyiş ve Kod Doğrulaması ([x9c103s.c](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/x9c103s.c))
- **Donanım Değişimi (Design Fact 3):** Power Card üzerindeki Hybrid Card'da önceden bulunan iki mekanik trim pot sökülmüş, yerine X9C103S dijital potansiyometresi bağlanmıştır.
- **Adımlama Mekanizması:**
  - `PB12` (CS): Active-Low entegre seçimi.
  - `PB13` (U/D): High=Yukarı (direnç artırma), Low=Aşağı (direnç azaltma).
  - `PB14` (INC): Düşen kenar adımlama strob darbesi.
- **Frekans Konumları:**
  - **28 kHz Konumu:** Silecek 40. adıma getirilir (`Step 40` $\approx 40.4\text{ k}\Omega$).
  - **40 kHz Konumu:** Silecek 90. adıma getirilir (`Step 90` $\approx 90.9\text{ k}\Omega$).
- **Ampirik Eşleme (Empirical Mapping):** Kod içerisindeki 40. ve 90. adım değerleri empirik ölçümlerle (deneysel olarak) Hybrid Card rezonans dirençlerine karşılık gelecek şekilde sabitlenmiştir.

---

## 4. X9C ≠ Triac Rule (Kritik Yanılsamanın Düzeltilmesi)

> [!CAUTION]
> **DÜZELTME:** Önceki taslaklarda yapılabilecek "X9C potansiyometresi triyak zamanlamasını veya triyak gücünü kontrol eder" şeklindeki tüm yorumlar **HAKİKATE AYKIRIDIR**.
> - **X9C103S:** Yalnızca Hybrid Card'ın rezonans çalışma frekansını (28kHz veya 40kHz) belirleyen **Dijital Direnç Sürücüsüdür**.
> - **Triyak (TIM15 + PC6):** Yalnızca Power Card'ın çıkış gücünü (%0 - %100) belirleyen **Faz Açısı Ateşleme Sürücüsüdür**.

---

## 5. İki Eksenin Etkileşim Matrisi (Interaction Matrix)

Yazılım seviyesinde bu iki kontrol ekseni tamamen bağımsızdır ve matristeki 4 kombinasyonun tamamı yazılım tarafından sorunsuzca desteklenmektedir:

| Güç Setpoint (% Power) | Frekans Setpoint (kHz) | STM32 Triyak Firing Delay ($D_{\text{us}}$) | STM32 X9C Silecek Adımı | Donanım Durumu |
|:---:|:---:|:---:|:---:|---|
| **%50** | **28 kHz** | 5000 µs | Step 40 | %50 Güçte 28 kHz Ultrasonik Kavitasyon |
| **%50** | **40 kHz** | 5000 µs | Step 90 | %50 Güçte 40 kHz Ultrasonik Kavitasyon |
| **%100** | **28 kHz** | 500 µs | Step 40 | %100 Tam Güçte 28 kHz Kavitasyon |
| **%100** | **40 kHz** | 500 µs | Step 90 | %100 Tam Güçte 40 kHz Kavitasyon |
