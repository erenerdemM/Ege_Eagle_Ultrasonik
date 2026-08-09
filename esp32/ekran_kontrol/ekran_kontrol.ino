#include <Arduino.h>
#include <Preferences.h>
#include "esp_timer.h"

// --- NEXTION (HMI) UART ---
#define RXD2 16
#define TXD2 17

// --- STM32 (SLAVE) UART ---
// GPIO26/27 kullanilmaz: ESP32-S3-N16R8 uzerinde bu pinler SPI flash hattina (SPICS1/SPIHD) sabittir.
#define STM_RXD 18
#define STM_TXD 8
#define STM_BAUD 115200 // huart3 (STM32) sabit 115200; ESP32 tarafi bununla eslesmeli
#define STM_BAGLANTI_TIMEOUT 3000

// STM32 fault_flags bitleri (bkz. STM32/system_state.h) - PT100 acık/kısa devre tespiti icin
#define FAULT_PT100_OPEN_BIT   0x01
#define FAULT_PT100_SHORT_BIT  0x02

// --- ZERO-CROSS SIMULATOR (HIL MASA TESTI) ---
// GPIO4 -> STM32 PC7'ye kablolanir: "Zero-Cross Missing" fault'unu masada engeller.
// NOT: ESP32-S3 LEDC donanimi 100Hz + 8-bit cozunurluk kombinasyonu icin gecerli bir
// clock divider uretemiyor ("div_param=0" hatasi). Bu yuzden LEDC yerine ozel bir
// esp_timer donanim zamanlayicisi ile GPIO4 dogrudan toggle edilir (LEDC'den bagimsiz, non-blocking).
#define ZC_SIM_PIN 4
#define ZC_SIM_FREQ_HZ 100
#define ZC_SIM_HALF_PERIOD_US (1000000ULL / ZC_SIM_FREQ_HZ / 2) // 100Hz kare dalga -> 5000us yari periyot

// ==========================================
// 1. SİSTEM DEĞİŞKENLERİ (GLOBAL HAFIZA)
// ==========================================
#define MAX_GOZ 11 // index 0 kullanilmaz; gecerli Tank ID araligi 1..10 (multi-drop hatta en fazla 10 STM32 slave)

int secili_goz = 1;      
int temp_goz = 1;        

// --- P1, P2, P3 GLOBAL ŞABLONLARI ---
int aktif_program = 0;       // Page 0'da o an seçilen program
int duzenlenen_program = 1;  // Page 2'de o an ayarı değiştirilen program (Varsayılan P1)

int p_sure[4] = {0, 15, 20, 25};               
int p_sicaklik[4] = {0, 40, 50, 60};           

String girilen_sifre = "";
String dogru_sifre = "123456";

int guc_seviyesi = 50;       
int kart_id = 1;             
int max_goz_sayisi = 3;      

// ==========================================
// 2. HER GÖZÜN BAĞIMSIZ BEYNİ (ARRAY/DİZİLER)
// ==========================================
bool makine_calisiyor[MAX_GOZ];
String durum_metni[MAX_GOZ];
int hedef_sure[MAX_GOZ];      
int hedef_sicaklik[MAX_GOZ];  
int kalan_saniye[MAX_GOZ];     // STM32'den gelen gerçek kalan süre (sn)
float anlik_sicaklik[MAX_GOZ]; // STM32'den gelen gerçek sıcaklık (temp_x10/10.0)

// --- STM32 TELEMETRİ DURUMU (her göz/tank icin bagimsiz; multi-drop hatta N slave ayni ESP32'ye baglidir) ---
int stm_fault[MAX_GOZ];
int stm_relay[MAX_GOZ];
int stm_pwr[MAX_GOZ];
bool stm_bagli[MAX_GOZ];
unsigned long stm_son_veri_zamani[MAX_GOZ];

unsigned long sonGuncellemeZamani = 0;
unsigned long hilWdtDebugZamani = 0;  // HIL_DEEP_DEBUG

String gelenMesaj = "";
String stmMesaj = "";
String usbMesaj = "";  // HIL_TEST_MOD: PC (COM10, USB Debug) uzerinden gelen HIL komut arabellegi

Preferences prefs;

