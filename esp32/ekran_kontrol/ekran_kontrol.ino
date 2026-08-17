#include <Arduino.h>
#include <Preferences.h>
#include "esp_timer.h"

// --- NEXTION (HMI) UART ---
#define RXD2 16
#define TXD2 17

// --- NEXTION COLOR CONSTANTS (TASK 1 HARDENING) ---
#define NEXTION_COLOR_RED     63488
#define NEXTION_COLOR_GREEN   2016
#define NEXTION_COLOR_DEFAULT 50712

// --- BUS DIAGNOSTICS ARCHITECTURE (TASK 2) ---
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
// GPIO26/27 kullanilmaz: ESP32-S3-N16R8 uzerinde bu pinler SPI flash hattina (SPICS1/SPIHD) sabittir.
#define STM_RXD 18
#define STM_TXD 8
#define RS485_DE_PIN 5
#define STM_BAUD 115200 // huart3 (STM32) sabit 115200; ESP32 tarafi bununla eslesmeli
#define STM_BAGLANTI_TIMEOUT 3000

void rs485Transmit(const String &msg) {
  digitalWrite(RS485_DE_PIN, HIGH);
  delayMicroseconds(10);
  Serial1.print(msg);
  Serial1.flush(); // Waits for hardware TX FIFO and shift register to flush completely
  delayMicroseconds(5);
  digitalWrite(RS485_DE_PIN, LOW);
}

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
bool g_service_authenticated = false;
unsigned long service_auth_time = 0;
const unsigned long SERVICE_SESSION_TIMEOUT_MS = 300000; // 5 minute auth timeout

int guc_seviyesi = 50;       
int kart_id = 1;             
int max_goz_sayisi = 3;      
int step_increment = 4;
int swp_span = 2;
int swp_per = 400;

// ==========================================
// 2. HER GÖZÜN BAĞIMSIZ BEYNİ (ARRAY/DİZİLER)
// ==========================================
struct ESP32DegasConfig {
  uint16_t duration_minutes = 15;
  uint8_t  power_pct = 100;
  uint8_t  frequency_khz = 28;
  uint16_t pulse_on_ms = 1000;
  uint16_t pulse_off_ms = 500;
  uint8_t  temp_ctrl = 0;
  float    target_temp_c = 50.0;
} service_degas[MAX_GOZ];

Preferences degasPrefs;

bool makine_calisiyor[MAX_GOZ];
bool degas_armed[MAX_GOZ];
bool degas_active[MAX_GOZ];
String durum_metni[MAX_GOZ];
int hedef_sure[MAX_GOZ];      
int hedef_sicaklik[MAX_GOZ];  
int kalan_saniye[MAX_GOZ];     // STM32'den gelen gerçek kalan süre (sn)
float anlik_sicaklik[MAX_GOZ]; // STM32'den gelen gerçek sıcaklık (temp_x10/10.0)

// --- STM32 TELEMETRİ DURUMU (her göz/tank icin bagimsiz; multi-drop hatta N slave ayni ESP32'ye baglidir) ---
int stm_fault[MAX_GOZ];
int stm_relay[MAX_GOZ];
int stm_pwr[MAX_GOZ];
int stm_freq[MAX_GOZ];
int stm_prov_state[MAX_GOZ];
bool stm_bagli[MAX_GOZ];
unsigned long stm_son_veri_zamani[MAX_GOZ];

unsigned long sonGuncellemeZamani = 0;
unsigned long hilWdtDebugZamani = 0;  // HIL_DEEP_DEBUG
unsigned long sonHeartbeatZamani = 0; // Periodic heartbeat (1000ms) for STM32 RX silence watchdog
bool hil_heartbeat_active = true;    // Production default: enabled (controllable via HIL_HEARTBEAT_OFF/ON)

String gelenMesaj = "";
String stmMesaj = "";
String usbMesaj = "";  // HIL_TEST_MOD: PC (COM10, USB Debug) uzerinden gelen HIL komut arabellegi

Preferences prefs;

// Requirement 2 (Layer 1): ESP32 Master Interlock - Check if any tank is currently running
bool isAnyTankRunning() {
  for (int i = 1; i < MAX_GOZ; i++) {
    if (stm_bagli[i] && (makine_calisiyor[i] || degas_active[i] || stm_relay[i] != 0)) {
      return true;
    }
  }
  return false;
}

