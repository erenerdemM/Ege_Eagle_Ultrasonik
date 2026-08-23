#include <Arduino.h>
#include <Preferences.h>
#include "esp_timer.h"

// --- NEXTION (HMI) UART ---
#define RXD2 16
#define TXD2 17

// --- NEXTION COLOR CONSTANTS ---
#define NEXTION_COLOR_RED     63488
#define NEXTION_COLOR_GREEN   2016
#define NEXTION_COLOR_DEFAULT 50712

// --- TEST-ONLY NEXTION SIMULATOR OUTPUT MIRROR ---
// When 1, nextionGonder() mirrors every raw Nextion frame (bytes + 3x 0xFF) to USB CDC (Serial) for simulator loop testing.
// When 0, production behavior is 100% standard (Serial2 only).
#ifndef NEXTION_SIM_MIRROR
#define NEXTION_SIM_MIRROR 1
#endif

#if NEXTION_SIM_MIRROR
#define DEBUG_PRINTLN(x) do {} while(0)
#define DEBUG_PRINT(x)   do {} while(0)
#else
#define DEBUG_PRINTLN(x) DEBUG_PRINTLN(x)
#define DEBUG_PRINT(x)   DEBUG_PRINT(x)
#endif

// --- BUS DIAGNOSTICS ARCHITECTURE ---
struct BusDiagnostics {
  uint32_t rx_valid_count = 0;
  uint32_t rx_crc_error_count = 0;
  uint32_t rx_malformed_count = 0;
  uint32_t rx_timeout_count = 0;
  uint32_t rx_dropped_count = 0;
  uint32_t tx_frame_count = 0;
  uint32_t tx_ack_count = 0;
  uint32_t tx_nack_count = 0;
} g_bus_diag;

// --- STM32 (SLAVE) UART ---
#define STM_RXD 18
#define STM_TXD 8
#define RS485_DE_PIN 5
#define STM_BAUD 115200
#define STM_BAGLANTI_TIMEOUT 3000

void rs485Transmit(const String &msg) {
  digitalWrite(RS485_DE_PIN, HIGH);
  delayMicroseconds(50);
  Serial1.print(msg);
  Serial1.flush();
  delayMicroseconds(500); // Ensure last byte is fully shifted out before disabling DE
  digitalWrite(RS485_DE_PIN, LOW);
  delay(12); // Inter-frame guard for half-duplex RS485 ACK turnaround
}

// STM32 fault_flags bitleri
#define FAULT_PT100_OPEN_BIT   0x01
#define FAULT_PT100_SHORT_BIT  0x02

// --- ZERO-CROSS SIMULATOR ---
#define ZC_SIM_PIN 4
#define ZC_SIM_FREQ_HZ 100
#define ZC_SIM_HALF_PERIOD_US (1000000ULL / ZC_SIM_FREQ_HZ / 2)

// ==========================================
// 1. SİSTEM DEĞİŞKENLERİ & PER-TANK YAPILAR
// ==========================================
#define MAX_GOZ 11 // index 0 unused; valid Tank ID range: 1..10

int secili_goz = 1;      
int temp_goz = 1;
int aktif_sayfa = 0;          // Tracks current active HMI page (0..7)
int current_service_page = 5; // Tracks active service page (5, 6, 7)

// --- P1, P2, P3 GLOBAL ŞABLONLARI ---
int aktif_program = 0;       // Page 0'da o an seçilen program (0 = None/Manual)
int duzenlenen_program = 1;  // Page 2'de o an ayarı değiştirilen program (Varsayılan P1)

int p_sure[4] = {0, 15, 20, 25};               
int p_sicaklik[4] = {0, 40, 50, 60};
int p_sweep[4] = {0, 0, 0, 0}; // Stored sweep_enabled state (0=OFF, 1=ON)           

String girilen_sifre = "";
String dogru_sifre = "123456";
bool g_service_authenticated = false;
unsigned long service_auth_time = 0;
const unsigned long SERVICE_SESSION_TIMEOUT_MS = 300000; // 5 minute auth timeout

// Global settings
int max_goz_sayisi = 3;

// ==========================================
// 2. HER GÖZÜN BAĞIMSIZ BEYNİ (ARRAY/DİZİLER)
// ==========================================
// Tank-scoped service settings
int guc_seviyesi[MAX_GOZ]; // Power % per tank (10..100%)
int kart_id[MAX_GOZ];      // Card ID per tank (1..255)

// Tank-scoped Sweep configuration
struct ESP32SweepConfig {
  uint8_t  enabled = 0;        // 0=OFF, 1=ON (in service menu)
  uint8_t  span_khz = 2;       // 1..4 kHz
  uint16_t period_ms = 400;    // 100..1000 ms
  uint8_t  step_increment = 4; // 1..8
} service_sweep[MAX_GOZ];

// Tank-scoped DEGAS configuration
struct ESP32DegasConfig {
  uint16_t duration_minutes = 15;
  uint8_t  power_pct = 100;
  uint8_t  frequency_khz = 28;
  uint16_t pulse_on_ms = 1000;
  uint16_t pulse_off_ms = 500;
  uint8_t  temp_ctrl = 0;
  float    target_temp_c = 50.0;
} service_degas[MAX_GOZ];

Preferences prefs;
Preferences degasPrefs;
Preferences provPrefs;
Preferences walPrefs;

// Runtime state per tank
bool makine_calisiyor[MAX_GOZ];
bool degas_armed[MAX_GOZ];
bool degas_active[MAX_GOZ];
bool runtime_sweep[MAX_GOZ];   // Active runtime sweep state
String durum_metni[MAX_GOZ];
int hedef_sure[MAX_GOZ];      
int hedef_sicaklik[MAX_GOZ];  
int kalan_saniye[MAX_GOZ];     // STM32'den gelen gerçek kalan süre (sn)
float anlik_sicaklik[MAX_GOZ]; // STM32'den gelen gerçek sıcaklık (temp_x10/10.0)

// STM32 telemetry per tank
int stm_fault[MAX_GOZ];
int stm_relay[MAX_GOZ];
int stm_pwr[MAX_GOZ];
int stm_freq[MAX_GOZ];
int stm_prov_state[MAX_GOZ];
bool stm_bagli[MAX_GOZ];
unsigned long stm_son_veri_zamani[MAX_GOZ];

unsigned long sonGuncellemeZamani = 0;
unsigned long sonSaniyeZamani = 0;
unsigned long hilWdtDebugZamani = 0;
unsigned long sonHeartbeatZamani = 0;
bool hil_heartbeat_active = true;

String gelenMesaj = "";
String stmMesaj = "";
String usbMesaj = "";

uint8_t nextion_ff_count = 0;
uint8_t usb_ff_count = 0;
uint8_t stm_ff_count = 0;

// Forward declarations
void nextionGonder(String komut);
bool hatOku(Stream &kaynak, String &tampon, uint8_t &ff_count, String &satir);
bool hatOku(Stream &kaynak, String &tampon, String &satir);
void stmSweep(bool enabled);
void updatePage0UI();
void updatePage1UI(int g);
void updatePage2UI(int prog);
void updatePage3UI();
void updatePage4UI();
void updatePage5UI(int g);
void updatePage6UI(int g);
void updatePage7UI(int g);
void updatePage8UI(int g);

// ==========================================
// 3. INTERLOCK & SAFETY EVALUATORS
// ==========================================
bool isAnyTankRunning() {
  for (int i = 1; i < MAX_GOZ; i++) {
    if (stm_bagli[i] && (makine_calisiyor[i] || degas_active[i] || stm_relay[i] != 0)) {
      return true;
    }
  }
  return false;
}

bool isProvisioningAllowed() {
  if (!g_service_authenticated) {
    DEBUG_PRINTLN("--> HATA: SERVIS YETKILENDIRMESI GEREKLI (g_service_authenticated == false)");
    return false;
  }
  if ((millis() - service_auth_time) > SERVICE_SESSION_TIMEOUT_MS) {
    g_service_authenticated = false;
    DEBUG_PRINTLN("--> HATA: SERVIS OTURUM SURESI DOLDU");
    return false;
  }
  if (isAnyTankRunning()) {
    DEBUG_PRINTLN("--> HATA: CALISAN TANK VAR! PROVISIONING KILITLI (SYS_MODE_RUNNING INTERLOCK)");
    return false;
  }
  return true;
}

bool isKartBagli(uint8_t goz_id) {
  if (goz_id == 0 || goz_id >= MAX_GOZ) return false;
  return (stm_bagli[goz_id] && ((millis() - stm_son_veri_zamani[goz_id]) < STM_BAGLANTI_TIMEOUT));
}

bool baslatmaEngelliMi() {
  if (isKartBagli(secili_goz)) return false;

  DEBUG_PRINTLN("--> HATA: GÖZ " + String(secili_goz) + " KART BAGLI DEGIL! ISLEM BASLATILAMIYOR.");
  durum_metni[secili_goz] = "Kart Yok!";
  nextionGonder("t_durum.txt=\"Kart Yok!\"");
  return true;
}

// --- ZERO-CROSS SIMULATOR ---
static esp_timer_handle_t zcSimTimer = nullptr;
volatile bool zcSimPinState = false;

static void zcSimTimerCallback(void* arg) {
  zcSimPinState = !zcSimPinState;
  digitalWrite(ZC_SIM_PIN, zcSimPinState ? HIGH : LOW);
}

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
// 4. NVS (KALICI HAFIZA) FONKSİYONLARI
// ==========================================
void nvsYukle() {
  prefs.begin("ultra", false);
  for (int i = 1; i <= 3; i++) {
    p_sure[i]     = prefs.getInt(("pS" + String(i)).c_str(), p_sure[i]);
    p_sicaklik[i] = prefs.getInt(("pT" + String(i)).c_str(), p_sicaklik[i]);
    p_sweep[i]    = prefs.getInt(("pSw" + String(i)).c_str(), p_sweep[i]);
  }
  max_goz_sayisi = prefs.getInt("maxgoz", max_goz_sayisi);

  // Per-tank settings loading
  for (int g = 1; g < MAX_GOZ; g++) {
    guc_seviyesi[g] = prefs.getInt(("guc_" + String(g)).c_str(), 50);
    kart_id[g]      = prefs.getInt(("kid_" + String(g)).c_str(), g);
    service_sweep[g].enabled        = prefs.getInt(("sw_en_" + String(g)).c_str(), 0);
    service_sweep[g].span_khz       = prefs.getInt(("sw_sp_" + String(g)).c_str(), 2);
    service_sweep[g].period_ms      = prefs.getInt(("sw_pr_" + String(g)).c_str(), 400);
    service_sweep[g].step_increment = prefs.getInt(("sw_st_" + String(g)).c_str(), 4);

    // Validation bounds
    if (guc_seviyesi[g] < 10 || guc_seviyesi[g] > 100) guc_seviyesi[g] = 50;
    if (kart_id[g] < 1 || kart_id[g] > 255) kart_id[g] = g;
    if (service_sweep[g].span_khz < 1 || service_sweep[g].span_khz > 4) service_sweep[g].span_khz = 2;
    if (service_sweep[g].period_ms < 100 || service_sweep[g].period_ms > 1000) service_sweep[g].period_ms = 400;
    if (service_sweep[g].step_increment < 1 || service_sweep[g].step_increment > 8) service_sweep[g].step_increment = 4;
  }
  prefs.end();

  // Debug dump
  for (int i = 1; i <= 3; i++) {
    DEBUG_PRINTLN("DEBUG_ESP32: NVS_READ key=pS" + String(i) + " val=" + String(p_sure[i]));
    DEBUG_PRINTLN("DEBUG_ESP32: NVS_READ key=pT" + String(i) + " val=" + String(p_sicaklik[i]));
    DEBUG_PRINTLN("DEBUG_ESP32: NVS_READ key=pSw" + String(i) + " val=" + String(p_sweep[i]));
  }
  DEBUG_PRINTLN("DEBUG_ESP32: NVS_READ key=maxgoz val=" + String(max_goz_sayisi));
}

void nvsKaydet() {
  prefs.begin("ultra", false);
  for (int i = 1; i <= 3; i++) {
    prefs.putInt(("pS" + String(i)).c_str(), p_sure[i]);
    prefs.putInt(("pT" + String(i)).c_str(), p_sicaklik[i]);
    prefs.putInt(("pSw" + String(i)).c_str(), p_sweep[i]);
  }
  prefs.putInt("maxgoz", max_goz_sayisi);
  for (int g = 1; g < MAX_GOZ; g++) {
    prefs.putInt(("guc_" + String(g)).c_str(), guc_seviyesi[g]);
    prefs.putInt(("kid_" + String(g)).c_str(), kart_id[g]);
    prefs.putInt(("sw_en_" + String(g)).c_str(), service_sweep[g].enabled);
    prefs.putInt(("sw_sp_" + String(g)).c_str(), service_sweep[g].span_khz);
    prefs.putInt(("sw_pr_" + String(g)).c_str(), service_sweep[g].period_ms);
    prefs.putInt(("sw_st_" + String(g)).c_str(), service_sweep[g].step_increment);
  }
  prefs.end();
}

void nvsKaydetGoz(int g) {
  if (g < 1 || g >= MAX_GOZ) return;
  prefs.begin("ultra", false);
  prefs.putInt(("guc_" + String(g)).c_str(), guc_seviyesi[g]);
  prefs.putInt(("kid_" + String(g)).c_str(), kart_id[g]);
  prefs.putInt("maxgoz", max_goz_sayisi);
  prefs.end();
  DEBUG_PRINTLN("DEBUG_ESP32: NVS_SAVE_TANK g=" + String(g) + " guc=" + String(guc_seviyesi[g]) + " id=" + String(kart_id[g]));
}

void sweepNvsKaydet(int g) {
  if (g < 1 || g >= MAX_GOZ) return;
  prefs.begin("ultra", false);
  prefs.putInt(("sw_en_" + String(g)).c_str(), service_sweep[g].enabled);
  prefs.putInt(("sw_sp_" + String(g)).c_str(), service_sweep[g].span_khz);
  prefs.putInt(("sw_pr_" + String(g)).c_str(), service_sweep[g].period_ms);
  prefs.putInt(("sw_st_" + String(g)).c_str(), service_sweep[g].step_increment);
  prefs.end();
  DEBUG_PRINTLN("DEBUG_ESP32: SWEEP_NVS_SAVE g=" + String(g) + " en=" + String(service_sweep[g].enabled) + " span=" + String(service_sweep[g].span_khz));
}