// Kart (STM32 tank karti) son STM_BAGLANTI_TIMEOUT ms icinde telemetri gonderdi mi?
bool isKartBagli(uint8_t goz_id) {
  if (goz_id == 0 || goz_id >= MAX_GOZ) return false;
  return (millis() - stm_son_veri_zamani[goz_id]) < STM_BAGLANTI_TIMEOUT;
}

// Secili goz offline ise process baslatma komutlarini engeller ve HMI'yi uyarir.
// true donerse cagiran komut dali islemeyi durdurmalidir (islem baslatilamaz).
bool baslatmaEngelliMi() {
  if (isKartBagli(secili_goz)) return false;

  Serial.println("--> HATA: GÖZ " + String(secili_goz) + " KART BAGLI DEGIL! ISLEM BASLATILAMIYOR.");
  durum_metni[secili_goz] = "Kart Yok!";
  nextionGonder("t_durum.txt=\"Kart Yok!\"");
  return true;
}

// --- ZERO-CROSS SIMULATOR: esp_timer donanim zamanlayicisi durumu ---
static esp_timer_handle_t zcSimTimer = nullptr;
volatile bool zcSimPinState = false;

static void zcSimTimerCallback(void* arg) {
  zcSimPinState = !zcSimPinState;
  digitalWrite(ZC_SIM_PIN, zcSimPinState ? HIGH : LOW);
}

// Donanim zamanlayicisini kurar ve GPIO4'te surekli 100Hz kare dalga uretimini baslatir.
void zcSimBaslat() {
  pinMode(ZC_SIM_PIN, OUTPUT);
  digitalWrite(ZC_SIM_PIN, LOW);
  zcSimPinState = false;

  const esp_timer_create_args_t zcTimerArgs = {
    .callback = &zcSimTimerCallback,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "zc_sim_timer"
  };
  esp_timer_create(&zcTimerArgs, &zcSimTimer);
  esp_timer_start_periodic(zcSimTimer, ZC_SIM_HALF_PERIOD_US);
}

// ==========================================
// 3. NVS (KALICI HAFIZA) FONKSİYONLARI
// ==========================================
void nvsYukle() {
  prefs.begin("ultra", false);
  for (int i = 1; i <= 3; i++) {
    p_sure[i] = prefs.getInt(("pS" + String(i)).c_str(), p_sure[i]);
    p_sicaklik[i] = prefs.getInt(("pT" + String(i)).c_str(), p_sicaklik[i]);
  }
  guc_seviyesi = prefs.getInt("guc", guc_seviyesi);
  kart_id = prefs.getInt("kartid", kart_id);
  max_goz_sayisi = prefs.getInt("maxgoz", max_goz_sayisi);
  prefs.end();

  // HIL_DEEP_DEBUG: dump every NVS key read back, so the host can confirm persisted values loaded correctly
  for (int i = 1; i <= 3; i++) {
    Serial.println("DEBUG_ESP32: NVS_READ key=pS" + String(i) + " val=" + String(p_sure[i]));
    Serial.println("DEBUG_ESP32: NVS_READ key=pT" + String(i) + " val=" + String(p_sicaklik[i]));
  }
  Serial.println("DEBUG_ESP32: NVS_READ key=guc val=" + String(guc_seviyesi));
  Serial.println("DEBUG_ESP32: NVS_READ key=kartid val=" + String(kart_id));
  Serial.println("DEBUG_ESP32: NVS_READ key=maxgoz val=" + String(max_goz_sayisi));
}

void nvsKaydet() {
  prefs.begin("ultra", false);
  for (int i = 1; i <= 3; i++) {
    prefs.putInt(("pS" + String(i)).c_str(), p_sure[i]);
    prefs.putInt(("pT" + String(i)).c_str(), p_sicaklik[i]);
  }
  prefs.putInt("guc", guc_seviyesi);
  prefs.putInt("kartid", kart_id);
  prefs.putInt("maxgoz", max_goz_sayisi);
  prefs.end();

  // HIL_DEEP_DEBUG: confirm what was actually written this call, for NVS round-trip verification
  for (int i = 1; i <= 3; i++) {
    Serial.println("DEBUG_ESP32: NVS_WRITE key=pS" + String(i) + " val=" + String(p_sure[i]));
    Serial.println("DEBUG_ESP32: NVS_WRITE key=pT" + String(i) + " val=" + String(p_sicaklik[i]));
  }
  Serial.println("DEBUG_ESP32: NVS_WRITE key=guc val=" + String(guc_seviyesi));
  Serial.println("DEBUG_ESP32: NVS_WRITE key=kartid val=" + String(kart_id));
  Serial.println("DEBUG_ESP32: NVS_WRITE key=maxgoz val=" + String(max_goz_sayisi));
}