// Requirement 1 & Requirement 2 Interlock Evaluator
bool isProvisioningAllowed() {
  if (!g_service_authenticated) {
    Serial.println("--> HATA: SERVIS YETKILENDIRMESI GEREKLI (g_service_authenticated == false)");
    return false;
  }
  if (isAnyTankRunning()) {
    Serial.println("--> HATA: CALISAN TANK VAR! PROVISIONING KILITLI (SYS_MODE_RUNNING INTERLOCK)");
    return false;
  }
  return true;
}

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
  step_increment = prefs.getInt("step_inc", 4);
  swp_span = prefs.getInt("swp_span", 2);
  swp_per = prefs.getInt("swp_per", 400);
  prefs.end();

  // HIL_DEEP_DEBUG: dump every NVS key read back, so the host can confirm persisted values loaded correctly
  for (int i = 1; i <= 3; i++) {
    Serial.println("DEBUG_ESP32: NVS_READ key=pS" + String(i) + " val=" + String(p_sure[i]));
    Serial.println("DEBUG_ESP32: NVS_READ key=pT" + String(i) + " val=" + String(p_sicaklik[i]));
  }
  Serial.println("DEBUG_ESP32: NVS_READ key=guc val=" + String(guc_seviyesi));
  Serial.println("DEBUG_ESP32: NVS_READ key=kartid val=" + String(kart_id));
  Serial.println("DEBUG_ESP32: NVS_READ key=maxgoz val=" + String(max_goz_sayisi));
  Serial.println("DEBUG_ESP32: NVS_READ key=step_inc val=" + String(step_increment));
  Serial.println("DEBUG_ESP32: NVS_READ key=swp_span val=" + String(swp_span));
  Serial.println("DEBUG_ESP32: NVS_READ key=swp_per val=" + String(swp_per));
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
  prefs.putInt("step_inc", step_increment);
  prefs.putInt("swp_span", swp_span);
  prefs.putInt("swp_per", swp_per);
  prefs.end();

  // HIL_DEEP_DEBUG: confirm what was actually written this call, for NVS round-trip verification
  for (int i = 1; i <= 3; i++) {
    Serial.println("DEBUG_ESP32: NVS_WRITE key=pS" + String(i) + " val=" + String(p_sure[i]));
    Serial.println("DEBUG_ESP32: NVS_WRITE key=pT" + String(i) + " val=" + String(p_sicaklik[i]));
  }
  Serial.println("DEBUG_ESP32: NVS_WRITE key=guc val=" + String(guc_seviyesi));
  Serial.println("DEBUG_ESP32: NVS_WRITE key=kartid val=" + String(kart_id));
  Serial.println("DEBUG_ESP32: NVS_WRITE key=maxgoz val=" + String(max_goz_sayisi));
  Serial.println("DEBUG_ESP32: NVS_WRITE key=step_inc val=" + String(step_increment));
  Serial.println("DEBUG_ESP32: NVS_WRITE key=swp_span val=" + String(swp_span));
  Serial.println("DEBUG_ESP32: NVS_WRITE key=swp_per val=" + String(swp_per));
}

// --- DEGAS SERVICE NVS PERSISTENCE (service_degas) ---
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

void updateDegasPageUI(int g) {
  if (g < 1 || g >= MAX_GOZ) return;
  nextionGonder("t_deg_goz.txt=\"Goz: " + String(g) + "\"");
  nextionGonder("t_deg_dur.txt=\"" + String(service_degas[g].duration_minutes) + "\"");
  nextionGonder("t_deg_pwr.txt=\"" + String(service_degas[g].power_pct) + "\"");
  nextionGonder("t_deg_frq.txt=\"" + String(service_degas[g].frequency_khz) + "\"");
  nextionGonder("t_deg_on.txt=\"" + String(service_degas[g].pulse_on_ms) + "\"");
  nextionGonder("t_deg_off.txt=\"" + String(service_degas[g].pulse_off_ms) + "\"");
  nextionGonder("t_deg_tc.txt=\"" + String(service_degas[g].temp_ctrl != 0 ? "ON" : "OFF") + "\"");
  if (service_degas[g].temp_ctrl != 0) {
    nextionGonder("t_deg_tgt.txt=\"" + String((int)service_degas[g].target_temp_c) + "\"");
  } else {
    nextionGonder("t_deg_tgt.txt=\"--\"");
  }
}

void disarmDegasIfArmed(int g) {
  if (g >= 1 && g < MAX_GOZ) {
    if (degas_armed[g] && !degas_active[g]) {
      degas_armed[g] = false;
      if (g == secili_goz) {
        nextionGonder("b_degas.bco=" + String(NEXTION_COLOR_DEFAULT));
      }
      Serial.println("--> ESP32: DEGAS selection intent disarmed on normal parameter edit.");
    }
  }
}

// ==========================================
// Phase 5.2 NVS PROVISIONING & WAL REGISTRY
// ==========================================
Preferences provPrefs;
Preferences walPrefs;

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

// Helper function: generates deterministic, collision-resistant NVS keys strictly <= 15 chars (NVS_KEY_NAME_MAX_SIZE)
String getProvNvsKey(String uid24, const char* suffix) {
  String s = String(suffix);
  int maxUidLen = 15 - s.length();
  if ((int)uid24.length() > maxUidLen) {
    return uid24.substring(uid24.length() - maxUidLen) + s;
  }
  return uid24 + s;
}

// --- NVS Registry ("eagle_prov") Functions ---
void provNvsKaydet(String uid24, int tankId, int state) {
  provPrefs.begin("eagle_prov", false);
  provPrefs.putInt(getProvNvsKey(uid24, "_id").c_str(), tankId);
  provPrefs.putInt(getProvNvsKey(uid24, "_st").c_str(), state);
  provPrefs.end();

  Serial.println("DEBUG_ESP32: PROV_REGISTRY_SAVE UID=" + uid24 + " TankID=" + String(tankId) + " State=" + String(state));
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

  Serial.println("DEBUG_ESP32: PROV_REGISTRY_DELETE UID=" + uid24);
}

// --- Write-Ahead Logging ("eagle_prov_wal") Functions ---
void walYaz(int step, String uid24, int proposedId) {
  walPrefs.begin("eagle_prov_wal", false);
  walPrefs.putInt("step", step);
  walPrefs.putString("uid", uid24);
  walPrefs.putInt("id", proposedId);
  walPrefs.end();

  Serial.println("DEBUG_ESP32: WAL_WRITE step=" + String(step) + " uid=" + uid24 + " proposedId=" + String(proposedId));
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

  Serial.println("DEBUG_ESP32: WAL_CLEARED");
}

void walKurtar() {
  int step = 0;
  String uid = "";
  int proposedId = 0;

  if (walOku(step, uid, proposedId)) {
    Serial.println("--> WAL RECOVERY DETECTED! Uncommitted transaction: step=" + String(step) + " uid=" + uid + " id=" + String(proposedId));
    if (step == WAL_STEP_STAGING_PENDING) {
      Serial.println("--> WAL RECOVERY: Aborting unconfirmed staging transaction for UID=" + uid);
      rs485Transmit("T0:CANCEL_STAGE\n");
      walTemizle();
    } else if (step == WAL_STEP_COMMIT_PENDING) {
      Serial.println("--> WAL RECOVERY: Retrying commit transaction for UID=" + uid + " ID=" + String(proposedId));
      rs485Transmit("T0:ASSIGN_ID:" + String(proposedId) + ":" + uid + "\n");
      provNvsKaydet(uid, proposedId, PROV_STATE_ACTIVE);
      walTemizle();
    }
  }
}