void degasNvsYukle() {
  degasPrefs.begin("degas_cfg", false);
  for (int g = 1; g < MAX_GOZ; g++) {
    String pfx = String("d") + String(g) + "_";
    service_degas[g].duration_minutes = degasPrefs.getUShort((pfx + "dur").c_str(), 15);
    service_degas[g].power_pct        = degasPrefs.getUChar((pfx + "pwr").c_str(), 100);
    service_degas[g].frequency_khz    = degasPrefs.getUChar((pfx + "frq").c_str(), 28);
    service_degas[g].pulse_on_ms      = degasPrefs.getUShort((pfx + "on").c_str(), 1000);
    service_degas[g].pulse_off_ms     = degasPrefs.getUShort((pfx + "off").c_str(), 500);
    service_degas[g].temp_ctrl        = degasPrefs.getUChar((pfx + "tc").c_str(), 0);
    service_degas[g].target_temp_c    = degasPrefs.getFloat((pfx + "tgt").c_str(), 50.0f);

    /* Software boundary validation on load */
    if (service_degas[g].duration_minutes < 1 || service_degas[g].duration_minutes > 120) service_degas[g].duration_minutes = 15;
    if (service_degas[g].power_pct < 10 || service_degas[g].power_pct > 100) service_degas[g].power_pct = 100;
    if (service_degas[g].frequency_khz < 28 || service_degas[g].frequency_khz > 40) service_degas[g].frequency_khz = 28;
    if (service_degas[g].pulse_on_ms < 100 || service_degas[g].pulse_on_ms > 10000) service_degas[g].pulse_on_ms = 1000;
    if (service_degas[g].pulse_off_ms > 10000) service_degas[g].pulse_off_ms = 500;
    if (service_degas[g].temp_ctrl > 1) service_degas[g].temp_ctrl = 0;
    if (service_degas[g].target_temp_c < 20.0f || service_degas[g].target_temp_c > 90.0f) service_degas[g].target_temp_c = 50.0f;
  }
  degasPrefs.end();
}

void degasNvsKaydet(int g) {
  if (g < 1 || g >= MAX_GOZ) return;
  degasPrefs.begin("degas_cfg", false);
  String pfx = String("d") + String(g) + "_";
  degasPrefs.putUShort((pfx + "dur").c_str(), service_degas[g].duration_minutes);
  degasPrefs.putUChar((pfx + "pwr").c_str(), service_degas[g].power_pct);
  degasPrefs.putUChar((pfx + "frq").c_str(), service_degas[g].frequency_khz);
  degasPrefs.putUShort((pfx + "on").c_str(), service_degas[g].pulse_on_ms);
  degasPrefs.putUShort((pfx + "off").c_str(), service_degas[g].pulse_off_ms);
  degasPrefs.putUChar((pfx + "tc").c_str(), service_degas[g].temp_ctrl);
  degasPrefs.putFloat((pfx + "tgt").c_str(), service_degas[g].target_temp_c);
  degasPrefs.end();
}

// ==========================================
// 5. NEXTION UI UPDATE HELPERS
// ==========================================
void nextionGonder(String komut) {
  Serial2.print(komut);
  Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
#if NEXTION_SIM_MIRROR
  Serial.print(komut);
  Serial.write(0xFF); Serial.write(0xFF); Serial.write(0xFF);
#endif
}

// Page 2 Temporary recipe edit state (committed to NVS only on P_SAVE)
int edit_p_sure[4] = {0, 15, 20, 30};
int edit_p_sicaklik[4] = {0, 40, 50, 60};
int edit_p_sweep[4] = {0, 0, 1, 0};

void initRecipeEditBuffers() {
  for (int i = 1; i <= 3; i++) {
    edit_p_sure[i] = p_sure[i];
    edit_p_sicaklik[i] = p_sicaklik[i];
    edit_p_sweep[i] = p_sweep[i];
  }
}

void discardRecipeEditBuffers(int prog = 0) {
  if (prog >= 1 && prog <= 3) {
    edit_p_sure[prog] = p_sure[prog];
    edit_p_sicaklik[prog] = p_sicaklik[prog];
    edit_p_sweep[prog] = p_sweep[prog];
  } else {
    initRecipeEditBuffers();
  }
}

// Page 5 Temporary service edit state (committed to NVS only on PAGE5_SAVE / SRV_SAVE)
int edit_guc_seviyesi[MAX_GOZ];
int edit_kart_id[MAX_GOZ];
int edit_max_goz_sayisi = 3;

// Page 6 Temporary sweep service edit state (committed to NVS only on PAGE6_SAVE / SWP_SAVE)
ESP32SweepConfig edit_service_sweep[MAX_GOZ];

// Page 7 Temporary degas service edit state (committed to NVS only on PAGE7_SAVE / SRV_DEGAS_SAVE)
ESP32DegasConfig edit_service_degas[MAX_GOZ];

void initServiceEditBuffers() {
  edit_max_goz_sayisi = max_goz_sayisi;
  for (int g = 1; g < MAX_GOZ; g++) {
    edit_guc_seviyesi[g] = guc_seviyesi[g];
    edit_kart_id[g]      = kart_id[g];
    edit_service_sweep[g]= service_sweep[g];
    edit_service_degas[g]= service_degas[g];
  }
}

void discardServiceEditBuffers(int g = 0) {
  if (g >= 1 && g < MAX_GOZ) {
    edit_guc_seviyesi[g] = guc_seviyesi[g];
    edit_kart_id[g]      = kart_id[g];
    edit_service_sweep[g]= service_sweep[g];
    edit_service_degas[g]= service_degas[g];
  } else {
    initServiceEditBuffers();
  }
}

void updatePage0ButtonColors() {
  uint16_t deg_color = (degas_armed[secili_goz] || degas_active[secili_goz]) ? NEXTION_COLOR_GREEN : NEXTION_COLOR_DEFAULT;
  nextionGonder("b_degas.bco=" + String(deg_color));
  nextionGonder("b_deg.bco=" + String(deg_color));

  uint16_t swe_color = runtime_sweep[secili_goz] ? NEXTION_COLOR_GREEN : NEXTION_COLOR_DEFAULT;
  nextionGonder("b_sweep.bco=" + String(swe_color));
  nextionGonder("b_swe.bco=" + String(swe_color));

  nextionGonder("b_prog_p1.bco=" + String((aktif_program == 1) ? NEXTION_COLOR_GREEN : NEXTION_COLOR_DEFAULT));
  nextionGonder("b_p1.bco=" + String((aktif_program == 1) ? NEXTION_COLOR_GREEN : NEXTION_COLOR_DEFAULT));

  nextionGonder("b_prog_p2.bco=" + String((aktif_program == 2) ? NEXTION_COLOR_GREEN : NEXTION_COLOR_DEFAULT));
  nextionGonder("b_p2.bco=" + String((aktif_program == 2) ? NEXTION_COLOR_GREEN : NEXTION_COLOR_DEFAULT));

  nextionGonder("b_prog_p3.bco=" + String((aktif_program == 3) ? NEXTION_COLOR_GREEN : NEXTION_COLOR_DEFAULT));
  nextionGonder("b_p3.bco=" + String((aktif_program == 3) ? NEXTION_COLOR_GREEN : NEXTION_COLOR_DEFAULT));

  nextionGonder("b_prog_fp.bco=" + String((aktif_program == 4) ? NEXTION_COLOR_GREEN : NEXTION_COLOR_DEFAULT));
  nextionGonder("b_fp.bco=" + String((aktif_program == 4) ? NEXTION_COLOR_GREEN : NEXTION_COLOR_DEFAULT));
}

void updatePage0UI() {
  nextionGonder("b_goz_sec.txt=\"Goz: " + String(secili_goz) + "\"");
  nextionGonder("b_goz.txt=\"Goz: " + String(secili_goz) + "\"");
  nextionGonder(String("t_set_sure.txt=\"") + (hedef_sure[secili_goz] < 10 ? "0" : "") + String(hedef_sure[secili_goz]) + "\"");
  nextionGonder("t_set_sic.txt=\"" + String(hedef_sicaklik[secili_goz]) + "\"");

  int kalan_dk = kalan_saniye[secili_goz] / 60;
  int kalan_sn = kalan_saniye[secili_goz] % 60;
  char zamanBuf[6];
  snprintf(zamanBuf, sizeof(zamanBuf), "%02d:%02d", kalan_dk, kalan_sn);

  bool pt100_hata = (stm_fault[secili_goz] & (FAULT_PT100_OPEN_BIT | FAULT_PT100_SHORT_BIT)) != 0;
  String sicaklikMetni = pt100_hata ? "--.-" : String(anlik_sicaklik[secili_goz], 1);

  nextionGonder(String("t_kalan.txt=\"") + zamanBuf + "\"");
  nextionGonder(String("t_kalan_sure.txt=\"") + zamanBuf + "\"");
  nextionGonder("t_anlik_sic.txt=\"" + sicaklikMetni + "\"");
  nextionGonder("t_durum.txt=\"" + durum_metni[secili_goz] + "\"");
  String freqMetni = (stm_freq[secili_goz] == 40) ? "40k" : "28k";
  nextionGonder("b_freq.txt=\"" + freqMetni + "\"");
  nextionGonder("b_frq.txt=\"" + freqMetni + "\"");
  updatePage0ButtonColors();
}

void updatePage1UI(int g) {
  if (g < 1 || g >= MAX_GOZ) g = secili_goz;
  temp_goz = g;
  nextionGonder("t_secili_goz.txt=\"" + String(temp_goz) + "\"");
  nextionGonder("t0.txt=\"" + String(temp_goz) + "\"");
  nextionGonder("t_goz.txt=\"" + String(temp_goz) + "\"");
}

void updatePage2UI(int prog) {
  if (prog < 1 || prog > 3) prog = 1;
  nextionGonder("t_prog_baslik.txt=\"PROGRAM P" + String(prog) + "\"");
  nextionGonder("t0.txt=\"PROGRAM P" + String(prog) + "\"");
  nextionGonder(String("t_set_sure.txt=\"") + (edit_p_sure[prog] < 10 ? "0" : "") + String(edit_p_sure[prog]) + "\"");
  nextionGonder("t_set_sic.txt=\"" + String(edit_p_sicaklik[prog]) + "\"");

  uint16_t swe_color = (edit_p_sweep[prog] != 0) ? NEXTION_COLOR_GREEN : NEXTION_COLOR_DEFAULT;
  nextionGonder("b_edit_sweep.bco=" + String(swe_color));
  nextionGonder("b_swe.bco=" + String(swe_color));

  nextionGonder("b_edit_p1.bco=" + String((prog == 1) ? NEXTION_COLOR_GREEN : NEXTION_COLOR_DEFAULT));
  nextionGonder("b_p1.bco=" + String((prog == 1) ? NEXTION_COLOR_GREEN : NEXTION_COLOR_DEFAULT));

  nextionGonder("b_edit_p2.bco=" + String((prog == 2) ? NEXTION_COLOR_GREEN : NEXTION_COLOR_DEFAULT));
  nextionGonder("b_p2.bco=" + String((prog == 2) ? NEXTION_COLOR_GREEN : NEXTION_COLOR_DEFAULT));

  nextionGonder("b_edit_p3.bco=" + String((prog == 3) ? NEXTION_COLOR_GREEN : NEXTION_COLOR_DEFAULT));
  nextionGonder("b_p3.bco=" + String((prog == 3) ? NEXTION_COLOR_GREEN : NEXTION_COLOR_DEFAULT));
}

void updatePage3UI() {
  nextionGonder("t0.txt=\"AYARLAR\"");
}

void updatePage4UI() {
  girilen_sifre = "";
  nextionGonder("t_sifre.txt=\"\"");
  nextionGonder("t_pass.txt=\"\"");
  nextionGonder("t0.txt=\"SIFRE GIRIN\"");
}

void updatePage5UI(int g) {
  if (g < 1 || g >= MAX_GOZ) return;
  int committed_goz = (kart_id[g] >= 1) ? kart_id[g] : g;
  int draft_id = (edit_kart_id[g] >= 1) ? edit_kart_id[g] : g;
  nextionGonder("t_srv_goz.txt=\"" + String(committed_goz) + "\"");
  nextionGonder("t_srv_guc.txt=\"" + String(edit_guc_seviyesi[g]) + "\"");
  nextionGonder("t_srv_id.txt=\"" + String(draft_id) + "\"");
  nextionGonder("t_srv_max.txt=\"" + String(edit_max_goz_sayisi) + "\"");
  // Backward compatibility
  nextionGonder("t0.txt=\"SERVIS AYARLARI - GOZ " + String(committed_goz) + "\"");
  nextionGonder("t_goz_num.txt=\"" + String(g) + "\"");
  nextionGonder("t_guc.txt=\"" + String(edit_guc_seviyesi[g]) + "\"");
  nextionGonder("t_id.txt=\"" + String(draft_id) + "\"");
  nextionGonder("t_max.txt=\"" + String(edit_max_goz_sayisi) + "\"");
}

void updatePage6UI(int g) {
  if (g < 1 || g >= MAX_GOZ) return;
  int committed_goz = (kart_id[g] >= 1) ? kart_id[g] : g;
  nextionGonder("t_swp_goz.txt=\"" + String(committed_goz) + "\"");
  nextionGonder("t_swp_span.txt=\"" + String(edit_service_sweep[g].span_khz) + "\"");
  nextionGonder("t_swp_per.txt=\"" + String(edit_service_sweep[g].period_ms) + "\"");
  nextionGonder("t_swp_step.txt=\"" + String(edit_service_sweep[g].step_increment) + "\"");
  // Backward compatibility
  nextionGonder("t_goz_num.txt=\"" + String(g) + "\"");
  nextionGonder("t_swp_period.txt=\"" + String(edit_service_sweep[g].period_ms) + "\"");
}

void updatePage7UI(int g) {
  if (g < 1 || g >= MAX_GOZ) return;
  int committed_goz = (kart_id[g] >= 1) ? kart_id[g] : g;
  nextionGonder("t_deg_goz.txt=\"" + String(committed_goz) + "\"");
  nextionGonder("t_deg_dur.txt=\"" + String(edit_service_degas[g].duration_minutes) + "\"");
  nextionGonder("t_deg_pow.txt=\"" + String(edit_service_degas[g].power_pct) + "\"");
  nextionGonder("t_deg_freq.txt=\"" + String(edit_service_degas[g].frequency_khz) + "\"");
  // Backward compatibility
  nextionGonder("t_goz_num.txt=\"" + String(g) + "\"");
  nextionGonder("t_deg_pwr.txt=\"" + String(edit_service_degas[g].power_pct) + "\"");
  nextionGonder("t_deg_frq.txt=\"" + String(edit_service_degas[g].frequency_khz) + "\"");
}

