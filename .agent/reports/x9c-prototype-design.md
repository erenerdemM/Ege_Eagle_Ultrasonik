# EAGLEULTRASONİK — X9C103S Prototip ve Teşhis Mimarisi (X9C Prototype Design)

> **Doküman Statüsü:** X9C Diagnostic & Prototype Engineering Feasibility  
> **Tarih:** 10 Ağustos 2026  
> **Aşama:** Phase 4.6 Baseline Adjustment

---

## 1. X9C103S Gerçek Donanım Rolü

X9C103S entegresi, Güç Kartı üzerindeki Hibrit Kart'ta bulunan iki adet mekanik trim potansiyometrenin yerine lehimlenen $10\text{ k}\Omega$ 100 adımlı dijital potansiyometredir.
- **Amacı:** Operatörün HMI üzerinden seçtiği 28 kHz veya 40 kHz çalışma frekansını Hibrit Kart rezonans direnç değerine dönüştürmektir.

---

## 2. Prototip Fikri: X9C Direncini STM32 ADC Üzerinden Ölçme Fizibilitesi

Prototip geliştirme ve teşhis (diagnostic/calibration) aşamasında X9C103S silecek (wiper) direncinin STM32 ADC kanalı üzerinden doğrulanması fikri incelenmiştir.

### 2.1. Ölçüm Devresi Topolojisi (Voltaj Bölücü)
$$V_{\text{ADC}} = V_{\text{CC}} \times \frac{R_{\text{X9C}}}{R_{\text{bilinen}} + R_{\text{X9C}}}$$

Buradan X9C anlık direnci hesaplanır:
$$R_{\text{X9C}} = R_{\text{bilinen}} \times \frac{V_{\text{ADC}}}{V_{\text{CC}} - V_{\text{ADC}}}$$

### 2.2. Fizibilite Değerlendirmesi
- **Kısıt 1 (Datasheet & Silecek Akımı):** X9C103S silecek terminalinden ($I_W$) maksimum $4.4\text{ mA}$ akım akabilir. Voltaj bölücü direnci $R_{\text{bilinen}} \ge 10\text{ k}\Omega$ seçilmelidir.
- **Kısıt 2 (Hibrit Kart Devresi Etkileşimi):** ADC ölçüm pini Hibrit Kart'ın osilatör rezonans devresine doğrudan bağlanırsa osilatörün empedansını bozar ve frekans kaymasına yol açar.
- **Karar:** ADC ile X9C direnci ölçümü **üretim gereksinimi DEĞİLDİR**. Sadece masaüstü kalibrasyon/teşhis için **"Prototype Calibration Candidate"** olarak kaydedilmiştir.

---

## 3. Ultrasonik Çıkış Frekansını ADC/EXTI ile Ölçme Konusu (Scope Limitation)

> [!NOTE]
> **KAPSAM DIŞI:** X9C direnci üzerinden Power Card ultrasonik çıkış frekansının STM32 ile ölçülmesi bu aşamada **KAPSAM DIŞIDIR**.
> Bu özellik **"FUTURE ENHANCEMENT / PHASE V2"** olarak dökümante edilmiştir.
> 
> **Mevcut Aşama Öncelikleri:**
> 1. X9C adımlama kontrolünün kararlı çalışması.
> 2. 28 kHz seçiminde Step 40 adımlaması.
> 3. 40 kHz seçiminde Step 90 adımlaması.
> 4. Kesme karartması (`__disable_irq`) olmadan adımlama yapılması.