// ==========================================
// 4. STM32 UART TX (KOMUT GÖNDERME)
// ==========================================
// Multi-drop bus: her komut "T<mevcut_goz>:" adresiyle gonderilir; sadece o ID'ye
// sahip STM32 karti komutu isler, digerleri sessizce yok sayar (bkz. esp32_uart.c).
void stmGonder(String komut) {
  String adresli = "T" + String(secili_goz) + ":" + komut;
  Serial1.print(adresli);
  String log = adresli;
  log.trim();
  Serial.println("[ESP->STM] " + log);
}

void stmSetTime(int dk) {
  stmGonder("SET_TIME:" + String(dk) + "\n");
}

void stmSetTemp(int derece) {
  stmGonder("SET_TEMP:" + String(derece) + "\n");
}

void stmSetPower(int yuzde) {
  stmGonder("SET_POWER:" + String(yuzde) + "\n");
}

void stmStart() {
  stmGonder("START\n");
}

void stmStop() {
  stmGonder("STOP\n"); // aynı zamanda fault-ack görevi görür
}

// T0: bus-wide broadcast (fiziksel olarak bagli STM32, kendi mevcut ID'sinden bagimsiz olarak alir)
void stmSetIdBroadcast(int yeniId) {
  String adresli = "T0:SET_ID:" + String(yeniId) + "\n";
  Serial1.print(adresli);
  String log = adresli;
  log.trim();
  Serial.println("[ESP->STM] " + log);
}

// Aktif gözün güncel setpointlerini STM32'ye gönderir (boot / reconnect / seçim değişimi)
void stmSetpointleriGonder() {
  stmSetTime(hedef_sure[secili_goz]);
  stmSetTemp(hedef_sicaklik[secili_goz]);
  stmSetPower(guc_seviyesi);
}

// HIL_TEST_MOD: "T<digit(s)>:" adres onekiyle baslayan hatlari STM32 bus komutu olarak tanir
// (bkz. esp32_uart.c ProcessLine), boylece PC bunlari dogrudan Serial1'e iletilmek uzere
// COM10 (USB Debug) uzerinden gonderebilir; digerleri HMI komut seti (komutIsle) olarak islenir.
bool isBusKomut(const String &s) {
  if (s.length() < 3 || s.charAt(0) != 'T') return false;
  int i = 1;
  while (i < (int)s.length() && isDigit(s.charAt(i))) i++;
  return (i > 1 && i < (int)s.length() && s.charAt(i) == ':');
}