void updatePage8UI(int g) {
  if (g < 1 || g >= MAX_GOZ) return;
  nextionGonder("t_deg_pon.txt=\"" + String(edit_service_degas[g].pulse_on_ms) + "\"");
  nextionGonder("t_deg_poff.txt=\"" + String(edit_service_degas[g].pulse_off_ms) + "\"");
  String tc_str = (edit_service_degas[g].temp_ctrl != 0) ? "ACIK" : "KAPALI";
  nextionGonder("t_deg_tctrl.txt=\"" + tc_str + "\"");
  if (edit_service_degas[g].temp_ctrl != 0) {
    nextionGonder("t_deg_temp.txt=\"" + String((int)edit_service_degas[g].target_temp_c) + "\"");
    nextionGonder("t_deg_tgt.txt=\"" + String((int)edit_service_degas[g].target_temp_c) + "\"");
  } else {
    nextionGonder("t_deg_temp.txt=\"--\"");
    nextionGonder("t_deg_tgt.txt=\"--\"");
  }
  // Backward compatibility
  nextionGonder("t_deg_on.txt=\"" + String(edit_service_degas[g].pulse_on_ms) + "\"");
  nextionGonder("t_deg_off.txt=\"" + String(edit_service_degas[g].pulse_off_ms) + "\"");
  nextionGonder("t_deg_tc.txt=\"" + String(edit_service_degas[g].temp_ctrl != 0 ? "ON" : "OFF") + "\"");
}

// Backward compatibility helper
void updateDegasPageUI(int g) {
  updatePage7UI(g);
}

void disarmDegasIfArmed(int g) {
  if (g >= 1 && g < MAX_GOZ) {
    if (degas_armed[g] && !degas_active[g]) {
      degas_armed[g] = false;
      if (g == secili_goz) {
        nextionGonder("b_deg.bco=" + String(NEXTION_COLOR_DEFAULT));
      }
      DEBUG_PRINTLN("--> ESP32: DEGAS selection intent disarmed on parameter edit.");
    }
  }
}

// ==========================================
// 6. PROVISIONING & WAL REGISTRY
// ==========================================
enum ESP32_ProvState {
  PROV_STATE_UNCOMMISSIONED = 0x00,
  PROV_STATE_STAGING        = 0x01,
  PROV_STATE_ACTIVE         = 0x02
};

enum WAL_Step {
  WAL_STEP_NONE            = 0,
  WAL_STEP_STAGING_PENDING = 1,
  WAL_STEP_COMMIT_PENDING  = 2
};

String getProvNvsKey(String uid24, const char* suffix) {
  String s = String(suffix);
  int maxUidLen = 15 - s.length();
  if ((int)uid24.length() > maxUidLen) {
    return uid24.substring(uid24.length() - maxUidLen) + s;
  }
  return uid24 + s;
}

void provNvsKaydet(String uid24, int tankId, int state) {
  provPrefs.begin("eagle_prov", false);
  provPrefs.putInt(getProvNvsKey(uid24, "_id").c_str(), tankId);
  provPrefs.putInt(getProvNvsKey(uid24, "_st").c_str(), state);
  provPrefs.end();
  DEBUG_PRINTLN("DEBUG_ESP32: PROV_REGISTRY_SAVE UID=" + uid24 + " TankID=" + String(tankId) + " State=" + String(state));
}

bool provNvsOku(String uid24, int &tankId, int &state) {
  provPrefs.begin("eagle_prov", true);
  String keyId = getProvNvsKey(uid24, "_id");
  String keySt = getProvNvsKey(uid24, "_st");
  if (!provPrefs.isKey(keyId.c_str())) {
    provPrefs.end();
    tankId = 0;
    state = PROV_STATE_UNCOMMISSIONED;
    return false;
  }
  tankId = provPrefs.getInt(keyId.c_str(), 0);
  state = provPrefs.getInt(keySt.c_str(), PROV_STATE_UNCOMMISSIONED);
  provPrefs.end();
  return true;
}

void provNvsSil(String uid24) {
  provPrefs.begin("eagle_prov", false);
  provPrefs.remove(getProvNvsKey(uid24, "_id").c_str());
  provPrefs.remove(getProvNvsKey(uid24, "_st").c_str());
  provPrefs.end();
  DEBUG_PRINTLN("DEBUG_ESP32: PROV_REGISTRY_DELETE UID=" + uid24);
}

void walYaz(int step, String uid24, int proposedId) {
  walPrefs.begin("eagle_prov_wal", false);
  walPrefs.putInt("step", step);
  walPrefs.putString("uid", uid24);
  walPrefs.putInt("id", proposedId);
  walPrefs.end();
  DEBUG_PRINTLN("DEBUG_ESP32: WAL_WRITE step=" + String(step) + " uid=" + uid24 + " proposedId=" + String(proposedId));
}

bool walOku(int &step, String &uid24, int &proposedId) {
  walPrefs.begin("eagle_prov_wal", true);
  step = walPrefs.getInt("step", WAL_STEP_NONE);
  uid24 = walPrefs.getString("uid", "");
  proposedId = walPrefs.getInt("id", 0);
  walPrefs.end();
  return (step != WAL_STEP_NONE);
}

void walTemizle() {
  walPrefs.begin("eagle_prov_wal", false);
  walPrefs.clear();
  walPrefs.end();
  DEBUG_PRINTLN("DEBUG_ESP32: WAL_CLEARED");
}

void walKurtar() {
  int step = 0;
  String uid = "";
  int proposedId = 0;

  if (walOku(step, uid, proposedId)) {
    DEBUG_PRINTLN("--> WAL RECOVERY DETECTED! Uncommitted transaction: step=" + String(step) + " uid=" + uid + " id=" + String(proposedId));
    if (step == WAL_STEP_STAGING_PENDING) {
      DEBUG_PRINTLN("--> WAL RECOVERY: Aborting unconfirmed staging transaction for UID=" + uid);
      rs485Transmit("T0:CANCEL_STAGE\n");
      walTemizle();
    } else if (step == WAL_STEP_COMMIT_PENDING) {
      DEBUG_PRINTLN("--> WAL RECOVERY: Retrying commit transaction for UID=" + uid + " ID=" + String(proposedId));
      rs485Transmit("T0:ASSIGN_ID:" + String(proposedId) + ":" + uid + "\n");
      provNvsKaydet(uid, proposedId, PROV_STATE_ACTIVE);
      walTemizle();
    }
  }
}

// ==========================================
// 7. STM32 UART TX (KOMUT GÖNDERME)
// ==========================================
void stmGonder(String komut) {
  komut.trim();
  int target_id = (secili_goz >= 1 && secili_goz < MAX_GOZ && kart_id[secili_goz] >= 1) ? kart_id[secili_goz] : secili_goz;
  String adresli = "T" + String(target_id) + ":" + komut + "\n";
  rs485Transmit(adresli);
  String log = adresli;
  log.trim();
  DEBUG_PRINTLN("[ESP->STM] " + log);
}

void stmSetTime(int dk) {
  stmGonder("SET_TIME:" + String(dk));
}

void stmSetTemp(int derece) {
  stmGonder("SET_TEMP:" + String(derece));
}

void stmSetPower(int yuzde) {
  stmGonder("SET_POWER:" + String(yuzde));
}

void stmSetFreq(int freq) {
  stmGonder("SET_FREQ:" + String(freq));
}

void stmSweep(bool enabled) {
  if (enabled) {
    stmGonder("SWEEP:ON");
  } else {
    stmGonder("SWEEP:OFF");
  }
}

void stmSetStepInc(int inc) {
  stmGonder("SET_STEP_INC:" + String(inc));
}

void stmSetSwpSpan(int span) {
  stmGonder("SET_SWP_SPAN:" + String(span));
}

void stmSetSwpPer(int per) {
  stmGonder("SET_SWP_PER:" + String(per));
}

void stmStart() {
  stmGonder("START");
}

void stmStop() {
  stmGonder("STOP");
}

void stmDegas() {
  stmGonder("DEGAS");
}

void stmSetIdBroadcast(int yeniId) {
  String adresli = "T0:SET_ID:" + String(yeniId) + "\n";
  rs485Transmit(adresli);
  String log = adresli;
  log.trim();
  DEBUG_PRINTLN("[ESP->STM] " + log);
}

void stmSetpointleriGonder() {
  stmSetTime(hedef_sure[secili_goz]);
  stmSetTemp(hedef_sicaklik[secili_goz]);
  stmSetPower(guc_seviyesi[secili_goz]);
  stmSetStepInc(service_sweep[secili_goz].step_increment);
  stmSetSwpSpan(service_sweep[secili_goz].span_khz);
  stmSetSwpPer(service_sweep[secili_goz].period_ms);
  stmSetFreq(stm_freq[secili_goz]);
  if (runtime_sweep[secili_goz]) {
    stmSweep(true);
  }
}

bool isBusKomut(const String &s) {
  if (s.length() < 3 || s.charAt(0) != 'T') return false;
  int colonIdx = s.indexOf(':');
  return (colonIdx > 1);
}

// ==========================================
// 8. STM32 TELEMETRİ İŞLEYİCİ
// ==========================================
void stmTelemetryIsle(String satir) {
  if (satir.startsWith("ERR:") || satir.startsWith("NACK")) {
    DEBUG_PRINTLN("--> STM32 REJECTION: " + satir);
    if (satir.startsWith("ERR:LOCKED_ACTIVE_MODE") || satir.startsWith("ERR:LOCKED_SYS_RUNNING")) {
      durum_metni[secili_goz] = "HATA: CALISIYOR!";
    } else if (satir.startsWith("ERR:SWEEP_PROHIBITED_IN_DEGAS")) {
      durum_metni[secili_goz] = "HATA: DEGAS AKTIF!";
    } else if (satir.startsWith("ERR:INVALID_SYS_MODE")) {
      durum_metni[secili_goz] = "HATA: GECERSIZ MOD!";
    } else if (satir.startsWith("NACK,ERR_FAULT_ACTIVE")) {
      durum_metni[secili_goz] = "HATA: ARIZA AKTIF!";
    } else {
      durum_metni[secili_goz] = "HATA: " + satir.substring(0, 16);
    }
    nextionGonder("t_durum.txt=\"" + durum_metni[secili_goz] + "\"");
    return;
  }

  if (satir.startsWith("ACK:") || satir.startsWith("ACK,") || satir.startsWith("DISCOVER_ACK,")) {
    DEBUG_PRINTLN("--> STM32 ACK: " + satir);
    if (satir.startsWith("ACK:FAULT_CLEARED") || satir.startsWith("ACK:NO_FAULT")) {
      if (durum_metni[secili_goz].startsWith("HATA")) {
        durum_metni[secili_goz] = "SISTEM BEKLEMEDE";
        nextionGonder("t_durum.txt=\"" + durum_metni[secili_goz] + "\"");
      }
    }
    return;
  }

  if (!satir.startsWith("STAT,")) return;

  String rest = satir.substring(5);
  int p1 = rest.indexOf(',');
  int p2 = rest.indexOf(',', p1 + 1);
  int p3 = rest.indexOf(',', p2 + 1);
  int p4 = rest.indexOf(',', p3 + 1);
  int p5 = rest.indexOf(',', p4 + 1);
  int p6 = rest.indexOf(',', p5 + 1);
  int p7 = rest.indexOf(',', p6 + 1);
  int p8 = rest.indexOf(',', p7 + 1);
  int p9 = rest.indexOf(',', p8 + 1);
  if (p1 == -1 || p2 == -1 || p3 == -1 || p4 == -1 || p5 == -1 || p6 == -1 || p7 == -1 || p8 == -1) return;

  int tank_id = rest.substring(0, p1).toInt();
  String mode_str = rest.substring(p1 + 1, p2);
  int rem_sec = rest.substring(p2 + 1, p3).toInt();
  int temp_x10 = rest.substring(p3 + 1, p4).toInt();
  int relay = rest.substring(p4 + 1, p5).toInt();
  int pwr = rest.substring(p5 + 1, p6).toInt();
  int freq = rest.substring(p6 + 1, p7).toInt();
  int fault = rest.substring(p7 + 1, p8).toInt();
  int prov_st = (p9 != -1) ? rest.substring(p8 + 1, p9).toInt() : rest.substring(p8 + 1).toInt();
  int swp_st  = (p9 != -1) ? rest.substring(p9 + 1).toInt() : 0;

  int g = 0;
  for (int i = 1; i < MAX_GOZ; i++) {
    if (kart_id[i] == tank_id) { g = i; break; }
  }
  if (g == 0) g = tank_id;

  if (g < 1 || g >= MAX_GOZ) return;

  anlik_sicaklik[g] = temp_x10 / 10.0;
  stm_relay[g] = relay;
  stm_pwr[g] = pwr;
  stm_freq[g] = freq;
  stm_fault[g] = fault;
  stm_prov_state[g] = prov_st;
  bool was_running = makine_calisiyor[g];
  bool was_degas = degas_active[g];

  bool yeniden_baglandi = !stm_bagli[g];
  stm_bagli[g] = true;
  stm_son_veri_zamani[g] = millis();

  if (fault > 0) {
    durum_metni[g] = "HATA! KOD:" + String(fault);
    makine_calisiyor[g] = false;
    degas_active[g] = false;
    degas_armed[g] = false;
    runtime_sweep[g] = false;
    if (g == secili_goz && aktif_sayfa == 0) {
      nextionGonder("b_deg.bco=" + String(NEXTION_COLOR_DEFAULT));
      updatePage0ButtonColors();
    }
  } else if (mode_str == "RUNNING") {
    makine_calisiyor[g] = true;
    degas_active[g] = false;
    durum_metni[g] = "YIKAMA DEVAM EDIYOR...";
    if (rem_sec > 0) kalan_saniye[g] = rem_sec;
  } else if (mode_str == "DEGAS") {
    degas_active[g] = true;
    makine_calisiyor[g] = false;
    durum_metni[g] = "DEGAS DEVAM EDIYOR...";
    if (rem_sec > 0) kalan_saniye[g] = rem_sec;
  } else if (mode_str == "IDLE") {
    if (was_running || was_degas) {
      if (kalan_saniye[g] <= 0) {
        makine_calisiyor[g] = false;
        degas_active[g] = false;
        degas_armed[g] = false;
        durum_metni[g] = "YIKAMA TAMAMLANDI!";
      }
    } else {
      if (durum_metni[g] != "YIKAMA TAMAMLANDI!" && !durum_metni[g].endsWith("START BEKLENIYOR")) {
        durum_metni[g] = "SISTEM BEKLEMEDE";
      }
    }
  }

  if (g == secili_goz && yeniden_baglandi) {
    stmSetpointleriGonder();
  }
}

