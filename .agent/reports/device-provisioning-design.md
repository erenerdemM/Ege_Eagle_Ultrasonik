# EAGLEULTRASONİK — Cihaz Kimliklendirme ve Devreye Alma Mimarisi (Device Provisioning Design)

> **Doküman Statüsü:** Provisioning Architecture Specification  
> **Tarih:** 10 Ağustos 2026  
> **Aşama:** Phase 4.6 Baseline Adjustment

---

## 1. Factory Default ID = 1 Analizi

Üretim hattından yeni çıkan veya Flash hafızası henüz konfigüre edilmemiş bir STM32 Slave kartının varsayılan olarak **Factory ID = 1** ile gelmesi bir bug değil, **bir devreye alma (commissioning) gereksinimidir**.

Kartın fabrikadan çıktığında henüz kalıcı bir ID'si yoktur. Kartın ESP32 ile haberleşebilmesi ve HMI ekranından benzersiz bir ID (örn. Havuz 3) atanabilmesi için ilk temasta Factory ID=1 adresini yanıtlaması gerekir.

### 1.1. Gerçek Risk Tanımı
Asıl risk tek başına `Factory ID = 1` olması değildir. **Asıl risk**, kalıcı ID atanmamış birden fazla sıfır kartın veya aynı kalıcı ID'ye sahip 2 cihazın **aynı anda aktif üretim otobüsüne (RS485) takılmasıdır**. Bu durumda aynı adrese gelen komuta her iki kart da yanıt verecek ve veriyolunda fiziksel veri çakışması (Bus Collision) oluşacaktır.

---

## 2. Devreye Alma Durumları (States)

- **Uncommissioned / Factory State:** Kart Flash belleğinde veya DIP switch'inde kalıcı adres tanımlanmamış durum. Varsayılan ID = 1.
- **Commissioned / Permanent State:** HMI üzerinden `SET_ID:<N>` emri verilerek Flash Page 127'ye kalıcı yazılmış benzersiz adres durumu ($N \in [1, 10]$).

---

## 3. Alternatif Devreye Alma Mimarilerinin Karşılaştırılması

| Opsiyon | Mimari Adı | Avantajlar | Dezavantajlar | Üretim Uygunluğu |
| --- | --- | --- | --- | --- |
| **A** | **Manual Sequential Provisioning** | Sıfır ekstra kod. Kartlar tek tek otobüse bağlanır, HMI'dan ID atanır. | Operatörün kartları tek tek takması gerekir. | **Prototip ve Küçük Seri için İdeal** |
| **B** | **HMI Assisted Commissioning Mode** | HMI'da "Yeni Kart Ekle" ekranı açılır. Otobüs adresleme moduna geçer. | HMI yazılımında özel menü gerektirir. | **ÖNERİLEN STANDART (Seçilen Yöntem)** |
| **C** | **STM32 Unique MCU UID Reading** | Her STM32'nin 96-bit benzersiz donanım UID'si okunur. Çakışma imkânsızdır. | HMI'da 96-bit UID girmek zordur, arayüz gerektirir. | **İleri Seviye Emniyet (V2)** |
| **D** | **Temporary Provisioning Address (T99)** | Sıfır kartlar varsayılan `T99:` adresini dinler, atama yapılınca kalıcı ID'ye geçer. | ID 1 çakışmasını önler fakat 2 sıfır kart takılırsa T99 çakışır. | **İyi Alternatif** |

---

## 4. Önerilen Standart Yöntem (HMI Assisted Commissioning)

1. Üretimde veya servis esnasında yeni bir kart takılacağı zaman operatör HMI'dan **"Devreye Alma Modu"**nu açar.
2. Yeni kart tek başına hatta takılır (Factory ID=1).
3. HMI üzerinden "Bu Kartı Havuz #X Olarak Kaydet" butonuna basılır.
4. ESP32 `T1:SET_ID:X` yayınlar. STM32 yeni ID'yi Flash Page 127'ye yazar ve kendini resetler.
5. Kart artık kalıcı `MY_TANK_ID = X` olarak çalışır.
