> [!WARNING]
> SUPERSEDED BY PHASE 4.6 DESIGN BASELINE

# EAGLEULTRASONİK — ESP32 Firmware Independent Audit Report

> **Auditor:** Senior Embedded Systems Expert
> **Target:** `esp32/ekran_kontrol/ekran_kontrol.ino` and related configs
> **Date:** 10 August 2026

## 1. Verification of Previous Findings

### ID: BUG-HIGH-02
SEVERITY: HIGH
STATUS: CONFIRMED
FILE: esp32/ekran_kontrol/ekran_kontrol.ino
LINE: 484, 581
FUNCTION: `komutIsle()`
TITLE: ESP32 HMI Olay İşleyicide Bloklayıcı delay() Kullanımı
OBSERVED BEHAVIOR: `P_SAVE` ve `SRV_SAVE` komutları işlenirken UI buton renk animasyonu için `delay(400)` ve `delay(600)` bloklayıcı (blocking) fonksiyonları kullanılmaktadır.
EXPECTED BEHAVIOR: Olay döngüsünün (event loop) ve donanım tamponlarının kilitlenmemesi için gecikmelerin `millis()` tabanlı (non-blocking) durum makineleri ile yönetilmesi beklenir.
EVIDENCE: 
```cpp
nextionGonder("b_save.bco=2016");
delay(400); // 400ms bloklanma
nextionGonder("b_save.bco=50712");
```
ROOT CAUSE: HMI görsel animasyonu için senkron (blocking) bekleme tercih edilmiş.
IMPACT: Bu süre zarfında ESP32'nin ana döngüsü (`loop()`) durur. STM32 cihazlarından UART üzerinden gelecek telemetri veya hata mesajları UART donanım tamponunda (FIFO) birikir. Çoklu drop otobüste (örneğin 10 cihaz) veri kaybına ve tampon taşmasına (buffer overflow) neden olur.
REPRODUCTION / FAILURE SCENARIO: Göz ayarları düzenlenip "SAVE" tuşuna basıldığında ESP32 600ms kör kalır. Bu esnada STM32'lerden biri acil hata (FAULT) paketi gönderirse ESP32 bunu okuyamaz veya düşürür.
RECOMMENDED FIX: UI bildirim zamanlayıcısını global bir `millis()` değişkenine bağlayın ve `loop()` içerisinde süresi dolunca eski renge çevirin.
VERIFICATION METHOD: Kayıt işleminde UART1 (STM32) trafiğinin kesintiye uğramadığı osiloskop veya log üzerinden doğrulanmalıdır.
CONFIDENCE: HIGH
CATEGORY: RELIABILITY RISK

### ID: BUG-MED-03
SEVERITY: MEDIUM
STATUS: PARTIALLY CONFIRMED
FILE: esp32/ekran_kontrol/ekran_kontrol.ino
LINE: 131, 568-571
FUNCTION: `nvsYukle()`, `komutIsle()`
TITLE: max_goz_sayisi Sınır Aşımı ve NVS Sanitizasyon Zafiyeti
OBSERVED BEHAVIOR: `komutIsle()` içindeki sınır kontrolü mantıksal olarak doğru görünse de (`if (max_goz_sayisi < MAX_GOZ - 1)`), kalıcı hafızadan (NVS) `max_goz_sayisi` okunduğunda herhangi bir sınır kontrolü (sanitization) yapılmamaktadır.
EXPECTED BEHAVIOR: NVS'den okunan değişkenlerin dizi sınırlarına uygun olup olmadığı kontrol edilmelidir.
EVIDENCE:
```cpp
max_goz_sayisi = prefs.getInt("maxgoz", max_goz_sayisi); // Sınır kontrolü yok!
```
ROOT CAUSE: NVS veri bütünlüğüne koşulsuz güven.
IMPACT: Eğer NVS bozulursa veya eski bir yazılımdan kalma büyük bir değer (örneğin 100) içerirse, `UP` komutu ile `temp_goz` 100'e kadar çıkarılabilir. `SEL` yapıldığında `secili_goz = 100` olur ve `hedef_sure[100]` gibi dizi dışı hafıza bölgelerine yazma yapılarak ESP32 çökertilir (Memory Corruption / Hard Fault).
REPRODUCTION / FAILURE SCENARIO: Flash belleğe yapay olarak `maxgoz=20` yazılır. Cihaz açılır, kullanıcı ekrandan UP butonuna basar, göz no 20 seçilir, START verilir ve MCU çöker.
RECOMMENDED FIX: `nvsYukle()` içinde `if(max_goz_sayisi >= MAX_GOZ) max_goz_sayisi = MAX_GOZ - 1;` kontrolü eklenmelidir.
VERIFICATION METHOD: NVS'e 255 değeri yazılıp cihazın 10 sınırında kalıp kalmadığı doğrulanmalıdır.
CONFIDENCE: HIGH
CATEGORY: BUG

