"""
Phase 5.4 Offline Nextion HMI Simulation & Verification Suite
EAGLEULTRASONİK Project — ESP32 Master Firmware Emulator

Emulates Nextion HMI touch screen protocol, menu pages, command dispatcher,
service authentication ("123456"), 5-minute inactivity session timeout,
NVS recipe persistence, 3000ms offline card connection watchdog,
provisioning interlocks, and multi-line command concatenation defense.

Runs offline without physical hardware, COM ports, or serial devices.
Run via: python -m unittest test_hmi_mock.py
"""

import unittest
import time
from typing import List, Dict, Tuple, Optional


# =============================================================================
# MOCK ESP32 HMI FIRMWARE EMULATOR (1-to-1 matching ekran_kontrol.ino)
# =============================================================================
class MockESP32HMI:
    MAX_GOZ = 11
    SERVICE_SESSION_TIMEOUT_MS = 300000  # 5 minutes
    STM_BAGLANTI_TIMEOUT = 3000         # 3 seconds

    def __init__(self):
        self.millis_ms: float = 0.0

        # System state variables
        self.secili_goz: int = 1
        self.temp_goz: int = 1
        self.aktif_program: int = 0
        self.duzenlenen_program: int = 1

        # Preset recipe templates (P1, P2, P3)
        self.p_sure = [0, 15, 20, 25]
        self.p_sicaklik = [0, 40, 50, 60]

        # Service auth state
        self.girilen_sifre: str = ""
        self.dogru_sifre: str = "123456"
        self.g_service_authenticated: bool = False
        self.service_auth_time: float = 0.0

        # Service configuration
        self.guc_seviyesi: int = 50
        self.kart_id: int = 1
        self.max_goz_sayisi: int = 3

        # Per-tank state arrays (index 1..10)
        self.makine_calisiyor = [False] * self.MAX_GOZ
        self.durum_metni = ["SISTEM BEKLEMEDE"] * self.MAX_GOZ
        self.hedef_sure = [0] * self.MAX_GOZ
        self.hedef_sicaklik = [0] * self.MAX_GOZ
        self.kalan_saniye = [0] * self.MAX_GOZ
        self.anlik_sicaklik = [24.0] * self.MAX_GOZ

        self.stm_fault = [0] * self.MAX_GOZ
        self.stm_relay = [0] * self.MAX_GOZ
        self.stm_pwr = [0] * self.MAX_GOZ
        self.stm_freq = [28] * self.MAX_GOZ
        self.stm_bagli = [False] * self.MAX_GOZ
        self.stm_son_veri_zamani = [0.0] * self.MAX_GOZ

        self.sonGuncellemeZamani: float = 0.0

        # Bus Diagnostics Counters (Task 2 & 5)
        self.g_bus_diag: Dict[str, int] = {
            "rx_valid_count": 0,
            "rx_crc_error_count": 0,
            "rx_malformed_count": 0,
            "rx_timeout_count": 0,
            "rx_dropped_count": 0,
            "tx_frame_count": 0,
            "tx_ack_count": 0,
            "tx_nack_count": 0
        }

        # NVS storage mock namespace "ultra"
        self.nvs_ultra: Dict[str, int] = {
            "pS1": 15, "pT1": 40,
            "pS2": 20, "pT2": 50,
            "pS3": 25, "pT3": 60,
            "guc": 50, "kartid": 1, "maxgoz": 3
        }

        # Telemetry & I/O Recorders
        self.nextion_tx_log: List[str] = []
        self.stm32_tx_log: List[str] = []
        self.debug_tx_log: List[str] = []

    # -------------------------------------------------------------------------
    # Helper & Interlock Evaluators
    # -------------------------------------------------------------------------
    def nextionGonder(self, komut: str):
        self.nextion_tx_log.append(komut)

    def stmGonder(self, komut: str):
        adresli = f"T{self.secili_goz}:{komut}"
        self.stm32_tx_log.append(adresli.strip())
        self.debug_tx_log.append(f"[ESP->STM] {adresli.strip()}")

    def isAnyTankRunning(self) -> bool:
        for i in range(1, self.MAX_GOZ):
            if self.makine_calisiyor[i] or self.stm_relay[i] != 0:
                return True
        return False

    def isProvisioningAllowed(self) -> bool:
        if not self.g_service_authenticated:
            self.debug_tx_log.append("--> HATA: SERVIS YETKILENDIRMESI GEREKLI (g_service_authenticated == false)")
            return False
        if self.isAnyTankRunning():
            self.debug_tx_log.append("--> HATA: CALISAN TANK VAR! PROVISIONING KILITLI")
            return False
        return True

    def isKartBagli(self, goz_id: int) -> bool:
        if goz_id == 0 or goz_id >= self.MAX_GOZ:
            return False
        return (self.millis_ms - self.stm_son_veri_zamani[goz_id]) < self.STM_BAGLANTI_TIMEOUT

    def baslatmaEngelliMi(self) -> bool:
        if self.isKartBagli(self.secili_goz):
            return False

        self.debug_tx_log.append(f"--> HATA: GÖZ {self.secili_goz} KART BAGLI DEGIL! ISLEM BASLATILAMIYOR.")
        self.durum_metni[self.secili_goz] = "Kart Yok!"
        self.nextionGonder('t_durum.txt="Kart Yok!"')
        return True

    def stmSetpointleriGonder(self):
        self.stmGonder(f"SET_TIME:{self.hedef_sure[self.secili_goz]}\n")
        self.stmGonder(f"SET_TEMP:{self.hedef_sicaklik[self.secili_goz]}\n")
        self.stmGonder(f"SET_POWER:{self.guc_seviyesi}\n")

    def nvsKaydet(self):
        for i in range(1, 4):
            self.nvs_ultra[f"pS{i}"] = self.p_sure[i]
            self.nvs_ultra[f"pT{i}"] = self.p_sicaklik[i]
        self.nvs_ultra["guc"] = self.guc_seviyesi
        self.nvs_ultra["kartid"] = self.kart_id
        self.nvs_ultra["maxgoz"] = self.max_goz_sayisi

    def nvsYukle(self):
        for i in range(1, 4):
            self.p_sure[i] = self.nvs_ultra.get(f"pS{i}", self.p_sure[i])
            self.p_sicaklik[i] = self.nvs_ultra.get(f"pT{i}", self.p_sicaklik[i])
        self.guc_seviyesi = self.nvs_ultra.get("guc", self.guc_seviyesi)
        self.kart_id = self.nvs_ultra.get("kartid", self.kart_id)
        self.max_goz_sayisi = self.nvs_ultra.get("maxgoz", self.max_goz_sayisi)

    # -------------------------------------------------------------------------
    # HMI Command Dispatcher (komutIsle)
    # -------------------------------------------------------------------------
    def komutIsle(self, komut: str):
        komut = komut.strip()
        if not komut:
            return

        # Defensive line splitter on '\n'
        if '\n' in komut:
            lines = komut.split('\n')
            for l in lines:
                self.komutIsle(l)
            return

        self.debug_tx_log.append(f'DEBUG_ESP32: HMI_RX raw="{komut}"')

        # --- P_HIZLI (Page 0) ---
        if komut == "P_HIZLI":
            if self.baslatmaEngelliMi():
                return
            self.hedef_sure[self.secili_goz] = 5
            self.hedef_sicaklik[self.secili_goz] = 30
            self.kalan_saniye[self.secili_goz] = 0
            self.makine_calisiyor[self.secili_goz] = True
            self.durum_metni[self.secili_goz] = "HIZLI YIKAMA DEVAM EDIYOR"

            self.nextionGonder('t_set_sure.txt="05"')
            self.nextionGonder('t_set_sic.txt="30"')

            self.stmGonder(f"SET_TIME:{self.hedef_sure[self.secili_goz]}\n")
            self.stmGonder(f"SET_TEMP:{self.hedef_sicaklik[self.secili_goz]}\n")
            self.stmGonder(f"SET_POWER:{self.guc_seviyesi}\n")
            self.stmGonder("START\n")

        # --- CMD_START|<sure>|<sicaklik> ---
        elif komut.startswith("CMD_START|"):
            if self.baslatmaEngelliMi():
                return
            parts = komut.split('|')
            if len(parts) >= 3:
                try:
                    s_sure = int(parts[1])
                    s_sic = int(parts[2])
                    self.hedef_sure[self.secili_goz] = s_sure
                    self.hedef_sicaklik[self.secili_goz] = s_sic

                    if s_sure == 0 or s_sic == 0:
                        self.durum_metni[self.secili_goz] = "SURE/SICAKLIK GIRIN!"
                    else:
                        self.makine_calisiyor[self.secili_goz] = True
                        self.kalan_saniye[self.secili_goz] = s_sure * 60
                        self.durum_metni[self.secili_goz] = "YIKAMA DEVAM EDIYOR..."

                        self.stmGonder(f"SET_TIME:{self.hedef_sure[self.secili_goz]}\n")
                        self.stmGonder(f"SET_TEMP:{self.hedef_sicaklik[self.secili_goz]}\n")
                        self.stmGonder(f"SET_POWER:{self.guc_seviyesi}\n")
                        self.stmGonder("START\n")
                except ValueError:
                    pass

        # --- CMD_STOP ---
        elif komut == "CMD_STOP":
            self.makine_calisiyor[self.secili_goz] = False
            self.durum_metni[self.secili_goz] = "SISTEM DURDURULDU"
            self.stmGonder("STOP\n")

        # --- CMD_FREQ|<freq> ---
        elif komut.startswith("CMD_FREQ|") or komut.startswith("SET_FREQ|"):
            parts = komut.split('|')
            if len(parts) >= 2:
                try:
                    freq = int(parts[1])
                    self.stmGonder(f"SET_FREQ:{freq}\n")
                except ValueError:
                    pass

        # --- P1_SEL / P2_SEL / P3_SEL ---
        elif komut in ["P1_SEL", "P2_SEL", "P3_SEL"]:
            if self.baslatmaEngelliMi():
                return
            self.aktif_program = 1 if komut == "P1_SEL" else (2 if komut == "P2_SEL" else 3)
            self.hedef_sure[self.secili_goz] = self.p_sure[self.aktif_program]
            self.hedef_sicaklik[self.secili_goz] = self.p_sicaklik[self.aktif_program]
            self.kalan_saniye[self.secili_goz] = 0
            self.makine_calisiyor[self.secili_goz] = False
            self.durum_metni[self.secili_goz] = f"P{self.aktif_program} SECILDI. START BEKLENIYOR"

            sure_str = f"{self.hedef_sure[self.secili_goz]:02d}"
            self.nextionGonder(f't_set_sure.txt="{sure_str}"')
            self.nextionGonder(f't_set_sic.txt="{self.hedef_sicaklik[self.secili_goz]}"')

            self.stmGonder(f"SET_TIME:{self.hedef_sure[self.secili_goz]}\n")
            self.stmGonder(f"SET_TEMP:{self.hedef_sicaklik[self.secili_goz]}\n")

        # --- EDIT_P1 / EDIT_P2 / EDIT_P3 (Page 2) ---
        elif komut in ["EDIT_P1", "EDIT_P2", "EDIT_P3"]:
            self.duzenlenen_program = 1 if komut == "EDIT_P1" else (2 if komut == "EDIT_P2" else 3)
            p = self.duzenlenen_program
            self.nextionGonder(f't0.txt="PROGRAM P{p}"')
            self.nextionGonder(f't_set_sure.txt="{self.p_sure[p]:02d}"')
            self.nextionGonder(f't_set_sic.txt="{self.p_sicaklik[p]}"')

        # --- P_SAVE|<sure>|<sicaklik> (Page 2) ---
        elif komut.startswith("P_SAVE|"):
            parts = komut.split('|')
            if len(parts) >= 3:
                try:
                    s_sure = int(parts[1])
                    s_sic = int(parts[2])
                    p = self.duzenlenen_program
                    self.p_sure[p] = s_sure
                    self.p_sicaklik[p] = s_sic
                    self.nvsKaydet()
                    self.nextionGonder("b_save.bco=2016")  # Green ACK
                    self.nextionGonder("b_save.bco=50712")
                except ValueError:
                    pass

        # --- PAGE1 Tank Selection ---
        elif komut == "PAGE1_OPEN":
            self.temp_goz = self.secili_goz
            self.nextionGonder(f't0.txt="{self.temp_goz}"')

        elif komut == "UP":
            if self.temp_goz < self.max_goz_sayisi:
                self.temp_goz += 1
            self.nextionGonder(f't0.txt="{self.temp_goz}"')

        elif komut == "DOWN":
            if self.temp_goz > 1:
                self.temp_goz -= 1
            self.nextionGonder(f't0.txt="{self.temp_goz}"')

        elif komut == "SEL":
            self.secili_goz = self.temp_goz
            self.nextionGonder("page page0")
            self.nextionGonder(f'b_goz.txt="Goz: {self.secili_goz}"')
            sure_str = f"{self.hedef_sure[self.secili_goz]:02d}"
            self.nextionGonder(f't_set_sure.txt="{sure_str}"')
            self.nextionGonder(f't_set_sic.txt="{self.hedef_sicaklik[self.secili_goz]}"')
            self.stmSetpointleriGonder()

        elif komut == "BACK":
            self.temp_goz = self.secili_goz
            self.nextionGonder("page page0")

        # --- Password & Keypad (Page 4) ---
        elif komut.startswith("KEY") and len(komut) == 4 and komut[3].isdigit():
            digit = komut[3]
            if len(self.girilen_sifre) < 6:
                self.girilen_sifre += digit
                stars = "*" * len(self.girilen_sifre)
                self.nextionGonder(f't_pass.txt="{stars}"')

        elif komut == "KEY_DEL":
            if len(self.girilen_sifre) > 0:
                self.girilen_sifre = self.girilen_sifre[:-1]
                stars = "*" * len(self.girilen_sifre)
                self.nextionGonder(f't_pass.txt="{stars}"')

        elif komut == "KEY_OK":
            if self.girilen_sifre == self.dogru_sifre:
                self.girilen_sifre = ""
                self.g_service_authenticated = True
                self.service_auth_time = self.millis_ms
                self.nextionGonder("page page5")
                self.nextionGonder(f't_guc.txt="{self.guc_seviyesi}"')
                self.nextionGonder(f't_id.txt="{self.kart_id}"')
                self.nextionGonder(f't_max.txt="{self.max_goz_sayisi}"')
            else:
                self.girilen_sifre = ""
                self.g_service_authenticated = False
                self.nextionGonder('t_pass.txt="HATALI!"')

        # --- Service Setup Page 5 Controls ---
        elif komut == "GUC_UP":
            if self.guc_seviyesi < 100:
                self.guc_seviyesi += 10
            self.nextionGonder(f't_guc.txt="{self.guc_seviyesi}"')

        elif komut == "GUC_DOWN":
            if self.guc_seviyesi > 10:
                self.guc_seviyesi -= 10
            self.nextionGonder(f't_guc.txt="{self.guc_seviyesi}"')

        elif komut == "ID_UP":
            self.kart_id += 1
            self.nextionGonder(f't_id.txt="{self.kart_id}"')

        elif komut == "ID_DOWN":
            if self.kart_id > 1:
                self.kart_id -= 1
            self.nextionGonder(f't_id.txt="{self.kart_id}"')

        elif komut == "MAX_UP":
            if self.max_goz_sayisi < self.MAX_GOZ - 1:
                self.max_goz_sayisi += 1
            self.nextionGonder(f't_max.txt="{self.max_goz_sayisi}"')

        elif komut == "MAX_DOWN":
            if self.max_goz_sayisi > 1:
                self.max_goz_sayisi -= 1
            self.nextionGonder(f't_max.txt="{self.max_goz_sayisi}"')

        elif komut == "SRV_SAVE":
            if not self.isProvisioningAllowed():
                self.g_bus_diag["tx_nack_count"] += 1
                self.nextionGonder("b_save.bco=63488")  # Red error
                return
            self.nvsKaydet()
            self.g_bus_diag["tx_ack_count"] += 1
            self.nextionGonder("b_save.bco=2016")

        elif komut in ["DIAG", "GET_DIAG"]:
            self.g_bus_diag["rx_valid_count"] = (self.g_bus_diag["rx_valid_count"] + 1) & 0xFFFFFFFF
            diag_str = f"DIAG,valid:{self.g_bus_diag['rx_valid_count']},crc_err:{self.g_bus_diag['rx_crc_error_count']},malformed:{self.g_bus_diag['rx_malformed_count']},timeout:{self.g_bus_diag['rx_timeout_count']},dropped:{self.g_bus_diag['rx_dropped_count']},tx:{self.g_bus_diag['tx_frame_count']},ack:{self.g_bus_diag['tx_ack_count']},nack:{self.g_bus_diag['tx_nack_count']}"
            self.nextionGonder(f't_diag.txt="{diag_str}"')

    # -------------------------------------------------------------------------
    # STM32 Telemetry Receiver (stmTelemetryIsle)
    # -------------------------------------------------------------------------
    def stmTelemetryIsle(self, satir: str):
        satir = satir.strip()
        if not satir.startswith("STAT,"):
            return

        parts = satir[5:].split(',')
        if len(parts) < 8:
            return

        try:
            tank_id = int(parts[0])
            mode_str = parts[1]
            rem_sec = int(parts[2])
            temp_x10 = int(parts[3])
            relay = int(parts[4])
            pwr = int(parts[5])
            freq = int(parts[6])
            fault = int(parts[7])
        except ValueError:
            return

        if tank_id < 1 or tank_id >= self.MAX_GOZ:
            return

        g = tank_id
        self.kalan_saniye[g] = rem_sec
        self.anlik_sicaklik[g] = temp_x10 / 10.0
        self.stm_relay[g] = relay
        self.stm_pwr[g] = pwr
        self.stm_freq[g] = freq
        self.stm_fault[g] = fault
        self.makine_calisiyor[g] = (mode_str == "RUNNING")

        yeniden_baglandi = not self.stm_bagli[g]
        self.stm_bagli[g] = True
        self.stm_son_veri_zamani[g] = self.millis_ms

        if fault > 0:
            self.durum_metni[g] = f"HATA! KOD:{fault}"
        elif mode_str == "RUNNING":
            self.durum_metni[g] = "YIKAMA DEVAM EDIYOR..."
        elif rem_sec <= 0 and self.hedef_sure[g] > 0:
            self.durum_metni[g] = "YIKAMA TAMAMLANDI!"
        else:
            self.durum_metni[g] = "SISTEM BEKLEMEDE"

        if g == self.secili_goz:
            hmi_durum = "Calisiyor" if mode_str == "RUNNING" else ("Hata" if mode_str == "FAULT" else "Beklemede")
            self.nextionGonder(f't_status.txt="{hmi_durum}"')
            if yeniden_baglandi:
                self.stmSetpointleriGonder()

    # -------------------------------------------------------------------------
    # Superloop Tick (loop)
    # -------------------------------------------------------------------------
    def loop_tick(self, current_millis: float):
        self.millis_ms = current_millis

        # Service Session Timeout Check (5 minutes)
        if self.g_service_authenticated and (self.millis_ms - self.service_auth_time > self.SERVICE_SESSION_TIMEOUT_MS):
            self.g_service_authenticated = False
            self.debug_tx_log.append("--> SERVIS OTURUMU ZAMAN ASIMINA UGRADI (g_service_authenticated = false)")

        # Connection Timeout Check (3 seconds)
        for i in range(1, self.MAX_GOZ):
            if self.stm_bagli[i] and (self.millis_ms - self.stm_son_veri_zamani[i] > self.STM_BAGLANTI_TIMEOUT):
                self.stm_bagli[i] = False

        # Periodic 1000ms Nextion Display Refresh (ekran_kontrol.ino lines 874-891)
        if self.millis_ms - self.sonGuncellemeZamani > 1000:
            self.sonGuncellemeZamani = self.millis_ms

            kalan_dk = self.kalan_saniye[self.secili_goz] // 60
            kalan_sn = self.kalan_saniye[self.secili_goz] % 60
            zaman_buf = f"{kalan_dk:02d}:{kalan_sn:02d}"

            pt100_hata = (self.stm_fault[self.secili_goz] & (0x01 | 0x02)) != 0
            sicaklik_metni = "--.-" if pt100_hata else f"{self.anlik_sicaklik[self.secili_goz]:.1f}"

            self.nextionGonder(f't_kalan_sure.txt="{zaman_buf}"')
            self.nextionGonder(f't_anlik_sic.txt="{sicaklik_metni}"')
            self.nextionGonder(f't_durum.txt="{self.durum_metni[self.secili_goz]}"')
            self.nextionGonder(f'b_goz.txt="Goz: {self.secili_goz}"')