// ==========================================
// 9. SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial1.begin(STM_BAUD, SERIAL_8N1, STM_RXD, STM_TXD);

  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, LOW);

  zcSimBaslat();

  for(int i=0; i<MAX_GOZ; i++) {
    makine_calisiyor[i] = false;
    degas_armed[i] = false;
    degas_active[i] = false;
    runtime_sweep[i] = false;
    durum_metni[i] = "SISTEM BEKLEMEDE";
    hedef_sure[i] = 0;
    kalan_saniye[i] = 0;
    hedef_sicaklik[i] = 0;
    anlik_sicaklik[i] = 24.0; 
    stm_fault[i] = 0;
    stm_relay[i] = 0;
    stm_pwr[i] = 0;
    stm_freq[i] = 28;
    stm_prov_state[i] = 2;
    stm_bagli[i] = false;
    stm_son_veri_zamani[i] = 0;
    guc_seviyesi[i] = 50;
    kart_id[i] = (i > 0) ? i : 1;
  }
  
  nvsYukle();
  degasNvsYukle();
  initServiceEditBuffers();
  initRecipeEditBuffers();
  walKurtar();

  DEBUG_PRINTLN("[SYS] Boot... NVS setpoints yuklendi:");
  for (int i = 1; i <= 3; i++) {
    DEBUG_PRINTLN("[SYS]   P" + String(i) + " = " + String(p_sure[i]) + "dk / " + String(p_sicaklik[i]) + "C / Sweep=" + String(p_sweep[i]));
  }
  DEBUG_PRINTLN("[SYS]   MaxGoz=" + String(max_goz_sayisi));

  stmSetpointleriGonder();
  DEBUG_PRINTLN("--- ULTRASONIK YIKAMA: REÇETE SISTEMI AKTIF ---");
}