// ==========================================
// 5. STM32 UART RX (TELEMETRİ AYRIŞTIRMA)
// ==========================================
// STM32 -> ESP32: "STAT,<TankID>,<Mode>,<rem_sec>,<temp_x10>,<relay>,<pwr>,<fault>"
// Gelen veri HER ZAMAN kendi Tank ID'sinin dizisine yazilir (10 tank da arka planda
// izlenir); Nextion ekran guncellemesi ise SADECE TankID, o an secili_goz (mevcut_goz)
// ile eslesirse yapilir.
void stmTelemetryIsle(String satir) {
  if (!satir.startsWith("STAT,")) return;

  String rest = satir.substring(5);
  int p1 = rest.indexOf(',');
  int p2 = rest.indexOf(',', p1 + 1);
  int p3 = rest.indexOf(',', p2 + 1);
  int p4 = rest.indexOf(',', p3 + 1);
  int p5 = rest.indexOf(',', p4 + 1);
  int p6 = rest.indexOf(',', p5 + 1);
  if (p1 == -1 || p2 == -1 || p3 == -1 || p4 == -1 || p5 == -1 || p6 == -1) return; // hatalı çerçeve

  int tank_id = rest.substring(0, p1).toInt();
  String mode_str = rest.substring(p1 + 1, p2);
  int rem_sec = rest.substring(p2 + 1, p3).toInt();
  int temp_x10 = rest.substring(p3 + 1, p4).toInt();
  int relay = rest.substring(p4 + 1, p5).toInt();
  int pwr = rest.substring(p5 + 1, p6).toInt();
  int fault = rest.substring(p6 + 1).toInt();

  if (tank_id < 1 || tank_id >= MAX_GOZ) return; // bozuk/gecersiz Tank ID -> dizi sinirlari disi erisim engellenir

  int g = tank_id;
  kalan_saniye[g] = rem_sec;
  anlik_sicaklik[g] = temp_x10 / 10.0;
  stm_relay[g] = relay;
  stm_pwr[g] = pwr;
  stm_fault[g] = fault;
  makine_calisiyor[g] = (mode_str == "RUNNING");

  bool yeniden_baglandi = !stm_bagli[g];
  stm_bagli[g] = true;
  stm_son_veri_zamani[g] = millis();

  if (fault > 0) {
    durum_metni[g] = "HATA! KOD:" + String(fault);
  } else if (mode_str == "RUNNING") {
    durum_metni[g] = "YIKAMA DEVAM EDIYOR...";
  } else if (rem_sec <= 0 && hedef_sure[g] > 0) {
    durum_metni[g] = "YIKAMA TAMAMLANDI!";
  } else {
    durum_metni[g] = "SISTEM BEKLEMEDE";
  }

  // --- HMI Durum Senkronizasyonu: sadece ekranda gosterilen goz icin aninda gonder ---
  if (g == secili_goz) {
    const char *hmi_durum = (mode_str == "RUNNING") ? "Calisiyor"
                          : (mode_str == "FAULT")   ? "Hata"
                                                     : "Beklemede";
    nextionGonder("t_status.txt=\"" + String(hmi_durum) + "\"");

    if (yeniden_baglandi) stmSetpointleriGonder(); // reconnect senkronizasyonu
  }
}

// ==========================================
// 6. SETUP
// ==========================================
void setup() {
  Serial.begin(115200); // Debug Hub konsolu (USB)
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);         // Nextion HMI
  Serial1.begin(STM_BAUD, SERIAL_8N1, STM_RXD, STM_TXD); // STM32 Slave

  // Zero-cross simulator: GPIO4'te esp_timer ile surekli 100Hz kare dalga (LEDC kullanilmiyor)
  zcSimBaslat();

  for(int i=0; i<MAX_GOZ; i++) {
    makine_calisiyor[i] = false;
    durum_metni[i] = "SISTEM BEKLEMEDE";
    hedef_sure[i] = 0;
    kalan_saniye[i] = 0;
    hedef_sicaklik[i] = 0;
    anlik_sicaklik[i] = 24.0; 
    stm_fault[i] = 0;
    stm_relay[i] = 0;
    stm_pwr[i] = 0;
    stm_bagli[i] = false;
    stm_son_veri_zamani[i] = 0;
  }
  
  nvsYukle();

  Serial.println("[SYS] Boot... NVS setpoints yuklendi:");
  for (int i = 1; i <= 3; i++) {
    Serial.println("[SYS]   P" + String(i) + " = " + String(p_sure[i]) + "dk / " + String(p_sicaklik[i]) + "C");
  }
  Serial.println("[SYS]   Guc=" + String(guc_seviyesi) + "% KartID=" + String(kart_id) + " MaxGoz=" + String(max_goz_sayisi));

  stmSetpointleriGonder(); // boot senkronizasyonu

  Serial.println("--- ULTRASONIK YIKAMA: REÇETE SISTEMI AKTIF ---");
}

void nextionGonder(String komut) {
  Serial2.print(komut);
  Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
}