// ==========================================
// 3.1 DISCOVERY & ATOMIC SWAP ORCHESTRATOR
// ==========================================
int discoverNodes(uint16_t seedRnd = 0) {
  if (!isProvisioningAllowed()) return 0;

  String cmd = (seedRnd > 0) ? ("T0:DISCOVER:" + String(seedRnd, HEX) + "\n") : "T0:DISCOVER\n";
  rs485Transmit(cmd);
  Serial.println("[ESP->STM] " + cmd);

  unsigned long start = millis();
  int found = 0;
  while (millis() - start < 600) {
    String stmLine;
    if (hatOku(Serial1, stmMesaj, stmLine)) {
      if (stmLine.startsWith("DISCOVER_ACK,")) {
        int p1 = stmLine.indexOf(',');
        int p2 = stmLine.indexOf(',', p1 + 1);
        if (p1 != -1 && p2 != -1) {
          String uid = stmLine.substring(p2 + 1);
          uid.trim();
          if (uid.length() == 24) {
            found++;
            Serial.println("--> [DISCOVERY] Found UNCOMMISSIONED node UID=" + uid);
          }
        }
      }
    }
  }
  return found;
}

bool executeAtomicSwap(String uidA, int oldIdA, int targetIdA, String uidB, int oldIdB, int targetIdB) {
  if (!isProvisioningAllowed()) {
    Serial.println("--> [SWAP] ERROR: Provisioning interlock locked");
    return false;
  }

  Serial.println("--> [SWAP] Initiating Atomic Swap: A(" + uidA + " ID " + String(oldIdA) + "->" + String(targetIdA) + ") <-> B(" + uidB + " ID " + String(oldIdB) + "->" + String(targetIdB) + ")");

  // Step 1: Stage Card A (ID -> STAGING / ID=0)
  walYaz(WAL_STEP_STAGING_PENDING, uidA, 0);
  rs485Transmit("T" + String(oldIdA) + ":STAGE_ID:" + uidA + "\n");
  delay(100);

  // Step 2: Re-assign Card B (oldIdB -> targetIdB)
  walYaz(WAL_STEP_COMMIT_PENDING, uidB, targetIdB);
  rs485Transmit("T" + String(oldIdB) + ":ASSIGN_ID:" + String(targetIdB) + ":" + uidB + "\n");
  provNvsKaydet(uidB, targetIdB, PROV_STATE_ACTIVE);
  delay(150);

  // Step 3: Re-assign Card A from STAGING (ID 0 -> targetIdA)
  walYaz(WAL_STEP_COMMIT_PENDING, uidA, targetIdA);
  rs485Transmit("T0:ASSIGN_ID:" + String(targetIdA) + ":" + uidA + "\n");
  provNvsKaydet(uidA, targetIdA, PROV_STATE_ACTIVE);
  delay(150);

  // Step 4: Clear WAL transaction
  walTemizle();
  Serial.println("--> [SWAP] SUCCESS: Atomic Swap complete!");
  return true;
}