// ==========================================
// 10. HMI KOMUT İŞLEYİCİ (komutIsle)
// ==========================================
void komutIsle(String komut) {
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

  DEBUG_PRINTLN("DEBUG_ESP32: HMI_RX raw=\"" + komut + "\"");

  // =========================================================================
  // PAGE 0 & GENERAL OPERATION COMMANDS
  // =========================================================================

  if (komut == "PAGE0_OPEN" || komut == "PAGE0") {
    aktif_sayfa = 0;
    nextionGonder("page page0");
    updatePage0UI();
    return;
  }

  // --- DEGAS TOGGLE (Page 0 b_deg / b_degas / CMD_DEGAS_ARM) ---
  if (komut == "CMD_DEGAS_SEL" || komut == "CMD_DEGAS_SELECT" || komut == "DEGAS_SEL" ||
      komut == "CMD_DEGAS_ARM" || komut == "DEGAS_ARM" || komut == "CMD_DEGAS" ||
      komut == "b_deg" || komut == "CMD_DEGAS_TOGGLE" || komut == "DEG_TOGGLE" ||
      komut == "DEG_SEL" || komut == "DEG" || komut == "b_degas") {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) {
      DEBUG_PRINTLN("--> DEGAS SECIMI REDDEDILDI: Yıkama/DEGAS aktif!");
      return;
    }
    degas_armed[secili_goz] = !degas_armed[secili_goz];
    if (degas_armed[secili_goz]) {
      aktif_program = 0; // Deselect preset programs P1, P2, P3, FP
      runtime_sweep[secili_goz] = false;
      stmSweep(false); // DEGAS ve Sweep kesinlikle birbirini dislar
      hedef_sure[secili_goz] = service_degas[secili_goz].duration_minutes;
      hedef_sicaklik[secili_goz] = (int)service_degas[secili_goz].target_temp_c;
      durum_metni[secili_goz] = "DEGAS SECILDI. START BEKLENIYOR";
      updatePage0UI();
      nextionGonder("t_durum.txt=\"" + durum_metni[secili_goz] + "\"");
      DEBUG_PRINTLN("--> ESP32: GÖZ " + String(secili_goz) + " DEGAS ARMED! (Sure: " + String(hedef_sure[secili_goz]) + " Dk, Sic: " + String(hedef_sicaklik[secili_goz]) + "C)");
    } else {
      durum_metni[secili_goz] = "SISTEM BEKLEMEDE";
      updatePage0UI();
      nextionGonder("t_durum.txt=\"" + durum_metni[secili_goz] + "\"");
      DEBUG_PRINTLN("--> ESP32: GÖZ " + String(secili_goz) + " DEGAS DISARMED.");
    }
  }
  else if (komut == "CMD_DEGAS_DESELECT" || komut == "DEGAS_DESEL") {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) return;
    degas_armed[secili_goz] = false;
    durum_metni[secili_goz] = "SISTEM BEKLEMEDE";
    updatePage0UI();
    nextionGonder("t_durum.txt=\"" + durum_metni[secili_goz] + "\"");
    DEBUG_PRINTLN("--> ESP32: GÖZ " + String(secili_goz) + " DEGAS DESELECTED.");
  }

  // --- SWEEP TOGGLE (Page 0 b_sweep / b_swe) ---
  else if (komut == "CMD_SWEEP_TOGGLE" || komut == "SWP_TOGGLE" || komut == "PAGE0_SWP_TOGGLE" ||
           ((komut == "b_sweep" || komut == "b_swe") && aktif_sayfa == 0)) {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) {
      DEBUG_PRINTLN("--> SWEEP LOCKED: PROCESS IS ACTIVE!");
      return;
    }
    if (degas_armed[secili_goz]) {
      degas_armed[secili_goz] = false;
    }
    runtime_sweep[secili_goz] = !runtime_sweep[secili_goz];
    stmSweep(runtime_sweep[secili_goz]);
    updatePage0ButtonColors(); // Sadece buton renklerini güncelle (Manuel süre/sıcaklık korunur)
    DEBUG_PRINTLN("--> ESP32: GÖZ " + String(secili_goz) + " SWEEP " + String(runtime_sweep[secili_goz] ? "ENABLED" : "DISABLED") + " (Setpoints Preserved)");
  }
  else if (komut == "CMD_SWEEP_ON" || komut == "CMD_SWEEP|ON") {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) {
      DEBUG_PRINTLN("--> SWEEP LOCKED: PROCESS IS ACTIVE!");
      return;
    }
    if (degas_armed[secili_goz]) {
      degas_armed[secili_goz] = false;
    }
    runtime_sweep[secili_goz] = true;
    stmSweep(true);
    updatePage0ButtonColors();
    DEBUG_PRINTLN("--> SWEEP ON! GOZ: " + String(secili_goz));
  }
  else if (komut == "CMD_SWEEP_OFF" || komut == "CMD_SWEEP|OFF") {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) {
      DEBUG_PRINTLN("--> SWEEP LOCKED: PROCESS IS ACTIVE!");
      return;
    }
    runtime_sweep[secili_goz] = false;
    stmSweep(false);
    updatePage0ButtonColors();
    DEBUG_PRINTLN("--> SWEEP OFF! GOZ: " + String(secili_goz));
  }

  // --- HIZLI PROGRAM (FP) (Page 0) ---
  else if (komut == "P_HIZLI") {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) {
      DEBUG_PRINTLN("--> P_HIZLI REDDEDILDI: ISLEM AKTIF!");
      return;
    }

    if (degas_armed[secili_goz]) {
      degas_armed[secili_goz] = false;
      nextionGonder("b_deg.bco=" + String(NEXTION_COLOR_DEFAULT));
    }

    aktif_program = 4;
    hedef_sure[secili_goz] = 5;    
    hedef_sicaklik[secili_goz] = 30; 
    runtime_sweep[secili_goz] = false;
    kalan_saniye[secili_goz] = 0;
    makine_calisiyor[secili_goz] = false;
    durum_metni[secili_goz] = "HIZLI PROGRAM SECILDI. START BEKLENIYOR";
    DEBUG_PRINTLN("--> ESP32: GÖZ " + String(secili_goz) + " ICIN FP YÜKLENDİ (START BEKLENIYOR)!");
    
    updatePage0UI();
    nextionGonder("t_durum.txt=\"" + durum_metni[secili_goz] + "\"");

    if (isKartBagli(secili_goz)) {
      stmSetTime(hedef_sure[secili_goz]);
      stmSetTemp(hedef_sicaklik[secili_goz]);
      stmSetPower(guc_seviyesi[secili_goz]);
      stmSetFreq(stm_freq[secili_goz]);
      stmSweep(false);
    }
  }
  
  // --- MOTORU BAŞLAT (Page 0) ---
  else if (komut.startsWith("CMD_START|") || komut == "CMD_START") {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz] || durum_metni[secili_goz].startsWith("HATA!")) {
      DEBUG_PRINTLN("--> START REDDEDILDI: ISLEM VEYA ARIZA AKTIF!");
      return;
    }
    if (baslatmaEngelliMi()) return;

    if (degas_armed[secili_goz]) {
      int g = secili_goz;
      degas_active[g] = true;
      makine_calisiyor[g] = false;
      kalan_saniye[g] = service_degas[g].duration_minutes * 60;
      durum_metni[g] = "DEGAS DEVAM EDIYOR...";
      nextionGonder("t_durum.txt=\"" + durum_metni[g] + "\"");
      String snapCmd = "START_DEGAS:" + String(service_degas[g].duration_minutes) +
                       ":" + String(service_degas[g].power_pct) +
                       ":" + String(service_degas[g].frequency_khz) +
                       ":" + String(service_degas[g].pulse_on_ms) +
                       ":" + String(service_degas[g].pulse_off_ms) +
                       ":" + String(service_degas[g].temp_ctrl) +
                       ":" + String(service_degas[g].target_temp_c, 1);
      DEBUG_PRINTLN("--> DEGAS START! GÖZ: " + String(g) + " Frame: " + snapCmd);
      stmGonder(snapCmd + "\n");
      return;
    }

    int fPipe = komut.indexOf('|');
    int sPipe = (fPipe != -1) ? komut.indexOf('|', fPipe + 1) : -1;
    
    if (fPipe != -1 && sPipe != -1) {
      String s_sure = komut.substring(fPipe + 1, sPipe);
      String s_sicaklik = komut.substring(sPipe + 1);
      
      int parsed_sure = s_sure.toInt();
      int parsed_sic = s_sicaklik.toInt();
      if (parsed_sure > 0) hedef_sure[secili_goz] = parsed_sure;
      if (parsed_sic > 0) hedef_sicaklik[secili_goz] = parsed_sic;

      DEBUG_PRINTLN("DEBUG_ESP32: HMI_PARSE cmd=CMD_START sure=" + String(hedef_sure[secili_goz]) + " sicaklik=" + String(hedef_sicaklik[secili_goz]));
    }

    if (hedef_sure[secili_goz] == 0) {
      durum_metni[secili_goz] = "SURE GIRIN!";
      DEBUG_PRINTLN("--> HATA: Sıfır süreyle başlatılamaz!");
      nextionGonder("t_durum.txt=\"SURE GIRIN!\"");
      return;
    }

    makine_calisiyor[secili_goz] = true;
    kalan_saniye[secili_goz] = hedef_sure[secili_goz] * 60; 
    durum_metni[secili_goz] = "YIKAMA DEVAM EDIYOR...";
    nextionGonder("t_durum.txt=\"" + durum_metni[secili_goz] + "\"");
    DEBUG_PRINTLN("--> MOTOR START! GÖZ: " + String(secili_goz) + " Hedef: " + String(hedef_sure[secili_goz]) + " Dk");

    stmSetTime(hedef_sure[secili_goz]);
    stmSetTemp(hedef_sicaklik[secili_goz]);
    stmSetPower(guc_seviyesi[secili_goz]);
    stmSetStepInc(service_sweep[secili_goz].step_increment);
    stmSetSwpSpan(service_sweep[secili_goz].span_khz);
    stmSetSwpPer(service_sweep[secili_goz].period_ms);
    stmSetFreq(stm_freq[secili_goz]);
    if (runtime_sweep[secili_goz]) {
      stmSweep(true);
    }
    stmStart();
  }
  
  // --- DURDUR (Page 0) ---
  else if (komut == "CMD_STOP") {
    makine_calisiyor[secili_goz] = false;
    degas_active[secili_goz] = false;
    runtime_sweep[secili_goz] = false;
    if (degas_armed[secili_goz]) {
      durum_metni[secili_goz] = "DEGAS DURDURULDU. START BEKLENIYOR";
    } else {
      durum_metni[secili_goz] = "SISTEM DURDURULDU";
    }
    nextionGonder("t_durum.txt=\"" + durum_metni[secili_goz] + "\"");
    DEBUG_PRINTLN("--> MOTOR STOP! GÖZ: " + String(secili_goz) + " Durduruldu.");
    stmSweep(false);
    stmStop();
    if (aktif_sayfa == 0) {
      updatePage0ButtonColors();
      updatePage0UI();
    }
  }

  // --- FREKANS TOGGLE (Page 0 b_freq / CMD_FREQ_TOGGLE) ---
  else if (komut == "CMD_FREQ_TOGGLE" || komut == "FREQ_TOGGLE" ||
           ((komut == "b_freq" || komut == "b_frq") && aktif_sayfa == 0)) {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) {
      DEBUG_PRINTLN("--> FREQ TOGGLE LOCKED: PROCESS IS ACTIVE!");
      return;
    }
    if (degas_armed[secili_goz]) {
      degas_armed[secili_goz] = false;
      nextionGonder("b_deg.bco=" + String(NEXTION_COLOR_DEFAULT));
    }
    runtime_sweep[secili_goz] = false;
    int yeni_freq = (stm_freq[secili_goz] == 40) ? 28 : 40;
    stm_freq[secili_goz] = yeni_freq;
    stmSetFreq(yeni_freq);
    stmSweep(false);
    String freqMetni = (yeni_freq == 40) ? "40k" : "28k";
    nextionGonder("b_freq.txt=\"" + freqMetni + "\"");
    nextionGonder("b_frq.txt=\"" + freqMetni + "\"");
    updatePage0ButtonColors();
    DEBUG_PRINTLN("--> ESP32: GÖZ " + String(secili_goz) + " FREQ TOGGLED TO " + String(yeni_freq) + " kHz (Sweep Disarmed)");
  }

  // --- FREKANS SEÇİMİ (28 kHz / 40 kHz) ---
  else if (komut.startsWith("CMD_FREQ|") || komut.startsWith("SET_FREQ|")) {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) {
      DEBUG_PRINTLN("--> FREQ SELECTION LOCKED: PROCESS IS ACTIVE!");
      return;
    }
    if (degas_armed[secili_goz]) {
      degas_armed[secili_goz] = false;
      nextionGonder("b_deg.bco=" + String(NEXTION_COLOR_DEFAULT));
    }
    int pipe = komut.indexOf('|');
    if (pipe != -1) {
      int freq = komut.substring(pipe + 1).toInt();
      if (freq == 28 || freq == 40) {
        runtime_sweep[secili_goz] = false;
        stm_freq[secili_goz] = freq;
        stmSetFreq(freq);
        stmSweep(false);
        String freqMetni = (freq == 40) ? "40k" : "28k";
        nextionGonder("b_freq.txt=\"" + freqMetni + "\"");
        nextionGonder("b_frq.txt=\"" + freqMetni + "\"");
        updatePage0ButtonColors();
        DEBUG_PRINTLN("--> FREKANS DEĞİŞTİRİLDİ: " + String(freq) + " kHz (Göz: " + String(secili_goz) + ", Sweep Disarmed)");
      }
    }
  }

  // --- PROGRAM SEÇİMLERİ (Page 0) ---
  else if (komut == "P1_SEL" || komut == "P2_SEL" || komut == "P3_SEL") {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) {
      DEBUG_PRINTLN("--> RECIPE SELECTION LOCKED: PROCESS IS ACTIVE!");
      return;
    }

    if (degas_armed[secili_goz]) {
      degas_armed[secili_goz] = false;
      nextionGonder("b_deg.bco=" + String(NEXTION_COLOR_DEFAULT));
    }

    aktif_program = (komut == "P1_SEL") ? 1 : ((komut == "P2_SEL") ? 2 : 3);

    hedef_sure[secili_goz] = p_sure[aktif_program];
    hedef_sicaklik[secili_goz] = p_sicaklik[aktif_program];
    runtime_sweep[secili_goz] = (p_sweep[aktif_program] != 0);
    kalan_saniye[secili_goz] = 0; 
    makine_calisiyor[secili_goz] = false; 
    durum_metni[secili_goz] = "P" + String(aktif_program) + " SECILDI. START BEKLENIYOR";
    
    // Page 0 UI güncelleme
    updatePage0UI();
    nextionGonder("t_durum.txt=\"" + durum_metni[secili_goz] + "\"");
    
    if (isKartBagli(secili_goz)) {
      stmSetTime(hedef_sure[secili_goz]);
      stmSetTemp(hedef_sicaklik[secili_goz]);
      stmSetFreq(stm_freq[secili_goz]);
      stmSweep(runtime_sweep[secili_goz]);
    }

    DEBUG_PRINTLN("--> ESP32: GÖZ " + String(secili_goz) + " için P" + String(aktif_program) + " yüklendi (Sweep=" + String(runtime_sweep[secili_goz] ? "ON" : "OFF") + ")");
  }
  // --- MANUEL MOD (Page 0) ---
  else if (komut == "MANUAL_MODE" || komut == "MAN_SEL" || komut == "P0_SEL") {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) {
      DEBUG_PRINTLN("--> MANUAL MODE LOCKED: PROCESS IS ACTIVE!");
      return;
    }
    aktif_program = 0;
    String freqMetni = (stm_freq[secili_goz] == 40) ? "40k" : "28k";
    nextionGonder("b_freq.txt=\"" + freqMetni + "\"");
    nextionGonder("b_frq.txt=\"" + freqMetni + "\"");
    updatePage0ButtonColors();
    if (degas_armed[secili_goz]) {
      durum_metni[secili_goz] = "DEGAS SECILDI. START BEKLENIYOR";
    } else {
      durum_metni[secili_goz] = "MANUEL MOD";
    }
    nextionGonder("t_durum.txt=\"" + durum_metni[secili_goz] + "\"");
    DEBUG_PRINTLN("--> ESP32: GÖZ " + String(secili_goz) + " MANUEL MOD SECILDI (Programlar pasif, frekans ve setpointler korundu).");
  }

  // --- SETPOINT AYARLARI (Page 0) ---
  else if ((aktif_sayfa == 0 || aktif_sayfa == 1) && (komut == "TIME_UP" || komut == "TIME_DOWN" || komut.startsWith("SET_TIME:") || komut.startsWith("CMD_SET_TIME:"))) {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) return;
    int pipe = komut.indexOf(':');
    if (pipe != -1) {
      int val = komut.substring(pipe + 1).toInt();
      if (val >= 1 && val <= 120) {
        hedef_sure[secili_goz] = val;
        stmSetTime(val);
      }
    } else if (komut == "TIME_UP") {
      if (hedef_sure[secili_goz] < 120) {
        hedef_sure[secili_goz]++;
        stmSetTime(hedef_sure[secili_goz]);
      }
    } else if (komut == "TIME_DOWN") {
      if (hedef_sure[secili_goz] > 1) {
        hedef_sure[secili_goz]--;
        stmSetTime(hedef_sure[secili_goz]);
      }
    }
    nextionGonder(String("t_set_sure.txt=\"") + (hedef_sure[secili_goz] < 10 ? "0" : "") + String(hedef_sure[secili_goz]) + "\"");
  }
  else if ((aktif_sayfa == 0 || aktif_sayfa == 1) && (komut == "TEMP_UP" || komut == "TEMP_DOWN" || komut.startsWith("SET_TEMP:") || komut.startsWith("CMD_SET_TEMP:"))) {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) return;
    int pipe = komut.indexOf(':');
    if (pipe != -1) {
      int val = komut.substring(pipe + 1).toInt();
      if (val >= 20 && val <= 90) {
        hedef_sicaklik[secili_goz] = val;
        stmSetTemp(val);
      }
    } else if (komut == "TEMP_UP") {
      if (hedef_sicaklik[secili_goz] < 90) {
        hedef_sicaklik[secili_goz]++;
        stmSetTemp(hedef_sicaklik[secili_goz]);
      }
    } else if (komut == "TEMP_DOWN") {
      if (hedef_sicaklik[secili_goz] > 20) {
        hedef_sicaklik[secili_goz]--;
        stmSetTemp(hedef_sicaklik[secili_goz]);
      }
    }
    nextionGonder("t_set_sic.txt=\"" + String(hedef_sicaklik[secili_goz]) + "\"");
  }

  // =========================================================================
  // PAGE 2: PROGRAM / RECIPE STORAGE (DRAFT BUFFER ARCHITECTURE)
  // =========================================================================
  else if (komut == "PAGE2_OPEN" || komut == "PAGE2" || komut == "b_programlar") {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) {
      DEBUG_PRINTLN("--> RECIPE MENU LOCKED: PROCESS IS ACTIVE!");
      return;
    }
    aktif_sayfa = 2;
    initRecipeEditBuffers();
    nextionGonder("page page2");
    updatePage2UI(duzenlenen_program);
  }
  else if (komut == "EDIT_P1" || komut == "EDIT_P2" || komut == "EDIT_P3" ||
           (aktif_sayfa == 2 && (komut == "b_edit_p1" || komut == "b_edit_p2" || komut == "b_edit_p3" || komut == "b_p1" || komut == "b_p2" || komut == "b_p3"))) {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) {
      DEBUG_PRINTLN("--> RECIPE EDITING LOCKED: PROCESS IS ACTIVE!");
      return;
    }
    if (komut == "EDIT_P1" || komut == "b_edit_p1" || komut == "b_p1") duzenlenen_program = 1;
    else if (komut == "EDIT_P2" || komut == "b_edit_p2" || komut == "b_p2") duzenlenen_program = 2;
    else if (komut == "EDIT_P3" || komut == "b_edit_p3" || komut == "b_p3") duzenlenen_program = 3;

    // Freshly copy real/master values to draft for this program
    edit_p_sure[duzenlenen_program] = p_sure[duzenlenen_program];
    edit_p_sicaklik[duzenlenen_program] = p_sicaklik[duzenlenen_program];
    edit_p_sweep[duzenlenen_program] = p_sweep[duzenlenen_program];

    updatePage2UI(duzenlenen_program);
    DEBUG_PRINTLN("--> ESP32: Page 2'de P" + String(duzenlenen_program) + " düzenleniyor (Temiz taslak yüklendi).");
  }
  else if (komut == "P_TIME_UP" || komut == "PAGE2_TIME_UP" || komut == "EDIT_TIME_UP" || komut == "b_edit_sure_up" || komut.startsWith("SET_EDIT_TIME:") ||
           (aktif_sayfa == 2 && (komut == "TIME_UP" || komut == "b_sure_up" || komut == "b_time_up" || komut == "b_up" || komut.startsWith("SET_TIME:")))) {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) return;
    int colon = komut.indexOf(':');
    if (colon != -1) {
      int val = komut.substring(colon + 1).toInt();
      if (val >= 1 && val <= 120) edit_p_sure[duzenlenen_program] = val;
    } else {
      if (edit_p_sure[duzenlenen_program] < 120) edit_p_sure[duzenlenen_program]++;
    }
    updatePage2UI(duzenlenen_program);
    DEBUG_PRINTLN("--> ESP32: Page 2 P" + String(duzenlenen_program) + " Draft Sure=" + String(edit_p_sure[duzenlenen_program]));
  }
  else if (komut == "P_TIME_DOWN" || komut == "PAGE2_TIME_DOWN" || komut == "EDIT_TIME_DOWN" || komut == "b_edit_sure_down" ||
           (aktif_sayfa == 2 && (komut == "TIME_DOWN" || komut == "b_sure_down" || komut == "b_time_down" || komut == "b_down"))) {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) return;
    if (edit_p_sure[duzenlenen_program] > 1) edit_p_sure[duzenlenen_program]--;
    updatePage2UI(duzenlenen_program);
    DEBUG_PRINTLN("--> ESP32: Page 2 P" + String(duzenlenen_program) + " Draft Sure=" + String(edit_p_sure[duzenlenen_program]));
  }
  else if (komut == "P_TEMP_UP" || komut == "PAGE2_TEMP_UP" || komut == "EDIT_TEMP_UP" || komut == "b_edit_sic_up" || komut.startsWith("SET_EDIT_TEMP:") ||
           (aktif_sayfa == 2 && (komut == "TEMP_UP" || komut == "b_sic_up" || komut == "b_temp_up" || komut.startsWith("SET_TEMP:")))) {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) return;
    int colon = komut.indexOf(':');
    if (colon != -1) {
      int val = komut.substring(colon + 1).toInt();
      if (val >= 20 && val <= 90) edit_p_sicaklik[duzenlenen_program] = val;
    } else {
      if (edit_p_sicaklik[duzenlenen_program] < 90) edit_p_sicaklik[duzenlenen_program]++;
    }
    updatePage2UI(duzenlenen_program);
    DEBUG_PRINTLN("--> ESP32: Page 2 P" + String(duzenlenen_program) + " Draft Sicaklik=" + String(edit_p_sicaklik[duzenlenen_program]));
  }
  else if (komut == "P_TEMP_DOWN" || komut == "PAGE2_TEMP_DOWN" || komut == "EDIT_TEMP_DOWN" || komut == "b_edit_sic_down" ||
           (aktif_sayfa == 2 && (komut == "TEMP_DOWN" || komut == "b_sic_down" || komut == "b_temp_down"))) {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) return;
    if (edit_p_sicaklik[duzenlenen_program] > 20) edit_p_sicaklik[duzenlenen_program]--;
    updatePage2UI(duzenlenen_program);
    DEBUG_PRINTLN("--> ESP32: Page 2 P" + String(duzenlenen_program) + " Draft Sicaklik=" + String(edit_p_sicaklik[duzenlenen_program]));
  }
  else if (komut == "EDIT_SWEEP_TOG" || komut == "b_edit_sweep" || komut == "P_SWEEP_TOGGLE" ||
           komut == "EDIT_SWEEP_TOGGLE" || komut == "PAGE2_SWEEP_TOGGLE" ||
           ((komut == "b_sweep" || komut == "b_swe") && aktif_sayfa == 2)) {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) {
      DEBUG_PRINTLN("--> RECIPE EDITING LOCKED: PROCESS IS ACTIVE!");
      return;
    }
    edit_p_sweep[duzenlenen_program] = (edit_p_sweep[duzenlenen_program] != 0) ? 0 : 1;
    updatePage2UI(duzenlenen_program);
    DEBUG_PRINTLN("--> ESP32: Page 2 P" + String(duzenlenen_program) + " Draft Sweep=" + String(edit_p_sweep[duzenlenen_program] != 0 ? "ON" : "OFF"));
  }
  else if (komut.startsWith("P_SAVE|") || komut == "P_SAVE" || komut == "P_KAYDET" || (aktif_sayfa == 2 && (komut == "b_save" || komut == "b_kaydet"))) {
    if (degas_active[secili_goz] || makine_calisiyor[secili_goz]) {
      DEBUG_PRINTLN("--> RECIPE SAVE LOCKED: PROCESS IS ACTIVE!");
      return;
    }
    int fPipe = komut.indexOf('|');
    int sPipe = (fPipe != -1) ? komut.indexOf('|', fPipe + 1) : -1;
    int tPipe = (sPipe != -1) ? komut.indexOf('|', sPipe + 1) : -1;
    
    if (fPipe != -1 && sPipe != -1) {
      String s_sure = komut.substring(fPipe + 1, sPipe);
      String s_sicaklik = (tPipe != -1) ? komut.substring(sPipe + 1, tPipe) : komut.substring(sPipe + 1);
      int ps = s_sure.toInt();
      int pt = s_sicaklik.toInt();
      if (ps >= 1 && ps <= 120) edit_p_sure[duzenlenen_program] = ps;
      if (pt >= 20 && pt <= 90) edit_p_sicaklik[duzenlenen_program] = pt;
      if (tPipe != -1) {
        edit_p_sweep[duzenlenen_program] = (komut.substring(tPipe + 1).toInt() != 0) ? 1 : 0;
      }
    }
    // COMMIT DRAFT TO MASTER ARRAYS
    p_sure[duzenlenen_program] = edit_p_sure[duzenlenen_program];
    p_sicaklik[duzenlenen_program] = edit_p_sicaklik[duzenlenen_program];
    p_sweep[duzenlenen_program] = edit_p_sweep[duzenlenen_program];
    nvsKaydet();
    DEBUG_PRINTLN("--> KAYIT BASARILI: P" + String(duzenlenen_program) + " güncellendi. (" + String(p_sure[duzenlenen_program]) + " Dk / " + String(p_sicaklik[duzenlenen_program]) + "C / Sweep=" + String(p_sweep[duzenlenen_program]) + ")");
    nextionGonder("b_save.bco=" + String(NEXTION_COLOR_GREEN));
    delay(400);
    nextionGonder("b_save.bco=" + String(NEXTION_COLOR_DEFAULT));
    updatePage2UI(duzenlenen_program);
  }
  else if (aktif_sayfa == 2 && (komut == "BACK" || komut == "b_back" || komut == "PAGE2_BACK")) {
    // DISCARD UNCOMMITTED EDITS: Revert draft buffers from master p_*
    discardRecipeEditBuffers();
    aktif_sayfa = 3;
    nextionGonder("page page3");
    updatePage3UI();
  }

  // =========================================================================
  // PAGE 3: MAIN SETTINGS MENU (PROGRAMLAR, SERVIS, DIL, SAAT)
  // =========================================================================
  else if (komut == "PAGE3_OPEN" || komut == "PAGE3" || komut == "b_set" || komut == "b_ayarlar") {
    aktif_sayfa = 3;
    nextionGonder("page page3");
    updatePage3UI();
  }

  // =========================================================================
  // PAGE 1: KART/GÖZ SEÇİMİ (OPERATOR TANK SELECT)
  // =========================================================================
  else if (komut == "PAGE1_OPEN" || komut == "PAGE1") {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) {
      DEBUG_PRINTLN("--> PAGE1 LOCKED: PROCESS IS ACTIVE!");
      return;
    }
    aktif_sayfa = 1;
    nextionGonder("page page1");
    updatePage1UI(secili_goz);
  }
  else if (komut == "TANK_UP" || (aktif_sayfa == 1 && (komut == "UP" || komut == "b_up" || komut == "b_goz_up")) || komut == "PAGE1_UP") {
    if (temp_goz < max_goz_sayisi) temp_goz++;
    updatePage1UI(temp_goz);
  }
  else if (komut == "TANK_DOWN" || (aktif_sayfa == 1 && (komut == "DOWN" || komut == "b_down" || komut == "b_goz_down")) || komut == "PAGE1_DOWN") {
    if (temp_goz > 1) temp_goz--;
    updatePage1UI(temp_goz);
  }
  else if (komut == "TANK_SEL_OK" || komut == "SEL" || (aktif_sayfa == 1 && (komut == "b_ok" || komut == "b_onayla")) || komut == "PAGE1_OK" || komut == "PAGE1_SEL") {
    secili_goz = temp_goz;
    aktif_sayfa = 0;
    nextionGonder("page page0");
    updatePage0UI();
    stmSetpointleriGonder();
  }
  else if ((aktif_sayfa == 1 && (komut == "BACK" || komut == "b_back")) || komut == "PAGE1_BACK") {
    temp_goz = secili_goz;
    aktif_sayfa = 0;
    nextionGonder("page page0");
    updatePage0UI();
  }

  // =========================================================================
  // PAGE 4: ŞİFRE VE KLAVYE
  // =========================================================================
  else if (komut == "PAGE4_OPEN" || komut == "PAGE4" || komut == "b_servis") {
    aktif_sayfa = 4;
    nextionGonder("page page4");
    updatePage4UI();
  }
  else if (komut == "KEY_BACK" || (aktif_sayfa == 4 && (komut == "b_back" || komut == "BACK" || komut == "b_geri"))) {
    discardServiceEditBuffers();
    girilen_sifre = "";
    aktif_sayfa = 3;
    nextionGonder("page page3");
    updatePage3UI();
  }
  else if ((komut.length() == 1 && isDigit(komut.charAt(0))) ||
           (komut.startsWith("KEY_") && komut.length() == 5 && isDigit(komut.charAt(4))) ||
           (komut.startsWith("KEY") && komut.length() == 4 && isDigit(komut.charAt(3))) ||
           (komut.startsWith("b_num") && komut.length() == 6 && isDigit(komut.charAt(5))) ||
           (komut.startsWith("b_") && komut.length() == 3 && isDigit(komut.charAt(2))) ||
           (komut.startsWith("b") && komut.length() == 2 && isDigit(komut.charAt(1)))) {
    char digit = (komut.length() == 1) ? komut.charAt(0) :
                 (komut.startsWith("KEY_")) ? komut.charAt(4) :
                 (komut.startsWith("KEY")) ? komut.charAt(3) :
                 (komut.startsWith("b_num")) ? komut.charAt(5) :
                 (komut.startsWith("b_")) ? komut.charAt(2) : komut.charAt(1);
    if (girilen_sifre.length() < 6) {
      girilen_sifre += digit;
      String yildizlar = "";
      for (int i = 0; i < girilen_sifre.length(); i++) yildizlar += "*";
      nextionGonder("t_sifre.txt=\"" + yildizlar + "\"");
      nextionGonder("t_pass.txt=\"" + yildizlar + "\"");
    }
  }
  else if (komut == "KEY_DEL" || komut == "b_del" || komut == "b_sil" || komut == "PAGE4_DEL" || komut == "DEL") {
    if (girilen_sifre.length() > 0) {
      girilen_sifre.remove(girilen_sifre.length() - 1);
      String yildizlar = "";
      for (int i = 0; i < girilen_sifre.length(); i++) yildizlar += "*";
      nextionGonder("t_sifre.txt=\"" + yildizlar + "\"");
      nextionGonder("t_pass.txt=\"" + yildizlar + "\"");
    }
  }
  else if (komut == "KEY_SPACE" || komut == "b_space" || komut == "b_bosluk" || komut == "KEY_SPC") {
    // Space key handler
  }
  else if (komut == "KEY_OK" || komut == "b_onayla" || komut == "PAGE4_OK" || (aktif_sayfa == 4 && komut == "b_ok")) {
    if (girilen_sifre == dogru_sifre) {
      girilen_sifre = "";
      g_service_authenticated = true;
      service_auth_time = millis();
      initServiceEditBuffers();
      current_service_page = 5;
      aktif_sayfa = 5;
      nextionGonder("page page5");
      updatePage5UI(secili_goz);
    } else {
      girilen_sifre = "";
      g_service_authenticated = false;
      nextionGonder("t_sifre.txt=\"HATALI!\"");
      nextionGonder("t_pass.txt=\"HATALI!\"");
    }
  }

  // =========================================================================
  // SERVICE MENU NAVIGATION (PAGES 5, 6, 7, 8 CYCLIC GROUP)
  // =========================================================================
  else if (komut == "NAV_FORWARD" || komut == "b_forwoard" || komut == "PAGE_FORWARD") {
    if (!isProvisioningAllowed()) {
      nextionGonder("b_save.bco=" + String(NEXTION_COLOR_RED));
      return;
    }
    if (current_service_page == 5) {
      current_service_page = 6;
      aktif_sayfa = 6;
      nextionGonder("page page6");
      updatePage6UI(secili_goz);
    } else if (current_service_page == 6) {
      current_service_page = 7;
      aktif_sayfa = 7;
      nextionGonder("page page7");
      updatePage7UI(secili_goz);
    } else if (current_service_page == 7) {
      current_service_page = 8;
      aktif_sayfa = 8;
      nextionGonder("page page8");
      updatePage8UI(secili_goz);
    } else if (current_service_page == 8) {
      current_service_page = 5;
      aktif_sayfa = 5;
      nextionGonder("page page5");
      updatePage5UI(secili_goz);
    }
  }
  else if (komut == "NAV_BACK" || (aktif_sayfa >= 5 && komut == "b_back") || komut == "PAGE_BACK") {
    if (!isProvisioningAllowed()) {
      nextionGonder("b_save.bco=" + String(NEXTION_COLOR_RED));
      return;
    }
    if (current_service_page == 5) {
      current_service_page = 8;
      aktif_sayfa = 8;
      nextionGonder("page page8");
      updatePage8UI(secili_goz);
    } else if (current_service_page == 8) {
      current_service_page = 7;
      aktif_sayfa = 7;
      nextionGonder("page page7");
      updatePage7UI(secili_goz);
    } else if (current_service_page == 7) {
      current_service_page = 6;
      aktif_sayfa = 6;
      nextionGonder("page page6");
      updatePage6UI(secili_goz);
    } else if (current_service_page == 6) {
      current_service_page = 5;
      aktif_sayfa = 5;
      nextionGonder("page page5");
      updatePage5UI(secili_goz);
    }
  }
  else if (komut == "SRV_DISCARD" || komut == "SERVICE_EXIT" || komut == "b_exit" || komut == "SRV_EXIT" || komut == "EXIT_SERVICE" || komut == "SRV_BACK") {
    discardServiceEditBuffers();
    g_service_authenticated = false;
    aktif_sayfa = 0;
    nextionGonder("page page0");
    updatePage0UI();
  }

  // =========================================================================
  // =========================================================================
  // PAGE 5: SERVICE SETTINGS / SELECTED TANK (TANK-SCOPED)
  // =========================================================================
  else if (komut == "PAGE5_OPEN" || komut == "PAGE5") {
    current_service_page = 5;
    aktif_sayfa = 5;
    updatePage5UI(secili_goz);
  }
  else if (komut == "PAGE8_OPEN" || komut == "PAGE8") {
    current_service_page = 8;
    aktif_sayfa = 8;
    updatePage8UI(secili_goz);
  }
  // Tank selection on Page 5/6/7/8: b_srv_goz_up, b_swp_goz_up, b_deg_goz_up
  else if (komut == "SRV_TANK_UP" || komut == "b_srv_goz_up" || komut == "b_swp_goz_up" || komut == "b_deg_goz_up" ||
           komut == "PAGE5_GOZ_UP" || komut == "PAGE6_GOZ_UP" || komut == "PAGE7_GOZ_UP" || komut == "PAGE8_GOZ_UP" ||
           komut == "SWP_GOZ_UP" || komut == "DEG_GOZ_UP" || komut == "SRV_GOZ_INC" ||
           ((aktif_sayfa >= 5 && aktif_sayfa <= 8) && (komut == "b_goz_up" || komut == "GOZ_UP" || komut == "b_up" || komut == "UP"))) {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();

    if (komut == "b_swp_goz_up" || komut == "PAGE6_GOZ_UP" || komut == "SWP_GOZ_UP") {
      current_service_page = 6;
      aktif_sayfa = 6;
    } else if (komut == "b_deg_goz_up" || komut == "PAGE7_GOZ_UP" || komut == "DEG_GOZ_UP") {
      current_service_page = 7;
      aktif_sayfa = 7;
    } else if (komut == "b_srv_goz_up" || komut == "PAGE5_GOZ_UP") {
      current_service_page = 5;
      aktif_sayfa = 5;
    }

    if (secili_goz < edit_max_goz_sayisi) {
      secili_goz++;
    } else {
      secili_goz = 1;
    }
    switch (current_service_page) {
      case 5: updatePage5UI(secili_goz); break;
      case 6: updatePage6UI(secili_goz); break;
      case 7: updatePage7UI(secili_goz); break;
      case 8: updatePage8UI(secili_goz); break;
      default:
        if (aktif_sayfa == 6) updatePage6UI(secili_goz);
        else if (aktif_sayfa == 7) updatePage7UI(secili_goz);
        else if (aktif_sayfa == 8) updatePage8UI(secili_goz);
        else updatePage5UI(secili_goz);
        break;
    }
    DEBUG_PRINTLN("--> ESP32 Service Tank Changed: GÖZ " + String(secili_goz) + " on Page " + String(current_service_page));
  }
  else if (komut == "SRV_TANK_DOWN" || komut == "b_srv_goz_down" || komut == "b_swp_goz_down" || komut == "b_deg_goz_down" ||
           komut == "PAGE5_GOZ_DOWN" || komut == "PAGE6_GOZ_DOWN" || komut == "PAGE7_GOZ_DOWN" || komut == "PAGE8_GOZ_DOWN" ||
           komut == "SWP_GOZ_DOWN" || komut == "DEG_GOZ_DOWN" || komut == "SRV_GOZ_DEC" ||
           ((aktif_sayfa >= 5 && aktif_sayfa <= 8) && (komut == "b_goz_down" || komut == "GOZ_DOWN" || komut == "b_down" || komut == "DOWN"))) {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();

    if (komut == "b_swp_goz_down" || komut == "PAGE6_GOZ_DOWN" || komut == "SWP_GOZ_DOWN") {
      current_service_page = 6;
      aktif_sayfa = 6;
    } else if (komut == "b_deg_goz_down" || komut == "PAGE7_GOZ_DOWN" || komut == "DEG_GOZ_DOWN") {
      current_service_page = 7;
      aktif_sayfa = 7;
    } else if (komut == "b_srv_goz_down" || komut == "PAGE5_GOZ_DOWN") {
      current_service_page = 5;
      aktif_sayfa = 5;
    }

    if (secili_goz > 1) {
      secili_goz--;
    } else {
      secili_goz = edit_max_goz_sayisi;
    }
    switch (current_service_page) {
      case 5: updatePage5UI(secili_goz); break;
      case 6: updatePage6UI(secili_goz); break;
      case 7: updatePage7UI(secili_goz); break;
      case 8: updatePage8UI(secili_goz); break;
      default:
        if (aktif_sayfa == 6) updatePage6UI(secili_goz);
        else if (aktif_sayfa == 7) updatePage7UI(secili_goz);
        else if (aktif_sayfa == 8) updatePage8UI(secili_goz);
        else updatePage5UI(secili_goz);
        break;
    }
    DEBUG_PRINTLN("--> ESP32 Service Tank Changed: GÖZ " + String(secili_goz) + " on Page " + String(current_service_page));
  }
  else if (komut == "SRV_GUC_UP" || komut == "b_srv_guc_up" || komut == "GUC_UP" || komut == "b_guc_up") {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 5;
    aktif_sayfa = 5;
    if (edit_guc_seviyesi[secili_goz] < 100) edit_guc_seviyesi[secili_goz] += 10;
    updatePage5UI(secili_goz);
  }
  else if (komut == "SRV_GUC_DOWN" || komut == "b_srv_guc_down" || komut == "GUC_DOWN" || komut == "b_guc_down") {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 5;
    aktif_sayfa = 5;
    if (edit_guc_seviyesi[secili_goz] > 10) edit_guc_seviyesi[secili_goz] -= 10;
    updatePage5UI(secili_goz);
  }
  else if (komut == "SRV_ID_UP" || komut == "b_srv_id_up" || komut == "ID_UP" || komut == "b_id_up") {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 5;
    aktif_sayfa = 5;
    int old_id = edit_kart_id[secili_goz];
    if (old_id < edit_max_goz_sayisi) {
      int new_id = old_id + 1;
      // ID collision swap in edit buffer: find target slot and swap
      for (int g = 1; g < MAX_GOZ; g++) {
        if (g != secili_goz && edit_kart_id[g] == new_id) {
          edit_kart_id[g] = old_id;
          DEBUG_PRINTLN("--> ID Swap Detected: Göz " + String(g) + " remapped to ID " + String(old_id));
        }
      }
      edit_kart_id[secili_goz] = new_id;
    }
    updatePage5UI(secili_goz);
  }
  else if (komut == "SRV_ID_DOWN" || komut == "b_srv_id_down" || komut == "ID_DOWN" || komut == "b_id_down") {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 5;
    aktif_sayfa = 5;
    int old_id = edit_kart_id[secili_goz];
    if (old_id > 1) {
      int new_id = old_id - 1;
      // ID collision swap in edit buffer: find target slot and swap
      for (int g = 1; g < MAX_GOZ; g++) {
        if (g != secili_goz && edit_kart_id[g] == new_id) {
          edit_kart_id[g] = old_id;
          DEBUG_PRINTLN("--> ID Swap Detected: Göz " + String(g) + " remapped to ID " + String(old_id));
        }
      }
      edit_kart_id[secili_goz] = new_id;
    }
    updatePage5UI(secili_goz);
  }
  else if (komut == "SRV_MAX_UP" || komut == "b_srv_max_up" || komut == "MAX_UP" || komut == "b_max_up") {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 5;
    aktif_sayfa = 5;
    if (edit_max_goz_sayisi < MAX_GOZ - 1) {
      edit_max_goz_sayisi++;
      bool duplicate = false;
      for (int g = 1; g < edit_max_goz_sayisi; g++) {
        if (edit_kart_id[g] == edit_kart_id[edit_max_goz_sayisi]) {
          duplicate = true;
          break;
        }
      }
      if (duplicate || edit_kart_id[edit_max_goz_sayisi] < 1 || edit_kart_id[edit_max_goz_sayisi] > edit_max_goz_sayisi) {
        edit_kart_id[edit_max_goz_sayisi] = edit_max_goz_sayisi;
      }
    }
    updatePage5UI(secili_goz);
  }
  else if (komut == "SRV_MAX_DOWN" || komut == "b_srv_max_down" || komut == "MAX_DOWN" || komut == "b_max_down") {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 5;
    aktif_sayfa = 5;
    if (edit_max_goz_sayisi > 1) edit_max_goz_sayisi--;
    updatePage5UI(secili_goz);
  }

  // =========================================================================
  // PAGE 6: SWEEP SERVICE SETTINGS (TANK-SCOPED)
  // =========================================================================
  else if (komut == "PAGE6_OPEN" || komut == "PAGE6") {
    current_service_page = 6;
    aktif_sayfa = 6;
    updatePage6UI(secili_goz);
  }
  else if (komut == "PAGE6_SWP_TOGGLE" || komut == "SWP_CFG_TOGGLE" || komut == "SWEEP_CFG_TOGGLE") {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 6;
    aktif_sayfa = 6;
    int g = secili_goz;
    edit_service_sweep[g].enabled = edit_service_sweep[g].enabled ? 0 : 1;
    updatePage6UI(g);
    DEBUG_PRINTLN("--> ESP32 Page 6 Göz " + String(g) + " Sweep Enabled=" + String(edit_service_sweep[g].enabled));
  }
  else if (komut == "SRV_SPAN_UP" || komut == "b_swp_span_up" || komut == "b_span_up" || komut == "SWP_SPAN_UP" || komut.startsWith("SET_SWP_SPAN:") || komut.startsWith("CMD_SET_SWP_SPAN:") || komut.startsWith("SET_SPAN:")) {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 6;
    aktif_sayfa = 6;
    int g = secili_goz;
    int colon = komut.indexOf(':');
    if (colon != -1) {
      int val = komut.substring(colon + 1).toInt();
      if (val >= 1 && val <= 4) edit_service_sweep[g].span_khz = val;
    } else {
      if (edit_service_sweep[g].span_khz < 4) edit_service_sweep[g].span_khz++;
    }
    updatePage6UI(g);
  }
  else if (komut == "SRV_SPAN_DOWN" || komut == "b_swp_span_do" || komut == "b_span_down" || komut == "SWP_SPAN_DOWN") {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 6;
    aktif_sayfa = 6;
    int g = secili_goz;
    if (edit_service_sweep[g].span_khz > 1) edit_service_sweep[g].span_khz--;
    updatePage6UI(g);
  }
  else if (komut == "SRV_PER_UP" || komut == "b_swp_per_up" || komut == "b_per_up" || komut == "SWP_PER_UP" || komut.startsWith("SET_SWP_PER:") || komut.startsWith("CMD_SET_SWP_PER:") || komut.startsWith("SET_PER:")) {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 6;
    aktif_sayfa = 6;
    int g = secili_goz;
    int colon = komut.indexOf(':');
    if (colon != -1) {
      int val = komut.substring(colon + 1).toInt();
      if (val >= 100 && val <= 1000) edit_service_sweep[g].period_ms = val;
    } else {
      if (edit_service_sweep[g].period_ms <= 900) edit_service_sweep[g].period_ms += 100;
    }
    updatePage6UI(g);
  }
  else if (komut == "SRV_PER_DOWN" || komut == "b_swp_per_down" || komut == "b_per_down" || komut == "SWP_PER_DOWN") {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 6;
    aktif_sayfa = 6;
    int g = secili_goz;
    if (edit_service_sweep[g].period_ms >= 200) edit_service_sweep[g].period_ms -= 100;
    updatePage6UI(g);
  }
  else if (komut == "SRV_STEP_UP" || komut == "b_swp_step_up" || komut == "b_step_up" || komut == "SWP_STEP_UP" || komut.startsWith("SET_STEP_INC:") || komut.startsWith("CMD_SET_STEP_INC:") || komut.startsWith("SET_SWP_STEP:")) {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 6;
    aktif_sayfa = 6;
    int g = secili_goz;
    int colon = komut.indexOf(':');
    if (colon != -1) {
      int val = komut.substring(colon + 1).toInt();
      if (val >= 1 && val <= 8) edit_service_sweep[g].step_increment = val;
    } else {
      if (edit_service_sweep[g].step_increment < 8) edit_service_sweep[g].step_increment++;
    }
    updatePage6UI(g);
  }
  else if (komut == "SRV_STEP_DOWN" || komut == "b_swp_step_do" || komut == "b_step_down" || komut == "SWP_STEP_DOWN") {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 6;
    aktif_sayfa = 6;
    int g = secili_goz;
    if (edit_service_sweep[g].step_increment > 1) edit_service_sweep[g].step_increment--;
    updatePage6UI(g);
  }

  // =========================================================================
  // PAGE 7 & PAGE 8: DEGAS SERVICE SETTINGS (TANK-SCOPED)
  // =========================================================================
  else if (komut == "PAGE7_OPEN" || komut == "PAGE7" || komut == "PAGE_DEGAS_OPEN" || komut == "PAGE_DEGAS") {
    current_service_page = 7;
    aktif_sayfa = 7;
    updatePage7UI(secili_goz);
  }
  else if (komut == "SRV_DDUR_UP" || komut == "b_deg_dur_up" || komut == "DEG_DUR_UP" || komut == "b_dur_up" || komut.startsWith("SET_DEG_DUR:")) {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 7;
    aktif_sayfa = 7;
    int g = secili_goz;
    if (komut.startsWith("SET_DEG_DUR:")) {
      int v = komut.substring(12).toInt();
      if (v >= 1 && v <= 120) edit_service_degas[g].duration_minutes = v;
    } else {
      if (edit_service_degas[g].duration_minutes < 120) edit_service_degas[g].duration_minutes++;
    }
    updatePage7UI(g);
  }
  else if (komut == "SRV_DDUR_DOWN" || komut == "b_deg_dur_down" || komut == "DEG_DUR_DOWN" || komut == "b_dur_down") {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 7;
    aktif_sayfa = 7;
    int g = secili_goz;
    if (edit_service_degas[g].duration_minutes > 1) edit_service_degas[g].duration_minutes--;
    updatePage7UI(g);
  }
  else if (komut == "SRV_DPOW_UP" || komut == "b_deg_pow_up" || komut == "DEG_PWR_UP" || komut == "b_pwr_up" || komut.startsWith("SET_DEG_PWR:")) {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 7;
    aktif_sayfa = 7;
    int g = secili_goz;
    if (komut.startsWith("SET_DEG_PWR:")) {
      int v = komut.substring(12).toInt();
      if (v >= 10 && v <= 100) edit_service_degas[g].power_pct = v;
    } else {
      if (edit_service_degas[g].power_pct <= 90) edit_service_degas[g].power_pct += 10;
    }
    updatePage7UI(g);
  }
  else if (komut == "SRV_DPOW_DOWN" || komut == "b_deg_pow_down" || komut == "DEG_PWR_DOWN" || komut == "b_pwr_down") {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 7;
    aktif_sayfa = 7;
    int g = secili_goz;
    if (edit_service_degas[g].power_pct >= 20) edit_service_degas[g].power_pct -= 10;
    updatePage7UI(g);
  }
  else if (komut == "SRV_DFREQ_UP" || komut == "b_deg_freq_up" || komut == "DEG_FRQ_UP" || komut == "b_frq_up" || komut.startsWith("SET_DEG_FRQ:")) {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 7;
    aktif_sayfa = 7;
    int g = secili_goz;
    if (komut.startsWith("SET_DEG_FRQ:")) {
      int v = komut.substring(12).toInt();
      if (v >= 28 && v <= 40) edit_service_degas[g].frequency_khz = v;
    } else {
      if (edit_service_degas[g].frequency_khz < 40) edit_service_degas[g].frequency_khz++;
    }
    updatePage7UI(g);
  }
  else if (komut == "SRV_DFREQ_DOWN" || komut == "b_deg_freq_do" || komut == "DEG_FRQ_DOWN" || komut == "b_frq_down") {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 7;
    aktif_sayfa = 7;
    int g = secili_goz;
    if (edit_service_degas[g].frequency_khz > 28) edit_service_degas[g].frequency_khz--;
    updatePage7UI(g);
  }
  else if (komut == "SRV_DPON_UP" || komut == "b_deg_pon_up" || komut == "DEG_ON_UP" || komut == "b_on_up" || komut.startsWith("SET_DEG_ON:")) {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 8;
    aktif_sayfa = 8;
    int g = secili_goz;
    if (komut.startsWith("SET_DEG_ON:")) {
      int v = komut.substring(11).toInt();
      if (v >= 100 && v <= 10000) edit_service_degas[g].pulse_on_ms = v;
    } else {
      if (edit_service_degas[g].pulse_on_ms <= 9900) edit_service_degas[g].pulse_on_ms += 100;
    }
    updatePage8UI(g);
  }
  else if (komut == "SRV_DPON_DOWN" || komut == "b_deg_pon_down" || komut == "DEG_ON_DOWN" || komut == "b_on_down") {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 8;
    aktif_sayfa = 8;
    int g = secili_goz;
    if (edit_service_degas[g].pulse_on_ms >= 200) edit_service_degas[g].pulse_on_ms -= 100;
    updatePage8UI(g);
  }
  else if (komut == "SRV_DPOFF_UP" || komut == "b_deg_poff_up" || komut == "DEG_OFF_UP" || komut == "b_off_up" || komut.startsWith("SET_DEG_OFF:")) {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 8;
    aktif_sayfa = 8;
    int g = secili_goz;
    if (komut.startsWith("SET_DEG_OFF:")) {
      int v = komut.substring(12).toInt();
      if (v == 0 || (v >= 100 && v <= 10000)) edit_service_degas[g].pulse_off_ms = v;
    } else {
      if (edit_service_degas[g].pulse_off_ms <= 9900) edit_service_degas[g].pulse_off_ms += 100;
    }
    updatePage8UI(g);
  }
  else if (komut == "SRV_DPOFF_DOWN" || komut == "b_deg_poff_do" || komut == "DEG_OFF_DOWN" || komut == "b_off_down") {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 8;
    aktif_sayfa = 8;
    int g = secili_goz;
    if (edit_service_degas[g].pulse_off_ms >= 100) edit_service_degas[g].pulse_off_ms -= 100;
    updatePage8UI(g);
  }
  else if (komut == "SRV_DTCTRL_TOG" || komut == "b_deg_tctrl" || komut == "DEG_TC_TOGGLE" || komut == "b_tc_toggle" || komut.startsWith("SET_DEG_TC:")) {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 8;
    aktif_sayfa = 8;
    int g = secili_goz;
    if (komut.startsWith("SET_DEG_TC:")) {
      int v = komut.substring(11).toInt();
      edit_service_degas[g].temp_ctrl = (v != 0) ? 1 : 0;
    } else {
      edit_service_degas[g].temp_ctrl = (edit_service_degas[g].temp_ctrl != 0) ? 0 : 1;
    }
    updatePage8UI(g);
  }
  else if (komut == "SRV_DTEMP_UP" || komut == "b_deg_temp_up" || komut == "DEG_TGT_UP" || komut == "b_tgt_up" || komut.startsWith("SET_DEG_TGT:")) {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 8;
    aktif_sayfa = 8;
    int g = secili_goz;
    if (edit_service_degas[g].temp_ctrl == 0) return;
    if (komut.startsWith("SET_DEG_TGT:")) {
      float v = komut.substring(12).toFloat();
      if (v >= 20.0f && v <= 90.0f) edit_service_degas[g].target_temp_c = v;
    } else {
      if (edit_service_degas[g].target_temp_c < 90.0f) edit_service_degas[g].target_temp_c += 1.0f;
    }
    updatePage8UI(g);
  }
  else if (komut == "SRV_DTEMP_DOWN" || komut == "b_deg_temp_do" || komut == "DEG_TGT_DOWN" || komut == "b_tgt_down") {
    if (!isProvisioningAllowed()) return;
    service_auth_time = millis();
    current_service_page = 8;
    aktif_sayfa = 8;
    int g = secili_goz;
    if (edit_service_degas[g].temp_ctrl == 0) return;
    if (komut.startsWith("SET_DEG_TGT:")) {
      float v = komut.substring(12).toFloat();
      if (v >= 20.0f && v <= 90.0f) edit_service_degas[g].target_temp_c = v;
    } else {
      if (edit_service_degas[g].target_temp_c > 20.0f) edit_service_degas[g].target_temp_c -= 1.0f;
    }
    updatePage8UI(g);
  }

  // =========================================================================
  // ATOMIC SERVICE SAVE (SRV_SAVE & PAGE-SPECIFIC ALIASES)
  // =========================================================================
  else if (komut == "SRV_SAVE" || komut == "PAGE5_SAVE" || komut == "PAGE6_SAVE" || komut == "PAGE7_SAVE" ||
           komut == "PAGE8_SAVE" || komut == "SRV_PAGE5_SAVE" || komut == "SWP_SAVE" ||
           komut == "SRV_SWEEP_SAVE" || komut == "SRV_DEGAS_SAVE" || komut == "SRV_SAVE_DEGAS") {
    if (!isProvisioningAllowed()) {
      nextionGonder("b_save.bco=" + String(NEXTION_COLOR_RED));
      g_bus_diag.tx_nack_count++;
      return;
    }

    // Validate DEGAS parameters across all tanks before commit
    for (int g = 1; g < MAX_GOZ; g++) {
      bool valid_degas = (edit_service_degas[g].duration_minutes >= 1 && edit_service_degas[g].duration_minutes <= 120) &&
                         (edit_service_degas[g].power_pct >= 10 && edit_service_degas[g].power_pct <= 100) &&
                         (edit_service_degas[g].frequency_khz >= 28 && edit_service_degas[g].frequency_khz <= 40) &&
                         (edit_service_degas[g].pulse_on_ms >= 100 && edit_service_degas[g].pulse_on_ms <= 10000) &&
                         (edit_service_degas[g].pulse_off_ms == 0 || (edit_service_degas[g].pulse_off_ms >= 100 && edit_service_degas[g].pulse_off_ms <= 10000)) &&
                         (edit_service_degas[g].temp_ctrl <= 1) &&
                         (edit_service_degas[g].target_temp_c >= 20.0f && edit_service_degas[g].target_temp_c <= 90.0f);
      if (!valid_degas) {
        nextionGonder("b_save.bco=" + String(NEXTION_COLOR_RED));
        g_bus_diag.tx_nack_count++;
        return;
      }
    }

    max_goz_sayisi = edit_max_goz_sayisi;

    int eski_goz = secili_goz;
    int hedef_yeni_goz = edit_kart_id[eski_goz];

    for (int g = 1; g < MAX_GOZ; g++) {
      int old_card_id = kart_id[g];
      int new_card_id = edit_kart_id[g];
      guc_seviyesi[g]  = edit_guc_seviyesi[g];
      service_sweep[g] = edit_service_sweep[g];
      service_degas[g] = edit_service_degas[g];

      if (new_card_id != old_card_id) {
        kart_id[g] = new_card_id;
        stm_bagli[g] = false;
        stm_son_veri_zamani[g] = 0;
        rs485Transmit("T" + String(old_card_id) + ":ASSIGN_ID=" + String(new_card_id) + "\n");
        stmSetIdBroadcast(new_card_id);
      }
      nvsKaydetGoz(g);
      sweepNvsKaydet(g);
      degasNvsKaydet(g);
    }

    if (secili_goz > max_goz_sayisi) {
      secili_goz = max_goz_sayisi;
    }
    if (secili_goz < 1) {
      secili_goz = 1;
    }
    initServiceEditBuffers();
    stmSetpointleriGonder();

    DEBUG_PRINTLN("--> SERVİS AYARLARI NVS'YE ATOMIK KAYDEDILDI (SRV_SAVE) - Seçili Göz: " + String(secili_goz));
    nextionGonder("b_save.bco=" + String(NEXTION_COLOR_GREEN));
    delay(600);
    nextionGonder("b_save.bco=" + String(NEXTION_COLOR_DEFAULT));
    switch (current_service_page) {
      case 5: updatePage5UI(secili_goz); break;
      case 6: updatePage6UI(secili_goz); break;
      case 7: updatePage7UI(secili_goz); break;
      case 8: updatePage8UI(secili_goz); break;
      default: updatePage5UI(secili_goz); break;
    }
    g_bus_diag.tx_ack_count++;
  }

  // =========================================================================
  // DIAGNOSTICS & HIL UTILITIES
  // =========================================================================
  else if (komut == "DIAG" || komut == "GET_DIAG") {
    String diag_str = "DIAG,valid:" + String(g_bus_diag.rx_valid_count) +
                       ",crc_err:" + String(g_bus_diag.rx_crc_error_count) +
                       ",malformed:" + String(g_bus_diag.rx_malformed_count) +
                       ",timeout:" + String(g_bus_diag.rx_timeout_count) +
                       ",dropped:" + String(g_bus_diag.rx_dropped_count) +
                       ",tx:" + String(g_bus_diag.tx_frame_count) +
                       ",ack:" + String(g_bus_diag.tx_ack_count) +
                       ",nack:" + String(g_bus_diag.tx_nack_count);
    DEBUG_PRINTLN("[ESP32_DIAG] " + diag_str);
  }
  else if (komut == "HIL_HEARTBEAT_OFF") {
    hil_heartbeat_active = false;
    DEBUG_PRINTLN("--> HIL_HEARTBEAT_OFF: Heartbeat paused for comm loss testing");
  }
  else if (komut == "HIL_HEARTBEAT_ON") {
    hil_heartbeat_active = true;
    sonHeartbeatZamani = millis();
    DEBUG_PRINTLN("--> HIL_HEARTBEAT_ON: Heartbeat resumed");
  }
}