## 2. New Findings

### ID: ESP32-BUG-01
SEVERITY: HIGH
STATUS: CONFIRMED
FILE: esp32/ekran_kontrol/ekran_kontrol.ino
LINE: 79-82, 316
FUNCTION: `isKartBagli()`, `setup()`
TITLE: Boot Sonrası İlk 3 Saniyede Yanlış Bağlantı Durumu (False Positive Connection)
OBSERVED BEHAVIOR: Sistem ilk açıldığında `stm_son_veri_zamani` dizisi `0` ile başlatılır. `isKartBagli()` fonksiyonu sadece `(millis() - stm_son_veri_zamani[goz_id]) < STM_BAGLANTI_TIMEOUT` kontrolü yapmaktadır. `stm_bagli` flag'i değerlendirilmez.
EXPECTED BEHAVIOR: Hiç veri göndermemiş bir kart bağlantısız (offline) kabul edilmelidir.
EVIDENCE:
```cpp
bool isKartBagli(uint8_t goz_id) {
  if (goz_id == 0 || goz_id >= MAX_GOZ) return false;
  return (millis() - stm_son_veri_zamani[goz_id]) < STM_BAGLANTI_TIMEOUT; // millis() - 0 < 3000
}
```
ROOT CAUSE: Boot sırasında `stm_son_veri_zamani` `0` olduğu için, `millis()` 3000'e ulaşana kadar matematiksel eşitsizlik yanlışlıkla doğru (true) sonuç verir.
IMPACT: Cihaz açılır açılmaz ilk 3 saniye içinde kullanıcı (veya HMI boot komutları) hatta bağlı olmayan bir cihaza START emri gönderebilir. Sistem `baslatmaEngelliMi()` doğrulamasını atlar.
REPRODUCTION / FAILURE SCENARIO: Sisteme güç verin, 2 saniye içinde hızlıca `P_HIZLI` gönderin. Kart bağlı olmamasına rağmen "HIZLI YIKAMA DEVAM EDIYOR" metni ekrana basılır ve hayalet cihaza komut atılır.
RECOMMENDED FIX: Kontrole `stm_bagli` durumunu da ekleyin: `return stm_bagli[goz_id] && ((millis() - stm_son_veri_zamani[goz_id]) < STM_BAGLANTI_TIMEOUT);`
VERIFICATION METHOD: Sistemi başlatıp 1 saniye sonra komut vererek işlemin engellendiği doğrulanır.
CONFIDENCE: HIGH
CATEGORY: SAFETY RISK