// ==========================================
// 4. STM32 UART TX (KOMUT GÖNDERME)
// ==========================================
// Multi-drop bus: her komut "T<mevcut_goz>:" adresiyle gonderilir; sadece o ID'ye
// sahip STM32 karti komutu isler, digerleri sessizce yok sayar (bkz. esp32_uart.c).
void stmGonder(String komut) {
  String adresli = "T" + String(secili_goz) + ":" + komut + "\n";
  rs485Transmit(adresli);
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

void stmSetFreq(int freq) {
  stmGonder("SET_FREQ:" + String(freq) + "\n");
}

void stmSweep(bool enabled) {
  if (enabled) {
    stmGonder("SWEEP:ON\n");
  } else {
    stmGonder("SWEEP:OFF\n");
  }
}

void stmSetStepInc(int inc) {
  stmGonder("SET_STEP_INC:" + String(inc) + "\n");
}

void stmSetSwpSpan(int span) {
  stmGonder("SET_SWP_SPAN:" + String(span) + "\n");
}

void stmSetSwpPer(int per) {
  stmGonder("SET_SWP_PER:" + String(per) + "\n");
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
  rs485Transmit(adresli);
  String log = adresli;
  log.trim();
  Serial.println("[ESP->STM] " + log);
}

// Aktif gözün güncel setpointlerini STM32'ye gönderir (boot / reconnect / seçim değişimi)
void stmSetpointleriGonder() {
  stmSetTime(hedef_sure[secili_goz]);
  stmSetTemp(hedef_sicaklik[secili_goz]);
  stmSetPower(guc_seviyesi);
  stmSetStepInc(step_increment);
  stmSetSwpSpan(swp_span);
  stmSetSwpPer(swp_per);
}

// HIL_TEST_MOD: "T<digit(s)>:" adres onekiyle baslayan hatlari STM32 bus komutu olarak tanir
// (bkz. esp32_uart.c ProcessLine), boylece PC bunlari dogrudan Serial1'e iletilmek uzere
// COM10 (USB Debug) uzerinden gonderebilir; digerleri HMI komut seti (komutIsle) olarak islenir.
bool isBusKomut(const String &s) {
  if (s.length() < 3 || s.charAt(0) != 'T') return false;
  int colonIdx = s.indexOf(':');
  return (colonIdx > 1);
}

// ==========================================
// STM32 -> ESP32: "STAT,<TankID>,<Mode>,<rem_sec>,<temp_x10>,<relay>,<pwr>,<freq>,<fault>,<prov>,<sweep>"
// Gelen veri HER ZAMAN kendi Tank ID'sinin dizisine yazilir (10 tank da arka planda
// izlenir); Nextion ekran guncellemesi ise SADECE TankID, o an secili_goz (mevcut_goz)
// ile eslesirse yapilir.
void stmTelemetryIsle(String satir) {
  if (satir.startsWith("ERR:") || satir.startsWith("NACK")) {
    Serial.println("--> STM32 REJECTION: " + satir);
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
    Serial.println("--> STM32 ACK: " + satir);
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
  if (p1 == -1 || p2 == -1 || p3 == -1 || p4 == -1 || p5 == -1 || p6 == -1 || p7 == -1 || p8 == -1) return; // hatalı çerçeve

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

  if (tank_id < 1 || tank_id >= MAX_GOZ) return; // bozuk/gecersiz Tank ID -> dizi sinirlari disi erisim engellenir

  int g = tank_id;
  kalan_saniye[g] = rem_sec;
  anlik_sicaklik[g] = temp_x10 / 10.0;
  stm_relay[g] = relay;
  stm_pwr[g] = pwr;
  stm_freq[g] = freq;
  stm_fault[g] = fault;
  stm_prov_state[g] = prov_st;
  makine_calisiyor[g] = (mode_str == "RUNNING");
  bool was_degas = degas_active[g];
  degas_active[g] = (mode_str == "DEGAS");

  if (!degas_active[g] && was_degas) {
    degas_armed[g] = false;
    if (g == secili_goz) {
      nextionGonder("b_degas.bco=" + String(NEXTION_COLOR_DEFAULT));
    }
  }

  bool yeniden_baglandi = !stm_bagli[g];
  stm_bagli[g] = true;
  stm_son_veri_zamani[g] = millis();

  if (fault > 0) {
    durum_metni[g] = "HATA! KOD:" + String(fault);
    degas_active[g] = false;
    degas_armed[g] = false;
    if (g == secili_goz) {
      nextionGonder("b_degas.bco=" + String(NEXTION_COLOR_DEFAULT));
    }
  } else if (mode_str == "RUNNING") {
    durum_metni[g] = "YIKAMA DEVAM EDIYOR...";
  } else if (mode_str == "DEGAS") {
    durum_metni[g] = "DEGAS DEVAM EDIYOR...";
  } else if (rem_sec <= 0 && (hedef_sure[g] > 0 || was_degas)) {
    durum_metni[g] = "YIKAMA TAMAMLANDI!";
  } else {
    durum_metni[g] = "SISTEM BEKLEMEDE";
  }

  // --- HMI Durum Senkronizasyonu: sadece ekranda gosterilen goz icin aninda gonder ---
  if (g == secili_goz) {
    const char *hmi_durum = (mode_str == "RUNNING") ? "Calisiyor"
                          : (mode_str == "DEGAS")   ? "Degas"
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

  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, LOW); // RX Mode initial state

  // Zero-cross simulator: GPIO4'te esp_timer ile surekli 100Hz kare dalga (LEDC kullanilmiyor)
  zcSimBaslat();

  for(int i=0; i<MAX_GOZ; i++) {
    makine_calisiyor[i] = false;
    degas_armed[i] = false;
    degas_active[i] = false;
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
  }
  
  nvsYukle();
  degasNvsYukle();
  walKurtar();

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

  // --- DEGAS MODU SEÇİMİ (Page 0) ---
  else if (komut == "CMD_DEGAS_SEL" || komut == "CMD_DEGAS_SELECT" || komut == "DEGAS_SEL") {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) {
      Serial.println("--> DEGAS SECIMI REDDEDILDI: Yıkama/DEGAS aktif!");
      return;
    }
    degas_armed[secili_goz] = !degas_armed[secili_goz];
    if (degas_armed[secili_goz]) {
      stmSweep(false); // DEGAS ve Sweep kesinlikle birbirini dislar
      nextionGonder("b_degas.bco=" + String(NEXTION_COLOR_GREEN));
      durum_metni[secili_goz] = "DEGAS SECILDI. START BEKLENIYOR";
      Serial.println("--> ESP32: GÖZ " + String(secili_goz) + " DEGAS ARMED!");
    } else {
      nextionGonder("b_degas.bco=" + String(NEXTION_COLOR_DEFAULT));
      durum_metni[secili_goz] = "SISTEM BEKLEMEDE";
      Serial.println("--> ESP32: GÖZ " + String(secili_goz) + " DEGAS DISARMED.");
    }
  }
  else if (komut == "CMD_DEGAS_DESELECT" || komut == "DEGAS_DESEL") {
    degas_armed[secili_goz] = false;
    nextionGonder("b_degas.bco=" + String(NEXTION_COLOR_DEFAULT));
    durum_metni[secili_goz] = "SISTEM BEKLEMEDE";
    Serial.println("--> ESP32: GÖZ " + String(secili_goz) + " DEGAS DESELECTED.");
  }

  // --- HIZLI PROGRAM (FP) (Page 0) ---
  else if (komut == "P_HIZLI") {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) {
      Serial.println("--> P_HIZLI REDDEDILDI: ISLEM AKTIF!");
      return;
    }
    if (baslatmaEngelliMi()) return;

    if (degas_armed[secili_goz]) {
      durum_metni[secili_goz] = "DEGAS DEVAM EDIYOR...";
      int g = secili_goz;
      String snapCmd = "START_DEGAS:" + String(service_degas[g].duration_minutes) +
                       ":" + String(service_degas[g].power_pct) +
                       ":" + String(service_degas[g].frequency_khz) +
                       ":" + String(service_degas[g].pulse_on_ms) +
                       ":" + String(service_degas[g].pulse_off_ms) +
                       ":" + String(service_degas[g].temp_ctrl) +
                       ":" + String(service_degas[g].target_temp_c, 1);
      Serial.println("--> ESP32: GÖZ " + String(g) + " DEGAS START Frame: " + snapCmd);
      stmGonder(snapCmd + "\n");
      return;
    }

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
  else if (komut.startsWith("CMD_START|") || komut == "CMD_START") {
    if (degas_active[secili_goz]) {
      Serial.println("--> START REDDEDILDI: DEGAS AKTIF!");
      return;
    }
    if (baslatmaEngelliMi()) return;

    if (degas_armed[secili_goz]) {
      durum_metni[secili_goz] = "DEGAS DEVAM EDIYOR...";
      int g = secili_goz;
      String snapCmd = "START_DEGAS:" + String(service_degas[g].duration_minutes) +
                       ":" + String(service_degas[g].power_pct) +
                       ":" + String(service_degas[g].frequency_khz) +
                       ":" + String(service_degas[g].pulse_on_ms) +
                       ":" + String(service_degas[g].pulse_off_ms) +
                       ":" + String(service_degas[g].temp_ctrl) +
                       ":" + String(service_degas[g].target_temp_c, 1);
      Serial.println("--> DEGAS START! GÖZ: " + String(g) + " Frame: " + snapCmd);
      stmGonder(snapCmd + "\n");
      return;
    }

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
    } else {
      /* Pipe'siz CMD_START */
      makine_calisiyor[secili_goz] = true;
      durum_metni[secili_goz] = "YIKAMA DEVAM EDIYOR...";
      stmStart();
    }
  }
  
  // --- DURDUR (Page 0) ---
  else if (komut == "CMD_STOP") {
    makine_calisiyor[secili_goz] = false;
    degas_active[secili_goz] = false;
    degas_armed[secili_goz] = false;
    nextionGonder("b_degas.bco=" + String(NEXTION_COLOR_DEFAULT));
    durum_metni[secili_goz] = "SISTEM DURDURULDU";
    Serial.println("--> MOTOR STOP! GÖZ: " + String(secili_goz) + " Durduruldu.");
    stmStop(); // fault-ack olarak da çalışır
  }

  // --- FREKANS SWEEP ---
  else if (komut == "CMD_SWEEP_ON" || komut == "CMD_SWEEP|ON") {
    if (degas_active[secili_goz]) {
      Serial.println("--> SWEEP LOCKED: DEGAS IS ACTIVE!");
      return;
    }
    if (degas_armed[secili_goz]) {
      degas_armed[secili_goz] = false;
      nextionGonder("b_degas.bco=" + String(NEXTION_COLOR_DEFAULT));
    }
    stmSweep(true);
    Serial.println("--> SWEEP ON! GOZ: " + String(secili_goz) + " | ±2 kHz | 400 ms");
  }
  else if (komut == "CMD_SWEEP_OFF" || komut == "CMD_SWEEP|OFF") {
    stmSweep(false);
    Serial.println("--> SWEEP OFF! GOZ: " + String(secili_goz));
  }

  // --- FREKANS SEÇİMİ (28 kHz / 40 kHz) ---
  else if (komut.startsWith("CMD_FREQ|") || komut.startsWith("SET_FREQ|")) {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) {
      Serial.println("--> FREQ SELECTION LOCKED: PROCESS IS ACTIVE!");
      return;
    }
    if (degas_armed[secili_goz]) {
      degas_armed[secili_goz] = false;
      nextionGonder("b_degas.bco=" + String(NEXTION_COLOR_DEFAULT));
    }
    int pipe = komut.indexOf('|');
    if (pipe != -1) {
      int freq = komut.substring(pipe + 1).toInt();
      stmSetFreq(freq);
      Serial.println("--> FREKANS DEĞİŞTİRİLDİ: " + String(freq) + " kHz (Göz: " + String(secili_goz) + ")");
    }
  }

  // --- PROGRAM SEÇİMLERİ (Page 0) - Sayfa değişimi YOK, Sadece veri yükler ---
  else if (komut == "P1_SEL" || komut == "P2_SEL" || komut == "P3_SEL") {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) {
      Serial.println("--> RECIPE SELECTION LOCKED: PROCESS IS ACTIVE!");
      return;
    }
    if (baslatmaEngelliMi()) return;

    if (degas_armed[secili_goz]) {
      degas_armed[secili_goz] = false;
      nextionGonder("b_degas.bco=" + String(NEXTION_COLOR_DEFAULT));
    }

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
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) {
      Serial.println("--> RECIPE EDITING LOCKED: PROCESS IS ACTIVE!");
      return;
    }
    duzenlenen_program = (komut == "EDIT_P1") ? 1 : ((komut == "EDIT_P2") ? 2 : 3);
    
    // Page 2'deki yazıları Global P Hafızasındaki değerlerle doldur
    nextionGonder("t0.txt=\"PROGRAM P" + String(duzenlenen_program) + "\"");
    nextionGonder(String("t_set_sure.txt=\"") + (p_sure[duzenlenen_program] < 10 ? "0" : "") + String(p_sure[duzenlenen_program]) + "\"");
    nextionGonder("t_set_sic.txt=\"" + String(p_sicaklik[duzenlenen_program]) + "\"");
    
    Serial.println("--> ESP32: Page 2'de P" + String(duzenlenen_program) + " düzenleniyor.");
  }

  // --- PROGRAM KAYDETME (Page 2) ---
  else if (komut.startsWith("P_SAVE|")) {
    if (!isProvisioningAllowed()) {
      Serial.println("--> AUTH REJECTED: SERVICE PIN REQUIRED FOR RECIPE SAVE");
      return;
    }
    if (degas_active[secili_goz] || makine_calisiyor[secili_goz]) {
      Serial.println("--> RECIPE SAVE LOCKED: PROCESS IS ACTIVE!");
      return;
    }
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
        nextionGonder("b_save.bco=" + String(NEXTION_COLOR_GREEN));
        delay(400);
        nextionGonder("b_save.bco=" + String(NEXTION_COLOR_DEFAULT));
    }
  }

  // --- KART/GÖZ SEÇİMİ (Page 1) ---
  else if (komut == "PAGE1_OPEN") {
    if (!isProvisioningAllowed()) {
      nextionGonder("b_save.bco=" + String(NEXTION_COLOR_RED));
      return;
    }
    nextionGonder("page page1");
    temp_goz = secili_goz;
    nextionGonder("t0.txt=\"" + String(temp_goz) + "\"");
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
    nextionGonder("b_goz.txt=\"Goz: " + String(secili_goz) + "\"");
    nextionGonder(String("t_set_sure.txt=\"") + (hedef_sure[secili_goz] < 10 ? "0" : "") + String(hedef_sure[secili_goz]) + "\"");
    nextionGonder("t_set_sic.txt=\"" + String(hedef_sicaklik[secili_goz]) + "\"");
    stmSetpointleriGonder();
  }
  else if (komut == "BACK") {
    temp_goz = secili_goz;
    nextionGonder("page page0");
  }

  // --- ŞİFRE VE KLAVYE (Page 4) ---
  else if (komut.startsWith("KEY") && komut.length() == 4 && isDigit(komut.charAt(3))) {
    char digit = komut.charAt(3);
    if (girilen_sifre.length() < 6) {
      girilen_sifre += digit;
      String yildizlar = "";
      for (int i = 0; i < girilen_sifre.length(); i++) yildizlar += "*";
      nextionGonder("t_pass.txt=\"" + yildizlar + "\"");
    }
  }
  else if (komut == "KEY_DEL") {
    if (girilen_sifre.length() > 0) {
      girilen_sifre.remove(girilen_sifre.length() - 1);
      String yildizlar = "";
      for (int i = 0; i < girilen_sifre.length(); i++) yildizlar += "*";
      nextionGonder("t_pass.txt=\"" + yildizlar + "\"");
    }
  }
  else if (komut == "KEY_OK") {
    if (girilen_sifre == dogru_sifre) {
      girilen_sifre = "";
      g_service_authenticated = true; // Servis oturumu basarili
      service_auth_time = millis();
      nextionGonder("page page5");
      nextionGonder("t_guc.txt=\"" + String(guc_seviyesi) + "\"");
      nextionGonder("t_id.txt=\"" + String(kart_id) + "\"");
      nextionGonder("t_max.txt=\"" + String(max_goz_sayisi) + "\"");
    } else {
      girilen_sifre = "";
      g_service_authenticated = false;
      nextionGonder("t_pass.txt=\"HATALI!\"");
    }
  }

  // --- SETPOINT AYARLARI (Page 0) ---
  else if (komut == "TIME_UP" || komut == "TIME_DOWN" || komut.startsWith("SET_TIME:") || komut.startsWith("CMD_SET_TIME:")) {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) return;
    disarmDegasIfArmed(secili_goz);
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
  else if (komut == "TEMP_UP" || komut == "TEMP_DOWN" || komut.startsWith("SET_TEMP:") || komut.startsWith("CMD_SET_TEMP:")) {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) return;
    disarmDegasIfArmed(secili_goz);
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

  // --- SERVIS AYARLARI (Page 5) ---
  else if (komut == "GUC_UP") {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) return;
    if (degas_armed[secili_goz]) {
      degas_armed[secili_goz] = false;
      nextionGonder("b_degas.bco=" + String(NEXTION_COLOR_DEFAULT));
    }
    if (guc_seviyesi < 100) guc_seviyesi += 10;
    nextionGonder("t_guc.txt=\"" + String(guc_seviyesi) + "\"");
  }
  else if (komut == "GUC_DOWN") {
    if (makine_calisiyor[secili_goz] || degas_active[secili_goz]) return;
    if (degas_armed[secili_goz]) {
      degas_armed[secili_goz] = false;
      nextionGonder("b_degas.bco=" + String(NEXTION_COLOR_DEFAULT));
    }
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
  else if (komut.startsWith("CMD_SET_STEP_INC:") || komut.startsWith("SET_STEP_INC:")) {
    if (!isProvisioningAllowed()) {
      Serial.println("--> AUTH REJECTED: SERVICE PIN REQUIRED FOR SWEEP STEP INC");
      return;
    }
    int val = komut.substring(komut.indexOf(':') + 1).toInt();
    if (val >= 1 && val <= 8) {
      step_increment = val;
      nvsKaydet();
      stmSetStepInc(step_increment);
    }
  }
  else if (komut.startsWith("CMD_SET_SWP_SPAN:") || komut.startsWith("SET_SWP_SPAN:") || komut.startsWith("SET_SPAN:")) {
    if (!isProvisioningAllowed()) {
      Serial.println("--> AUTH REJECTED: SERVICE PIN REQUIRED FOR SWEEP SPAN");
      return;
    }
    int val = komut.substring(komut.indexOf(':') + 1).toInt();
    if (val >= 1 && val <= 4) {
      swp_span = val;
      nvsKaydet();
      stmSetSwpSpan(swp_span);
    }
  }
  else if (komut.startsWith("CMD_SET_SWP_PER:") || komut.startsWith("SET_SWP_PER:") || komut.startsWith("SET_PER:")) {
    if (!isProvisioningAllowed()) {
      Serial.println("--> AUTH REJECTED: SERVICE PIN REQUIRED FOR SWEEP PERIOD");
      return;
    }
    int val = komut.substring(komut.indexOf(':') + 1).toInt();
    if (val >= 100 && val <= 1000) {
      swp_per = val;
      nvsKaydet();
      stmSetSwpPer(swp_per);
    }
  }
  // --- SERVICE SETTINGS PAGE 3: DEGAS SETTINGS ---
  else if (komut == "PAGE3_OPEN" || komut == "PAGE_DEGAS_OPEN" || komut == "PAGE3" || komut == "PAGE_DEGAS") {
    if (!isProvisioningAllowed()) {
      nextionGonder("b_save.bco=" + String(NEXTION_COLOR_RED));
      return;
    }
    nextionGonder("page page3");
    updateDegasPageUI(secili_goz);
  }
  else if (komut == "DEG_DUR_UP" || komut.startsWith("SET_DEG_DUR:")) {
    if (!isProvisioningAllowed()) return;
    int g = secili_goz;
    if (komut.startsWith("SET_DEG_DUR:")) {
      int v = komut.substring(12).toInt();
      if (v >= 1 && v <= 120) service_degas[g].duration_minutes = v;
    } else {
      if (service_degas[g].duration_minutes < 120) service_degas[g].duration_minutes++;
    }
    updateDegasPageUI(g);
  }
  else if (komut == "DEG_DUR_DOWN") {
    if (!isProvisioningAllowed()) return;
    int g = secili_goz;
    if (service_degas[g].duration_minutes > 1) service_degas[g].duration_minutes--;
    updateDegasPageUI(g);
  }
  else if (komut == "DEG_PWR_UP" || komut.startsWith("SET_DEG_PWR:")) {
    if (!isProvisioningAllowed()) return;
    int g = secili_goz;
    if (komut.startsWith("SET_DEG_PWR:")) {
      int v = komut.substring(12).toInt();
      if (v >= 10 && v <= 100) service_degas[g].power_pct = v;
    } else {
      if (service_degas[g].power_pct <= 90) service_degas[g].power_pct += 10;
    }
    updateDegasPageUI(g);
  }
  else if (komut == "DEG_PWR_DOWN") {
    if (!isProvisioningAllowed()) return;
    int g = secili_goz;
    if (service_degas[g].power_pct >= 20) service_degas[g].power_pct -= 10;
    updateDegasPageUI(g);
  }
  else if (komut == "DEG_FRQ_UP" || komut.startsWith("SET_DEG_FRQ:")) {
    if (!isProvisioningAllowed()) return;
    int g = secili_goz;
    if (komut.startsWith("SET_DEG_FRQ:")) {
      int v = komut.substring(12).toInt();
      if (v >= 28 && v <= 40) service_degas[g].frequency_khz = v;
    } else {
      if (service_degas[g].frequency_khz < 40) service_degas[g].frequency_khz++;
    }
    updateDegasPageUI(g);
  }
  else if (komut == "DEG_FRQ_DOWN") {
    if (!isProvisioningAllowed()) return;
    int g = secili_goz;
    if (service_degas[g].frequency_khz > 28) service_degas[g].frequency_khz--;
    updateDegasPageUI(g);
  }
  else if (komut == "DEG_ON_UP" || komut.startsWith("SET_DEG_ON:")) {
    if (!isProvisioningAllowed()) return;
    int g = secili_goz;
    if (komut.startsWith("SET_DEG_ON:")) {
      int v = komut.substring(11).toInt();
      if (v >= 100 && v <= 10000) service_degas[g].pulse_on_ms = v;
    } else {
      if (service_degas[g].pulse_on_ms <= 9900) service_degas[g].pulse_on_ms += 100;
    }
    updateDegasPageUI(g);
  }
  else if (komut == "DEG_ON_DOWN") {
    if (!isProvisioningAllowed()) return;
    int g = secili_goz;
    if (service_degas[g].pulse_on_ms >= 200) service_degas[g].pulse_on_ms -= 100;
    updateDegasPageUI(g);
  }
  else if (komut == "DEG_OFF_UP" || komut.startsWith("SET_DEG_OFF:")) {
    if (!isProvisioningAllowed()) return;
    int g = secili_goz;
    if (komut.startsWith("SET_DEG_OFF:")) {
      int v = komut.substring(12).toInt();
      if (v == 0 || (v >= 100 && v <= 10000)) service_degas[g].pulse_off_ms = v;
    } else {
      if (service_degas[g].pulse_off_ms <= 9900) service_degas[g].pulse_off_ms += 100;
    }
    updateDegasPageUI(g);
  }
  else if (komut == "DEG_OFF_DOWN") {
    if (!isProvisioningAllowed()) return;
    int g = secili_goz;
    if (service_degas[g].pulse_off_ms >= 100) service_degas[g].pulse_off_ms -= 100;
    updateDegasPageUI(g);
  }
  else if (komut == "DEG_TC_TOGGLE" || komut.startsWith("SET_DEG_TC:")) {
    if (!isProvisioningAllowed()) return;
    int g = secili_goz;
    if (komut.startsWith("SET_DEG_TC:")) {
      int v = komut.substring(11).toInt();
      service_degas[g].temp_ctrl = (v != 0) ? 1 : 0;
    } else {
      service_degas[g].temp_ctrl = (service_degas[g].temp_ctrl != 0) ? 0 : 1;
    }
    updateDegasPageUI(g);
  }
  else if (komut == "DEG_TGT_UP" || komut.startsWith("SET_DEG_TGT:")) {
    if (!isProvisioningAllowed()) return;
    int g = secili_goz;
    if (service_degas[g].temp_ctrl == 0) return; // Neutralized when temp control is OFF
    if (komut.startsWith("SET_DEG_TGT:")) {
      float v = komut.substring(12).toFloat();
      if (v >= 20.0f && v <= 90.0f) service_degas[g].target_temp_c = v;
    } else {
      if (service_degas[g].target_temp_c < 90.0f) service_degas[g].target_temp_c += 1.0f;
    }
    updateDegasPageUI(g);
  }
  else if (komut == "DEG_TGT_DOWN") {
    if (!isProvisioningAllowed()) return;
    int g = secili_goz;
    if (service_degas[g].temp_ctrl == 0) return; // Neutralized when temp control is OFF
    if (service_degas[g].target_temp_c > 20.0f) service_degas[g].target_temp_c -= 1.0f;
    updateDegasPageUI(g);
  }
  else if (komut == "SRV_DEGAS_SAVE" || komut == "SRV_SAVE_DEGAS") {
    if (!isProvisioningAllowed()) {
      nextionGonder("b_save.bco=" + String(NEXTION_COLOR_RED));
      g_bus_diag.tx_nack_count++;
      return;
    }
    int g = secili_goz;
    bool valid = (service_degas[g].duration_minutes >= 1 && service_degas[g].duration_minutes <= 120) &&
                 (service_degas[g].power_pct >= 10 && service_degas[g].power_pct <= 100) &&
                 (service_degas[g].frequency_khz >= 28 && service_degas[g].frequency_khz <= 40) &&
                 (service_degas[g].pulse_on_ms >= 100 && service_degas[g].pulse_on_ms <= 10000) &&
                 (service_degas[g].pulse_off_ms == 0 || (service_degas[g].pulse_off_ms >= 100 && service_degas[g].pulse_off_ms <= 10000)) &&
                 (service_degas[g].temp_ctrl <= 1) &&
                 (service_degas[g].target_temp_c >= 20.0f && service_degas[g].target_temp_c <= 90.0f);
    if (!valid) {
      nextionGonder("b_save.bco=" + String(NEXTION_COLOR_RED));
      g_bus_diag.tx_nack_count++;
      return;
    }
    degasNvsKaydet(g);
    Serial.println("--> DEGAS SERVİS AYARLARI KAYDEDİLDİ (GÖZ " + String(g) + ")");
    nextionGonder("b_save.bco=" + String(NEXTION_COLOR_GREEN));
    delay(600);
    nextionGonder("b_save.bco=" + String(NEXTION_COLOR_DEFAULT));
    g_bus_diag.tx_ack_count++;
  }
  else if (komut == "SRV_SAVE") {
    /* Requirement 1 & 2: Gating provisioning calls on ESP32 */
    if (!isProvisioningAllowed()) {
      nextionGonder("b_save.bco=" + String(NEXTION_COLOR_RED)); // Kirmizi (hata/reddetme)
      g_bus_diag.tx_nack_count++;
      return;
    }
    nvsKaydet();
    degasNvsKaydet(secili_goz);
    Serial.println("--> SERVİS AYARLARI KAYDEDİLDİ!");
    nextionGonder("b_save.bco=" + String(NEXTION_COLOR_GREEN));
    delay(600);
    nextionGonder("b_save.bco=" + String(NEXTION_COLOR_DEFAULT));
    g_bus_diag.tx_ack_count++;
  }
  else if (komut == "DIAG" || komut == "GET_DIAG") {
    String diag_str = "DIAG,valid:" + String(g_bus_diag.rx_valid_count) +
                       ",crc_err:" + String(g_bus_diag.rx_crc_error_count) +
                       ",malformed:" + String(g_bus_diag.rx_malformed_count) +
                       ",timeout:" + String(g_bus_diag.rx_timeout_count) +
                       ",dropped:" + String(g_bus_diag.rx_dropped_count) +
                       ",tx:" + String(g_bus_diag.tx_frame_count) +
                       ",ack:" + String(g_bus_diag.tx_ack_count) +
                       ",nack:" + String(g_bus_diag.tx_nack_count);
    Serial.println("[ESP32_DIAG] " + diag_str);
    nextionGonder("t_diag.txt=\"" + diag_str + "\"");
  }
  else if (komut == "HIL_HEARTBEAT_OFF") {
    hil_heartbeat_active = false;
    Serial.println("--> HIL_HEARTBEAT_OFF: Heartbeat paused for comm loss testing");
  }
  else if (komut == "HIL_HEARTBEAT_ON") {
    hil_heartbeat_active = true;
    sonHeartbeatZamani = millis();
    Serial.println("--> HIL_HEARTBEAT_ON: Heartbeat resumed");
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

  // --- Servis Menusu Oturum Zaman Asimi Kontrolu ---
  if (g_service_authenticated && (millis() - service_auth_time > SERVICE_SESSION_TIMEOUT_MS)) {
    g_service_authenticated = false;
    Serial.println("--> SERVIS OTURUMU ZAMAN ASIMINA UGRADI (g_service_authenticated = false)");
  }

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
      // Provisioning bus komutlarini yetkilendirme ve SYS_MODE_RUNNING kilitlerine tabi tut
      if (satirUsb.indexOf("STAGE_ID") != -1 || satirUsb.indexOf("ASSIGN_ID") != -1 ||
          satirUsb.indexOf("RESET_ID") != -1 || satirUsb.indexOf("DISCOVER") != -1 ||
          satirUsb.indexOf("SET_ID") != -1) {
        if (!isProvisioningAllowed()) {
          Serial.println("--> [PC->ESP] PROVISIONING REJECTED BY ESP32 INTERLOCK");
          continue;
        }
      }

      if (isBusKomut(satirUsb)) {
        rs485Transmit(satirUsb + "\n");
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
      makine_calisiyor[i] = false;
      degas_active[i] = false;
      degas_armed[i] = false;
      stm_relay[i] = 0;
      kalan_saniye[i] = 0;
      durum_metni[i] = "Kart Yok!";
      if (i == secili_goz) {
        nextionGonder("t_durum.txt=\"Kart Yok!\"");
        nextionGonder("t_status.txt=\"Kart Yok!\"");
        nextionGonder("b_degas.bco=" + String(NEXTION_COLOR_DEFAULT));
      }
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

  // --- Periodic Heartbeat (1000ms) to prevent STM32 RX silence watchdog timeout during RUNNING or DEGAS ---
  if (hil_heartbeat_active && (millis() - sonHeartbeatZamani >= 1000)) {
    sonHeartbeatZamani = millis();
    for (int i = 1; i < MAX_GOZ; i++) {
      if (stm_bagli[i] && (makine_calisiyor[i] || degas_active[i])) {
        rs485Transmit("T" + String(i) + ":HEARTBEAT\n");
      }
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