bool hatOku(Stream &kaynak, String &tampon, uint8_t &ff_count, String &satir) {
  while (kaynak.available()) {
    uint8_t b = (uint8_t)kaynak.read();

    // 1. Standard \n delimiter (for terminal / pytest / rs485 streams)
    if (b == 0x0A) { // '\n'
      ff_count = 0;
      satir = tampon;
      satir.trim();
      tampon = "";
      if (satir.length() > 0) return true;
      continue;
    }

    // 2. Ignore \r and null byte (0x00 from Nextion prints ...,0)
    if (b == 0x0D || b == 0x00) {
      continue;
    }

    // 3. 0xFF terminator handling for Nextion (3x 0xFF)
    if (b == 0xFF) {
      ff_count++;
      if (ff_count >= 3) {
        ff_count = 0;
        String t_str = tampon;
        t_str.trim();

        // Check if multi-part Nextion command is still incomplete (e.g. prints "CMD_START|",0 -> prints t_set_sure.txt,0 -> ...)
        bool isIncomplete = (t_str == "CMD_START|") ||
                            (t_str.startsWith("CMD_START|") && (t_str.indexOf('|', 10) == -1 || t_str.endsWith("|"))) ||
                            (t_str == "P_SAVE|") ||
                            (t_str.startsWith("P_SAVE|") && (t_str.indexOf('|', 7) == -1 || t_str.endsWith("|")));
        if (isIncomplete) {
          continue;
        }

        if (t_str.length() > 0) {
          satir = t_str;
          tampon = "";
          return true;
        }
      }
      continue;
    } else {
      ff_count = 0;
    }

    // 4. Normal ASCII characters (32..126)
    if (b >= 32 && b <= 126) {
      tampon += (char)b;
    }
  }
  return false;
}