### ID: ESP32-BUG-02
SEVERITY: MEDIUM
STATUS: CONFIRMED
FILE: esp32/ekran_kontrol/ekran_kontrol.ino
LINE: 238-255, 593-605
FUNCTION: `stmTelemetryIsle()`, `hatOku()`
TITLE: String Sınıfı Kötüye Kullanımı ve Ciddi Heap Fragmantasyonu (Memory Leak/Fragmentation)
OBSERVED BEHAVIOR: Saniyede defalarca çağrılan seri iletişim ve ayrıştırma fonksiyonlarında `String` sınıfı yoğun olarak ve yer ayrılmadan (reserve) kullanılmaktadır. `stmTelemetryIsle` içerisinde her paket için en az 8 ayrı `substring` ayrıştırması yapılmaktadır.
EXPECTED BEHAVIOR: Sık çalışan telemetri ayrıştırıcılarında C tipi statik char dizileri, `strtok()` veya `sscanf()` kullanılmalıdır.
EVIDENCE:
```cpp
  // hatOku() içerisinde her byte için:
  tampon += c; // Sürekli bellek yeniden tahsisi (realloc)

  // stmTelemetryIsle() içerisinde:
  String mode_str = rest.substring(p1 + 1, p2);
  // ve toplam 7 adet toInt() çağrısı için yeni objeler.
```
ROOT CAUSE: Hızlı prototipleme amacıyla `String` sınıfının maliyetinin göz ardı edilmesi.
IMPACT: Sürekli bellek tahsisi ve iadesi, RAM'de parçalanmaya (heap fragmentation) neden olur. 10 kartlık bir otobüste günde milyonlarca kez çalışacak bu blok, ESP32'nin birkaç gün içerisinde Out of Memory çökmesi (Crash/Reboot) yaşamasına yol açacaktır.
REPRODUCTION / FAILURE SCENARIO: Multi-drop hatta 10 tank bağlanarak 48 saat kesintisiz çalıştırıldığında cihaz reset atacaktır.
RECOMMENDED FIX: Gelen tamponlar `char[]` dizilerine dönüştürülmeli ve `sscanf(satir.c_str(), "STAT,%d,%15[^,],%d,%d,%d,%d,%d,%d", ...)` ile parse edilmelidir. En azından global String'ler için `setup()` içerisinde `.reserve(128)` yapılmalıdır.
VERIFICATION METHOD: Seri parse işlemi sırasında boş heap miktarının zamanla azalmadığı (`ESP.getFreeHeap()`) izlenmelidir.
CONFIDENCE: HIGH
CATEGORY: RELIABILITY RISK

### ID: ESP32-BUG-03
SEVERITY: MEDIUM
STATUS: CONFIRMED
FILE: esp32/ekran_kontrol/ekran_kontrol.ino
LINE: 404-407
FUNCTION: `komutIsle()` (CMD_START)
TITLE: Komut Pacing (Back-to-Back Tx) ve Tampon Ezme Riski
OBSERVED BEHAVIOR: ESP32, bir prosesi başlatırken STM32'ye arka arkaya gecikmesiz 4 farklı UART komutu fırlatmaktadır.
EXPECTED BEHAVIOR: Slave MCU'ların komut işleme kapasitesine göre paketler arası kısa bekleme payları bırakılmalıdır.
EVIDENCE:
```cpp
stmSetTime(hedef_sure[secili_goz]);
stmSetTemp(hedef_sicaklik[secili_goz]);
stmSetPower(guc_seviyesi);
stmStart();
```
ROOT CAUSE: UART TX donanım FIFO'sunun yeterince büyük olmasına güvenilerek, hedef işlemcinin (STM32) RX karakter kapasitesinin/hızının varsayılması.
IMPACT: STM32 önceki denetimlerde (BUG-HIGH-01) belirlendiği gibi tek satırlık bir tampon kullanmaktadır. ESP32 komutları hiç beklemeden yolladığı için, ilk komut işlenirken 2. ve 3. komutların karakterleri STM32 tarafından yutulabilir (dropped). Sonuç olarak tank ısınmayabilir veya ayarlanan sürede yıkamayabilir.
REPRODUCTION / FAILURE SCENARIO: STM32 bağlı iken START komutu verildiğinde cihazın "Power" değerini varsayılan değerde bıraktığı veya "Time" değerini almadığı görülür.
RECOMMENDED FIX: Tasarım mimarisine bir "Komut Kuyruğu" (Tx Queue) eklenmeli veya her komut arasına `vTaskDelay(pdMS_TO_TICKS(10))` konmalıdır. En ideal çözüm, tek bir birleşik komut (`START,sure,sicaklik,guc`) formatı kullanmaktır.
VERIFICATION METHOD: STM32'nin aldığı değerler ekrandan okunarak teyit edilir.
CONFIDENCE: MEDIUM
CATEGORY: PERFORMANCE ISSUE