// ==========================================
// 7. KARAR MERKEZİ
// ==========================================
void komutIsle(String komut) {
  // Savunma hatti: hatOku() zaten tek satir garanti eder, ama komut icinde yine de bir '\n'
  // varsa (ör. henuz reflash edilmemis eski cagiran kod / dogrudan komutIsle cagrisi) burada
  // satirlara bolunup HER biri ayri ayri islenir; birlesik string asla tek komut gibi gecmez.
  int nlIdx = komut.indexOf('\n');
  if (nlIdx != -1) {
    String ilkSatir = komut.substring(0, nlIdx);
    String kalanSatirlar = komut.substring(nlIdx + 1);
    ilkSatir.trim();
    if (ilkSatir.length() > 0) komutIsle(ilkSatir);
    kalanSatirlar.trim();
    if (kalanSatirlar.length() > 0) komutIsle(kalanSatirlar);
    return;
  }

  // HIL_DEEP_DEBUG: raw payload entering the HMI command decoder, regardless of source (Nextion/USB)
  Serial.println("DEBUG_ESP32: HMI_RX raw=\"" + komut + "\"");

  // --- HIZLI PROGRAM (FP) (Page 0) ---
  if (komut == "P_HIZLI") {
    if (baslatmaEngelliMi()) return;

    hedef_sure[secili_goz] = 5;    
    hedef_sicaklik[secili_goz] = 30; 
    kalan_saniye[secili_goz] = 0;
    makine_calisiyor[secili_goz] = true;
    durum_metni[secili_goz] = "HIZLI YIKAMA DEVAM EDIYOR";
    Serial.println("--> ESP32: GÖZ " + String(secili_goz) + " ICIN FP BAŞLATILDI!");
    
    nextionGonder("t_set_sure.txt=\"05\"");
    nextionGonder("t_set_sic.txt=\"30\"");

    stmSetTime(hedef_sure[secili_goz]);
    stmSetTemp(hedef_sicaklik[secili_goz]);
    stmSetPower(guc_seviyesi);
    stmStart();
  }
  
  // --- MOTORU BAŞLAT (Page 0) ---
  else if (komut.startsWith("CMD_START|")) {
    if (baslatmaEngelliMi()) return;

    int fPipe = komut.indexOf('|');
    int sPipe = komut.indexOf('|', fPipe + 1);
    
    if (fPipe != -1 && sPipe != -1) {
        String s_sure = komut.substring(fPipe + 1, sPipe);
        String s_sicaklik = komut.substring(sPipe + 1);
        
        hedef_sure[secili_goz] = s_sure.toInt();
        hedef_sicaklik[secili_goz] = s_sicaklik.toInt();

        // HIL_DEEP_DEBUG: confirm the pipe-delimited HMI payload was parsed into the expected fields
        Serial.println("DEBUG_ESP32: HMI_PARSE cmd=CMD_START sure=" + String(hedef_sure[secili_goz]) + " sicaklik=" + String(hedef_sicaklik[secili_goz]));

        if (hedef_sure[secili_goz] == 0 || hedef_sicaklik[secili_goz] == 0) {
          durum_metni[secili_goz] = "SURE/SICAKLIK GIRIN!";
          Serial.println("--> HATA: Sıfır değerle başlatılamaz!");
        } else {
          makine_calisiyor[secili_goz] = true;
          kalan_saniye[secili_goz] = hedef_sure[secili_goz] * 60; 
          durum_metni[secili_goz] = "YIKAMA DEVAM EDIYOR...";
          Serial.println("--> MOTOR START! GÖZ: " + String(secili_goz) + " Hedef: " + String(hedef_sure[secili_goz]) + " Dk");

          stmSetTime(hedef_sure[secili_goz]);
          stmSetTemp(hedef_sicaklik[secili_goz]);
          stmSetPower(guc_seviyesi);
          stmStart();
        }
    }
  }
  
  // --- DURDUR (Page 0) ---
  else if (komut == "CMD_STOP") {
    makine_calisiyor[secili_goz] = false;
    durum_metni[secili_goz] = "SISTEM DURDURULDU";
    Serial.println("--> MOTOR STOP! GÖZ: " + String(secili_goz) + " Durduruldu.");
    stmStop(); // fault-ack olarak da çalışır
  }

  // --- PROGRAM SEÇİMLERİ (Page 0) - Sayfa değişimi YOK, Sadece veri yükler ---
  else if (komut == "P1_SEL" || komut == "P2_SEL" || komut == "P3_SEL") {
    if (baslatmaEngelliMi()) return;

    aktif_program = (komut == "P1_SEL") ? 1 : ((komut == "P2_SEL") ? 2 : 3);

    hedef_sure[secili_goz] = p_sure[aktif_program];
    hedef_sicaklik[secili_goz] = p_sicaklik[aktif_program];
    kalan_saniye[secili_goz] = 0; 
    makine_calisiyor[secili_goz] = false; 
    durum_metni[secili_goz] = "P" + String(aktif_program) + " SECILDI. START BEKLENIYOR";
    
    // Page 0'daki kutulara programın değerlerini bas
    nextionGonder(String("t_set_sure.txt=\"") + (hedef_sure[secili_goz] < 10 ? "0" : "") + String(hedef_sure[secili_goz]) + "\"");
    nextionGonder("t_set_sic.txt=\"" + String(hedef_sicaklik[secili_goz]) + "\"");
    
    stmSetTime(hedef_sure[secili_goz]);
    stmSetTemp(hedef_sicaklik[secili_goz]);

    Serial.println("--> ESP32: GÖZ " + String(secili_goz) + " için P" + String(aktif_program) + " yüklendi.");
  }

  // --- PROGRAM DÜZENLEME SEÇİMİ (Page 2) ---
  else if (komut == "EDIT_P1" || komut == "EDIT_P2" || komut == "EDIT_P3") {
    duzenlenen_program = (komut == "EDIT_P1") ? 1 : ((komut == "EDIT_P2") ? 2 : 3);
    
    // Page 2'deki yazıları Global P Hafızasındaki değerlerle doldur
    nextionGonder("t0.txt=\"PROGRAM P" + String(duzenlenen_program) + "\"");
    nextionGonder(String("t_set_sure.txt=\"") + (p_sure[duzenlenen_program] < 10 ? "0" : "") + String(p_sure[duzenlenen_program]) + "\"");
    nextionGonder("t_set_sic.txt=\"" + String(p_sicaklik[duzenlenen_program]) + "\"");
    
    Serial.println("--> ESP32: Page 2'de P" + String(duzenlenen_program) + " düzenleniyor.");
  }

  // --- PROGRAM KAYDETME (Page 2) ---
  else if (komut.startsWith("P_SAVE|")) {
    int fPipe = komut.indexOf('|');
    int sPipe = komut.indexOf('|', fPipe + 1);
    
    if (fPipe != -1 && sPipe != -1) {
        String s_sure = komut.substring(fPipe + 1, sPipe);
        String s_sicaklik = komut.substring(sPipe + 1);
        
        // Düzenlenen Programa (Şablona) Kaydet
        p_sure[duzenlenen_program] = s_sure.toInt();
        p_sicaklik[duzenlenen_program] = s_sicaklik.toInt();
        // HIL_DEEP_DEBUG: confirm the pipe-delimited HMI payload was parsed before it hits NVS
        Serial.println("DEBUG_ESP32: HMI_PARSE cmd=P_SAVE prog=" + String(duzenlenen_program) + " sure=" + String(p_sure[duzenlenen_program]) + " sicaklik=" + String(p_sicaklik[duzenlenen_program]));
        nvsKaydet();

        Serial.println("--> KAYIT BASARILI: P" + String(duzenlenen_program) + " güncellendi. (" + s_sure + " Dk / " + s_sicaklik + "C)");
        
        // Sadece yeşil animasyon yap, sayfadan çıkma (Kullanıcı isterse P2'yi de düzenlesin)
        nextionGonder("b_save.bco=2016");
        delay(400);
        nextionGonder("b_save.bco=50712"); 
    }
  }

  // --- KART/GÖZ SEÇİMİ (Page 1) ---
  else if (komut == "PAGE1_OPEN") {
    temp_goz = secili_goz; 
    delay(50); 
    nextionGonder("t0.txt=\"" + String(temp_goz) + "\"");
    Serial.println("--> ESP32: Page 1 açıldı. Mevcut Göz (" + String(temp_goz) + ") ekrana basıldı.");
  }
  else if (komut == "UP") {
    if (temp_goz < max_goz_sayisi) temp_goz++; 
    nextionGonder("t0.txt=\"" + String(temp_goz) + "\""); 
  } 
  else if (komut == "DOWN") {
    if (temp_goz > 1) temp_goz--; 
    nextionGonder("t0.txt=\"" + String(temp_goz) + "\""); 
  }
  else if (komut == "SEL") {
    secili_goz = temp_goz; 
    nextionGonder("page page0"); 
    delay(50); 
    
    // Page 0'ı YENİ SEÇİLEN GÖZÜN HAFIZASIYLA doldur
    nextionGonder("b_goz.txt=\"Goz: " + String(secili_goz) + "\""); 
    nextionGonder(String("t_set_sure.txt=\"") + (hedef_sure[secili_goz] < 10 ? "0" : "") + String(hedef_sure[secili_goz]) + "\"");
    nextionGonder("t_set_sic.txt=\"" + String(hedef_sicaklik[secili_goz]) + "\"");

    stmSetpointleriGonder(); // yeni seçilen gözün setpointlerini STM32'ye ilet
  }
  else if (komut == "BACK") {
    temp_goz = secili_goz; 
    nextionGonder("page page0");
  }

  // --- ŞİFRE VE DİĞER MENÜLER (Buralar aynı) ---
  else if (komut.startsWith("KEY") && komut.length() == 4) {
    char basilanTus = komut.charAt(3); 
    if (isDigit(basilanTus) && girilen_sifre.length() < 6) {
      girilen_sifre += basilanTus; 
      String yildizlar = "";
      for(int i=0; i<girilen_sifre.length(); i++) yildizlar += "*";
      nextionGonder("t_pass.txt=\"" + yildizlar + "\""); 
    }
  }
  else if (komut == "KEY_DEL") {
    if (girilen_sifre.length() > 0) {
      girilen_sifre.remove(girilen_sifre.length() - 1); 
      String yildizlar = "";
      for(int i=0; i<girilen_sifre.length(); i++) yildizlar += "*";
      nextionGonder("t_pass.txt=\"" + yildizlar + "\"");
    }
  }
  else if (komut == "KEY_OK") {
    if (girilen_sifre == dogru_sifre) {
      girilen_sifre = ""; 
      nextionGonder("page page5"); 
      delay(50); 
      nextionGonder("t_guc.txt=\"" + String(guc_seviyesi) + "\"");
      nextionGonder("t_id.txt=\"" + String(kart_id) + "\"");
      nextionGonder("t_max.txt=\"" + String(max_goz_sayisi) + "\"");
    } else {
      girilen_sifre = "";
      nextionGonder("t_pass.txt=\"HATALI!\""); 
    }
  }
  else if (komut == "GUC_UP") {
    if (guc_seviyesi < 100) guc_seviyesi += 10;
    nextionGonder("t_guc.txt=\"" + String(guc_seviyesi) + "\"");
  }
  else if (komut == "GUC_DOWN") {
    if (guc_seviyesi > 10) guc_seviyesi -= 10;
    nextionGonder("t_guc.txt=\"" + String(guc_seviyesi) + "\"");
  }
  else if (komut == "ID_UP") {
    kart_id++;
    nextionGonder("t_id.txt=\"" + String(kart_id) + "\"");
  }
  else if (komut == "ID_DOWN") {
    if (kart_id > 1) kart_id--;
    nextionGonder("t_id.txt=\"" + String(kart_id) + "\"");
  }
  else if (komut == "MAX_UP") {
    if (max_goz_sayisi < MAX_GOZ - 1) max_goz_sayisi++; // dizi sinirini (1..10) asmayi engeller
    nextionGonder("t_max.txt=\"" + String(max_goz_sayisi) + "\"");
  }
  else if (komut == "MAX_DOWN") {
    if (max_goz_sayisi > 1) max_goz_sayisi--;
    nextionGonder("t_max.txt=\"" + String(max_goz_sayisi) + "\"");
  }
  else if (komut == "SRV_SAVE") {
    nvsKaydet();
    stmSetIdBroadcast(kart_id); // fiziksel bagli STM32'yi yeni Kart ID'sine hemen gectir
    Serial.println("--> SERVİS AYARLARI KAYDEDİLDİ!");
    nextionGonder("b_save.bco=2016"); 
    delay(600);
    nextionGonder("b_save.bco=50712"); 
  }
}