# =============================================================================
# THE 18 OFFLINE HMI MOCK UNITTEST TEST CASES
# =============================================================================
class TestHMIMockSuite(unittest.TestCase):

    def setUp(self):
        self.hmi = MockESP32HMI()
        self.hmi.millis_ms = 1000.0
        self.hmi.sonGuncellemeZamani = 1000.0
        # Feed telemetry to connect Tank 1 initially
        self.hmi.stmTelemetryIsle("STAT,1,IDLE,0,250,0,50,28,0")

    # -------------------------------------------------------------------------
    # Group 1: HMI Command Parsing & Processing (5 tests)
    # -------------------------------------------------------------------------
    def test_01_p_hizli_command(self):
        """Test 01: P_HIZLI sets 5 min / 30 degC and transmits START to STM32."""
        self.hmi.komutIsle("P_HIZLI")
        self.assertTrue(self.hmi.makine_calisiyor[1])
        self.assertEqual(self.hmi.hedef_sure[1], 5)
        self.assertEqual(self.hmi.hedef_sicaklik[1], 30)
        self.assertIn("T1:START", self.hmi.stm32_tx_log)
        self.assertIn('t_set_sure.txt="05"', self.hmi.nextion_tx_log)

    def test_02_cmd_start_command(self):
        """Test 02: CMD_START|10|50 parses duration and temp and starts washing cycle."""
        self.hmi.komutIsle("CMD_START|10|50")
        self.assertTrue(self.hmi.makine_calisiyor[1])
        self.assertEqual(self.hmi.hedef_sure[1], 10)
        self.assertEqual(self.hmi.hedef_sicaklik[1], 50)
        self.assertEqual(self.hmi.kalan_saniye[1], 600)
        self.assertIn("T1:START", self.hmi.stm32_tx_log)

    def test_03_cmd_stop_command(self):
        """Test 03: CMD_STOP stops washing cycle and emits T1:STOP."""
        self.hmi.makine_calisiyor[1] = True
        self.hmi.komutIsle("CMD_STOP")
        self.assertFalse(self.hmi.makine_calisiyor[1])
        self.assertIn("T1:STOP", self.hmi.stm32_tx_log)

    def test_04_cmd_freq_command(self):
        """Test 04: CMD_FREQ|40 sends T1:SET_FREQ:40 to STM32."""
        self.hmi.komutIsle("CMD_FREQ|40")
        self.assertIn("T1:SET_FREQ:40", self.hmi.stm32_tx_log)

    def test_05_preset_recipe_selection(self):
        """Test 05: P1_SEL, P2_SEL, P3_SEL load template values into active tank."""
        self.hmi.komutIsle("P2_SEL")
        self.assertEqual(self.hmi.hedef_sure[1], 20)      # P2 default 20 min
        self.assertEqual(self.hmi.hedef_sicaklik[1], 50)  # P2 default 50 degC
        self.assertIn("T1:SET_TIME:20", self.hmi.stm32_tx_log)
        self.assertIn("T1:SET_TEMP:50", self.hmi.stm32_tx_log)

    # -------------------------------------------------------------------------
    # Group 2: Service Authentication & Session Timeout (3 tests)
    # -------------------------------------------------------------------------
    def test_06_correct_password_login(self):
        """Test 06: Correct password '123456' authenticates service session."""
        for d in "123456":
            self.hmi.komutIsle(f"KEY{d}")
        self.hmi.komutIsle("KEY_OK")
        self.assertTrue(self.hmi.g_service_authenticated)
        self.assertIn("page page5", self.hmi.nextion_tx_log)

    def test_07_invalid_password_rejection(self):
        """Test 07: Invalid password '000000' is rejected with HATALI!."""
        for d in "000000":
            self.hmi.komutIsle(f"KEY{d}")
        self.hmi.komutIsle("KEY_OK")
        self.assertFalse(self.hmi.g_service_authenticated)
        self.assertIn('t_pass.txt="HATALI!"', self.hmi.nextion_tx_log)

    def test_08_service_session_5min_timeout(self):
        """Test 08: 5-minute inactivity timeout automatically resets authentication."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = 1000.0

        # Advance clock by 4 minutes (240,000 ms)
        self.hmi.loop_tick(241000.0)
        self.assertTrue(self.hmi.g_service_authenticated, "Session should remain valid under 5 min")

        # Advance clock past 5 minutes (301,000 ms total elapsed)
        self.hmi.loop_tick(302000.0)
        self.assertFalse(self.hmi.g_service_authenticated, "Session must timeout after 5 minutes of inactivity")

    # -------------------------------------------------------------------------
    # Group 3: Recipe Editing & NVS Persistence (2 tests)
    # -------------------------------------------------------------------------
    def test_09_recipe_editing_and_save(self):
        """Test 09: EDIT_P2 followed by P_SAVE|18|55 updates P2 setpoints and NVS storage."""
        self.hmi.komutIsle("EDIT_P2")
        self.assertEqual(self.hmi.duzenlenen_program, 2)

        self.hmi.komutIsle("P_SAVE|18|55")
        self.assertEqual(self.hmi.p_sure[2], 18)
        self.assertEqual(self.hmi.p_sicaklik[2], 55)
        self.assertEqual(self.hmi.nvs_ultra["pS2"], 18)
        self.assertEqual(self.hmi.nvs_ultra["pT2"], 55)

    def test_10_recipe_nvs_roundtrip_persistence(self):
        """Test 10: Reloading NVS preserves modified prescription template values."""
        self.hmi.nvs_ultra["pS3"] = 35
        self.hmi.nvs_ultra["pT3"] = 75
        self.hmi.nvsYukle()

        self.assertEqual(self.hmi.p_sure[3], 35)
        self.assertEqual(self.hmi.p_sicaklik[3], 75)

    # -------------------------------------------------------------------------
    # Group 4: Offline Card Connection Watchdog (3 tests)
    # -------------------------------------------------------------------------
    def test_11_offline_card_start_rejection(self):
        """Test 11: Telemetry older than 3000ms triggers isKartBagli == False and blocks CMD_START."""
        # Advance time by 4500ms without new telemetry
        self.hmi.loop_tick(5500.0)
        self.assertFalse(self.hmi.isKartBagli(1))

        self.hmi.komutIsle("CMD_START|10|50")
        self.assertFalse(self.hmi.makine_calisiyor[1], "Start must be blocked when card is offline")
        self.assertEqual(self.hmi.durum_metni[1], "Kart Yok!")
        self.assertIn('t_durum.txt="Kart Yok!"', self.hmi.nextion_tx_log)

    def test_12_reconnect_telemetry_recovery(self):
        """Test 12: Fresh telemetry re-establishes connection and enables CMD_START."""
        self.hmi.loop_tick(5500.0)
        self.assertFalse(self.hmi.isKartBagli(1))

        # Deliver fresh telemetry at current time
        self.hmi.stmTelemetryIsle("STAT,1,IDLE,0,250,0,50,28,0")
        self.assertTrue(self.hmi.isKartBagli(1))

        self.hmi.komutIsle("CMD_START|10|50")
        self.assertTrue(self.hmi.makine_calisiyor[1], "Start must succeed after telemetry reconnect")

    def test_13_pt100_error_display_formatting(self):
        """Test 13: PT100 fault bit forces temperature display to '--.-' during periodic Nextion update."""
        # Send telemetry with PT100 OPEN fault (0x01)
        self.hmi.stmTelemetryIsle("STAT,1,FAULT,0,0,0,50,28,1")
        # Trigger 1000ms periodic refresh loop tick
        self.hmi.loop_tick(2500.0)
        self.assertIn('t_anlik_sic.txt="--.-"', self.hmi.nextion_tx_log)

    # -------------------------------------------------------------------------
    # Group 5: Provisioning Interlocks (2 tests)
    # -------------------------------------------------------------------------
    def test_14_unauthenticated_provisioning_rejection(self):
        """Test 14: SRV_SAVE without authentication is rejected by isProvisioningAllowed."""
        self.hmi.g_service_authenticated = False
        self.hmi.komutIsle("SRV_SAVE")
        self.assertIn("b_save.bco=63488", self.hmi.nextion_tx_log, "SRV_SAVE must turn red on rejection")

    def test_15_running_tank_provisioning_rejection(self):
        """Test 15: SRV_SAVE while a tank is running is rejected by isProvisioningAllowed."""
        self.hmi.g_service_authenticated = True
        self.hmi.makine_calisiyor[1] = True  # Tank 1 is active

        self.hmi.komutIsle("SRV_SAVE")
        self.assertIn("b_save.bco=63488", self.hmi.nextion_tx_log, "SRV_SAVE must be blocked while tank is running")

    # -------------------------------------------------------------------------
    # Group 6: Tank Selection Page Workflow (2 tests)
    # -------------------------------------------------------------------------
    def test_16_tank_selection_up_down_clamping(self):
        """Test 16: Tank selection UP/DOWN stays bounded between 1 and max_goz_sayisi."""
        self.hmi.max_goz_sayisi = 3
        self.hmi.komutIsle("PAGE1_OPEN")

        self.hmi.komutIsle("UP")
        self.assertEqual(self.hmi.temp_goz, 2)
        self.hmi.komutIsle("UP")
        self.assertEqual(self.hmi.temp_goz, 3)
        self.hmi.komutIsle("UP")
        self.assertEqual(self.hmi.temp_goz, 3, "UP must clamp at max_goz_sayisi")

        self.hmi.komutIsle("DOWN")
        self.hmi.komutIsle("DOWN")
        self.hmi.komutIsle("DOWN")
        self.assertEqual(self.hmi.temp_goz, 1, "DOWN must clamp at 1")

    def test_17_tank_selection_confirm_and_sync(self):
        """Test 17: Confirming tank selection with SEL switches active tank and syncs setpoints."""
        self.hmi.max_goz_sayisi = 3
        self.hmi.komutIsle("PAGE1_OPEN")
        self.hmi.komutIsle("UP")  # Select tank 2
        self.hmi.komutIsle("SEL") # Confirm

        self.assertEqual(self.hmi.secili_goz, 2)
        self.assertIn('b_goz.txt="Goz: 2"', self.hmi.nextion_tx_log)
        self.assertIn("T2:SET_TIME:0", self.hmi.stm32_tx_log)

    # -------------------------------------------------------------------------
    # Group 7: HMI Input Robustness & Buffer Defense (1 test)
    # -------------------------------------------------------------------------
    def test_18_multiline_concatenated_input_safety(self):
        """Test 18: Multi-line string with embedded newline splits and executes safely."""
        # Deliver fresh telemetry
        self.hmi.stmTelemetryIsle("STAT,1,IDLE,0,250,0,50,28,0")
        
        # Send multi-line string
        self.hmi.komutIsle("P1_SEL\nCMD_START|15|45")
        self.assertTrue(self.hmi.makine_calisiyor[1])
        self.assertEqual(self.hmi.hedef_sure[1], 15)
        self.assertEqual(self.hmi.hedef_sicaklik[1], 45)

    # -------------------------------------------------------------------------
    # Group 8: TASK 5 HMI Diagnostics & Security Tests (4 tests: 19..22)
    # -------------------------------------------------------------------------
    def test_19_hmi_diagnostics_command_access(self):
        """Test 19: DIAG command queries bus diagnostics counters and formats Nextion response."""
        self.hmi.komutIsle("GET_DIAG")
        self.assertTrue(any("t_diag.txt=\"DIAG,valid:" in log for log in self.hmi.nextion_tx_log))

    def test_20_hmi_diagnostics_counter_increments(self):
        """Test 20: Counter tracking increments on valid commands, ACKs, and NACKs."""
        initial_nack = self.hmi.g_bus_diag["tx_nack_count"]
        self.hmi.g_service_authenticated = False
        self.hmi.komutIsle("SRV_SAVE")  # Unauthenticated -> NACK
        self.assertEqual(self.hmi.g_bus_diag["tx_nack_count"], initial_nack + 1)

    def test_21_hmi_diagnostics_counter_overflow_safety(self):
        """Test 21: Simulating UINT32 max overflow behavior handles wrap-around safely."""
        self.hmi.g_bus_diag["rx_valid_count"] = 0xFFFFFFFF
        self.hmi.komutIsle("GET_DIAG")
        # 0xFFFFFFFF + 1 wraps around to 0
        self.assertTrue(any("valid:0" in log for log in self.hmi.nextion_tx_log))

    def test_22_hmi_diagnostics_unauthenticated_permission_check(self):
        """Test 22: Querying diagnostics does not bypass service authentication or provisioning locks."""
        self.hmi.g_service_authenticated = False
        self.hmi.komutIsle("GET_DIAG")
        # Ensure g_service_authenticated remains False
        self.assertFalse(self.hmi.g_service_authenticated)
        self.assertFalse(self.hmi.isProvisioningAllowed())


if __name__ == "__main__":
    unittest.main()