bool hatOku(Stream &kaynak, String &tampon, String &satir) {
  static uint8_t default_ff = 0;
  return hatOku(kaynak, tampon, default_ff, satir);
}

void loop() {
  // --- Servis Menusu Oturum Zaman Asimi Kontrolu ---
  if (g_service_authenticated && (millis() - service_auth_time > SERVICE_SESSION_TIMEOUT_MS)) {
    g_service_authenticated = false;
    DEBUG_PRINTLN("--> SERVIS OTURUMU ZAMAN ASIMINA UGRADI (g_service_authenticated = false)");
  }

  // --- Nextion'dan gelen komutlar ---
  String satirNextion;
  while (hatOku(Serial2, gelenMesaj, nextion_ff_count, satirNextion)) {
    if (satirNextion.length() > 0) {
      DEBUG_PRINTLN("[HMI->ESP] " + satirNextion);
      komutIsle(satirNextion);
    }
  }

  // --- PC (USB Debug) test komutları ---
  String satirUsb;
  while (hatOku(Serial, usbMesaj, usb_ff_count, satirUsb)) {
    if (satirUsb.length() > 0) {
      DEBUG_PRINTLN("[PC->ESP] " + satirUsb);
      if (satirUsb.indexOf("STAGE_ID") != -1 || satirUsb.indexOf("ASSIGN_ID") != -1 ||
          satirUsb.indexOf("RESET_ID") != -1 || satirUsb.indexOf("DISCOVER") != -1 ||
          satirUsb.indexOf("SET_ID") != -1) {
        if (!isProvisioningAllowed()) {
          DEBUG_PRINTLN("--> [PC->ESP] PROVISIONING REJECTED BY ESP32 INTERLOCK");
          continue;
        }
      }

      if (isBusKomut(satirUsb)) {
        rs485Transmit(satirUsb + "\n");
        DEBUG_PRINTLN("[PC->STM] " + satirUsb);
      } else if (satirUsb.startsWith("STAT,") || satirUsb.startsWith("ACK:") || satirUsb.startsWith("ACK,") || satirUsb.startsWith("ERR:") || satirUsb.startsWith("NACK")) {
        stmTelemetryIsle(satirUsb);
      } else {
        komutIsle(satirUsb);
      }
    }
  }

  // --- STM32'den gelen telemetri ---
  String satirStm;
  while (hatOku(Serial1, stmMesaj, stm_ff_count, satirStm)) {
    if (satirStm.length() > 0) {
      DEBUG_PRINTLN("[STM->ESP] " + satirStm);
      stmTelemetryIsle(satirStm);
    }
  }

  // --- Bağlantı zaman aşımı kontrolü (her tank bağımsız) ---
  for (int i = 1; i < MAX_GOZ; i++) {
    if (stm_bagli[i] && (millis() - stm_son_veri_zamani[i] > STM_BAGLANTI_TIMEOUT)) {
      stm_bagli[i] = false;
      makine_calisiyor[i] = false;
      degas_active[i] = false;
      degas_armed[i] = false;
      stm_relay[i] = 0;
      kalan_saniye[i] = 0;
      durum_metni[i] = "Kart Yok!";
      if (i == secili_goz && aktif_sayfa == 0) {
        nextionGonder("t_durum.txt=\"Kart Yok!\"");
        nextionGonder("b_deg.bco=" + String(NEXTION_COLOR_DEFAULT));
      }
    }
  }

  // HIL watchdog debug
  if (millis() - hilWdtDebugZamani > STM_BAGLANTI_TIMEOUT) {
    hilWdtDebugZamani = millis();
    for (int i = 1; i < MAX_GOZ; i++) {
      if (stm_son_veri_zamani[i] == 0) continue;
      unsigned long age_ms = millis() - stm_son_veri_zamani[i];
      DEBUG_PRINTLN("DEBUG_ESP32: WDT tank=" + String(i) + " connected=" + String(isKartBagli(i) ? 1 : 0) + " age_ms=" + String(age_ms));
    }
  }

  // --- Periodic Heartbeat ---
  if (hil_heartbeat_active && (millis() - sonHeartbeatZamani >= 1000)) {
    sonHeartbeatZamani = millis();
    for (int i = 1; i < MAX_GOZ; i++) {
      if (stm_bagli[i] && (makine_calisiyor[i] || degas_active[i])) {
        int target_id = (kart_id[i] >= 1) ? kart_id[i] : i;
        rs485Transmit("T" + String(target_id) + ":HEARTBEAT\n");
      }
    }
  }

  // --- 1-Second Timer Countdown ---
  if (millis() - sonSaniyeZamani >= 1000) {
    sonSaniyeZamani = millis();
    for (int i = 1; i < MAX_GOZ; i++) {
      if ((makine_calisiyor[i] || degas_active[i]) && kalan_saniye[i] > 0) {
        kalan_saniye[i]--;
        if (kalan_saniye[i] == 0) {
          makine_calisiyor[i] = false;
          degas_active[i] = false;
          degas_armed[i] = false;
          runtime_sweep[i] = false;
          durum_metni[i] = "YIKAMA TAMAMLANDI!";
          stmSweep(false);
          stmStop();
          if (i == secili_goz && aktif_sayfa == 0) {
            nextionGonder("b_deg.bco=" + String(NEXTION_COLOR_DEFAULT));
            nextionGonder("t_durum.txt=\"YIKAMA TAMAMLANDI!\"");
            updatePage0ButtonColors();
          }
        }
      }
    }
  }

  // --- Periodic UI Refresh (Page 0 Only) ---
  if (aktif_sayfa == 0 && (millis() - sonGuncellemeZamani > 1000)) {
    sonGuncellemeZamani = millis();

    int kalan_dk = kalan_saniye[secili_goz] / 60;
    int kalan_sn = kalan_saniye[secili_goz] % 60;
    char zamanBuf[6];
    snprintf(zamanBuf, sizeof(zamanBuf), "%02d:%02d", kalan_dk, kalan_sn);

    bool pt100_hata = (stm_fault[secili_goz] & (FAULT_PT100_OPEN_BIT | FAULT_PT100_SHORT_BIT)) != 0;
    String sicaklikMetni = pt100_hata ? "--.-" : String(anlik_sicaklik[secili_goz], 1);

    nextionGonder(String("t_kalan.txt=\"") + zamanBuf + "\"");
    nextionGonder(String("t_kalan_sure.txt=\"") + zamanBuf + "\"");
    nextionGonder("t_anlik_sic.txt=\"" + sicaklikMetni + "\"");
    nextionGonder("t_durum.txt=\"" + durum_metni[secili_goz] + "\"");
    nextionGonder("b_goz_sec.txt=\"Goz: " + String(secili_goz) + "\""); 
    nextionGonder("b_goz.txt=\"Goz: " + String(secili_goz) + "\""); 

    String freqMetni = (stm_freq[secili_goz] == 40) ? "40k" : "28k";
    nextionGonder("b_freq.txt=\"" + freqMetni + "\"");
    nextionGonder("b_frq.txt=\"" + freqMetni + "\"");
    updatePage0ButtonColors();
  }
}