// ==========================================
// 8. ANA DÖNGÜ VE CANLI YAYIN
// ==========================================
// Non-blocking, tek-satir UART okuyucu: 'tampon' cagirilar arasi kalici state'tir (global String),
// bu yuzden ayni loop() cikisinda birden fazla "\n" sonlu komut ust uste binmeden TEK TEK islenir.
// '\n' gelince satir tamamlanir (bos olabilir, cagiran taraf kontrol eder); '\r' ve Nextion'un
// 0xFF sonlandirici byte'lari satir icerigine hic girmez, ayrica sondaki bosluklar trim() ile silinir.
bool hatOku(Stream &kaynak, String &tampon, String &satir) {
  while (kaynak.available()) {
    char c = kaynak.read();
    if (c == '\n') {
      satir = tampon;
      satir.trim();
      tampon = "";
      return true;
    }
    if (c != 0xFF && c != '\r') tampon += c;
  }
  return false;
}

void loop() {

  // --- Nextion'dan gelen komutlar (satir satir; ayni tick'te birikmis birden fazla komut olabilir) ---
  String satirNextion;
  while (hatOku(Serial2, gelenMesaj, satirNextion)) {
    if (satirNextion.length() > 0) {
      Serial.println("[HMI->ESP] " + satirNextion);
      komutIsle(satirNextion);
    }
  }

  // HIL_TEST_MOD: PC (COM10, USB Debug) uzerinden gelen test komutlari (satir satir).
  // Bus adresli komutlar ("T<id>:...") aynen Serial1'e (STM32 bus) iletilir;
  // digerleri (P_HIZLI, CMD_START|.., CMD_STOP, ...) normal HMI komut seti gibi islenir.
  String satirUsb;
  while (hatOku(Serial, usbMesaj, satirUsb)) {
    if (satirUsb.length() > 0) {
      Serial.println("[PC->ESP] " + satirUsb);
      if (isBusKomut(satirUsb)) {
        Serial1.print(satirUsb + "\n");
        Serial.println("[PC->STM] " + satirUsb);
      } else {
        komutIsle(satirUsb);
      }
    }
  }

  // --- STM32'den gelen telemetri (STAT,<TankID>,...\n) - hatta N slave paylaşır (satir satir) ---
  String satirStm;
  while (hatOku(Serial1, stmMesaj, satirStm)) {
    if (satirStm.length() > 0) {
      Serial.println("[STM->ESP] " + satirStm);
      stmTelemetryIsle(satirStm); // ilgili Tank ID'nin bagli/reconnect durumunu kendi icinde yonetir
    }
  }

  // --- Bağlantı zaman aşımı kontrolü (her göz/tank bagimsiz) ---
  for (int i = 1; i < MAX_GOZ; i++) {
    if (stm_bagli[i] && (millis() - stm_son_veri_zamani[i] > STM_BAGLANTI_TIMEOUT)) {
      stm_bagli[i] = false;
    }
  }

  // HIL_DEEP_DEBUG: dumps the STM_BAGLANTI_TIMEOUT (3000ms) watchdog state for every tank
  // that has connected at least once, so the host can verify isKartBagli()'s own timeout math.
  if (millis() - hilWdtDebugZamani > STM_BAGLANTI_TIMEOUT) {
    hilWdtDebugZamani = millis();
    for (int i = 1; i < MAX_GOZ; i++) {
      if (stm_son_veri_zamani[i] == 0) continue; // never seen telemetry from this tank yet
      unsigned long age_ms = millis() - stm_son_veri_zamani[i];
      Serial.println("DEBUG_ESP32: WDT tank=" + String(i) + " connected=" + String(isKartBagli(i) ? 1 : 0) + " age_ms=" + String(age_ms));
    }
  }

  if (millis() - sonGuncellemeZamani > 1000) {
    sonGuncellemeZamani = millis();

    // Seçili gözün verilerini ekrana bas
    int kalan_dk = kalan_saniye[secili_goz] / 60;
    int kalan_sn = kalan_saniye[secili_goz] % 60;
    char zamanBuf[6];
    snprintf(zamanBuf, sizeof(zamanBuf), "%02d:%02d", kalan_dk, kalan_sn);

    // PT100 acik/kisa devre hatasinda STM32 sicakligi 0'a zorlar; ekranda "--.-" goster
    bool pt100_hata = (stm_fault[secili_goz] & (FAULT_PT100_OPEN_BIT | FAULT_PT100_SHORT_BIT)) != 0;
    String sicaklikMetni = pt100_hata ? "--.-" : String(anlik_sicaklik[secili_goz], 1);

    nextionGonder(String("t_kalan_sure.txt=\"") + zamanBuf + "\"");
    nextionGonder("t_anlik_sic.txt=\"" + sicaklikMetni + "\"");
    nextionGonder("t_durum.txt=\"" + durum_metni[secili_goz] + "\"");
    nextionGonder("b_goz.txt=\"Goz: " + String(secili_goz) + "\""); 
  }
}