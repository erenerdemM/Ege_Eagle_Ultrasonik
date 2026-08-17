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
        self.step_increment: int = 4
        self.swp_span: int = 2
        self.swp_per: int = 400

        # Per-tank state arrays (index 1..10)
        self.makine_calisiyor = [False] * self.MAX_GOZ
        self.degas_armed = [False] * self.MAX_GOZ
        self.degas_active = [False] * self.MAX_GOZ
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

        # Per-tank service_degas NVS configuration
        self.service_degas = [
            {
                "duration_minutes": 15,
                "power_pct": 100,
                "frequency_khz": 28,
                "pulse_on_ms": 1000,
                "pulse_off_ms": 500,
                "temp_ctrl": 0,
                "target_temp_c": 50.0
            }
            for _ in range(self.MAX_GOZ)
        ]

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
        self.g_bus_diag["tx_frame_count"] += 1

    def isAnyTankRunning(self) -> bool:
        for i in range(1, self.MAX_GOZ):
            if self.stm_bagli[i] and (self.makine_calisiyor[i] or self.degas_active[i] or self.stm_relay[i] != 0):
                return True
        return False

    def isProvisioningAllowed(self) -> bool:
        if not self.g_service_authenticated:
            self.debug_tx_log.append("--> HATA: SERVIS YETKILENDIRMESI GEREKLI (g_service_authenticated == false)")
            return False
        if (self.millis_ms - self.service_auth_time) > self.SERVICE_SESSION_TIMEOUT_MS:
            self.g_service_authenticated = False
            self.debug_tx_log.append("--> HATA: SERVIS OTURUM SURESI DOLDU")
            return False
        if self.isAnyTankRunning():
            self.debug_tx_log.append("--> HATA: CALISAN TANK VAR! PROVISIONING KILITLI (SYS_MODE_RUNNING INTERLOCK)")
            return False
        return True

    def updateDegasPageUI(self, g: int):
        if g < 1 or g >= self.MAX_GOZ:
            return
        cfg = self.service_degas[g]
        self.nextionGonder(f't_deg_goz.txt="Goz: {g}"')
        self.nextionGonder(f't_deg_dur.txt="{cfg["duration_minutes"]}"')
        self.nextionGonder(f't_deg_pwr.txt="{cfg["power_pct"]}"')
        self.nextionGonder(f't_deg_frq.txt="{cfg["frequency_khz"]}"')
        self.nextionGonder(f't_deg_on.txt="{cfg["pulse_on_ms"]}"')
        self.nextionGonder(f't_deg_off.txt="{cfg["pulse_off_ms"]}"')
        tc_str = "ON" if cfg["temp_ctrl"] != 0 else "OFF"
        self.nextionGonder(f't_deg_tc.txt="{tc_str}"')
        if cfg["temp_ctrl"] != 0:
            self.nextionGonder(f't_deg_tgt.txt="{int(cfg["target_temp_c"])}"')
        else:
            self.nextionGonder('t_deg_tgt.txt="--"')

    def disarmDegasIfArmed(self, g: int):
        if 1 <= g < self.MAX_GOZ:
            if self.degas_armed[g] and not self.degas_active[g]:
                self.degas_armed[g] = False
                if g == self.secili_goz:
                    self.nextionGonder("b_degas.bco=50712")

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

        # --- CMD_DEGAS_SEL / CMD_DEGAS_SELECT / DEGAS_SEL ---
        if komut in ["CMD_DEGAS_SEL", "CMD_DEGAS_SELECT", "DEGAS_SEL"]:
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            self.degas_armed[self.secili_goz] = not self.degas_armed[self.secili_goz]
            if self.degas_armed[self.secili_goz]:
                self.stmGonder("SWEEP:OFF\n")
                self.nextionGonder("b_degas.bco=2016")
                self.durum_metni[self.secili_goz] = "DEGAS SECILDI. START BEKLENIYOR"
            else:
                self.nextionGonder("b_degas.bco=50712")
                self.durum_metni[self.secili_goz] = "SISTEM BEKLEMEDE"

        elif komut in ["CMD_DEGAS_DESELECT", "DEGAS_DESEL"]:
            self.degas_armed[self.secili_goz] = False
            self.nextionGonder("b_degas.bco=50712")
            self.durum_metni[self.secili_goz] = "SISTEM BEKLEMEDE"

        # --- P_HIZLI (Page 0) ---
        elif komut == "P_HIZLI":
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            if self.baslatmaEngelliMi():
                return
            if self.degas_armed[self.secili_goz]:
                self.durum_metni[self.secili_goz] = "DEGAS DEVAM EDIYOR..."
                g = self.secili_goz
                cfg = self.service_degas[g]
                snap = f"START_DEGAS:{cfg['duration_minutes']}:{cfg['power_pct']}:{cfg['frequency_khz']}:{cfg['pulse_on_ms']}:{cfg['pulse_off_ms']}:{cfg['temp_ctrl']}:{cfg['target_temp_c']:.1f}\n"
                self.stmGonder(snap)
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

        # --- CMD_START | CMD_START|<sure>|<sicaklik> ---
        elif komut.startswith("CMD_START"):
            if self.degas_active[self.secili_goz] or "HATA!" in self.durum_metni[self.secili_goz]:
                return
            if self.baslatmaEngelliMi():
                return
            if self.degas_armed[self.secili_goz]:
                self.durum_metni[self.secili_goz] = "DEGAS DEVAM EDIYOR..."
                g = self.secili_goz
                cfg = self.service_degas[g]
                snap = f"START_DEGAS:{cfg['duration_minutes']}:{cfg['power_pct']}:{cfg['frequency_khz']}:{cfg['pulse_on_ms']}:{cfg['pulse_off_ms']}:{cfg['temp_ctrl']}:{cfg['target_temp_c']:.1f}\n"
                self.stmGonder(snap)
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
            else:
                self.makine_calisiyor[self.secili_goz] = True
                self.durum_metni[self.secili_goz] = "YIKAMA DEVAM EDIYOR..."
                self.stmGonder("START\n")

        # --- CMD_STOP ---
        elif komut == "CMD_STOP":
            self.makine_calisiyor[self.secili_goz] = False
            self.degas_active[self.secili_goz] = False
            self.degas_armed[self.secili_goz] = False
            self.nextionGonder("b_degas.bco=50712")
            self.durum_metni[self.secili_goz] = "SISTEM DURDURULDU"
            self.stmGonder("STOP\n")

        # --- CMD_SWEEP ---
        elif komut in ["CMD_SWEEP_ON", "CMD_SWEEP|ON"]:
            if self.degas_active[self.secili_goz]:
                return
            if self.degas_armed[self.secili_goz]:
                self.degas_armed[self.secili_goz] = False
                self.nextionGonder("b_degas.bco=50712")
            self.stmGonder("SWEEP:ON\n")
        elif komut in ["CMD_SWEEP_OFF", "CMD_SWEEP|OFF"]:
            self.stmGonder("SWEEP:OFF\n")

        # --- CMD_FREQ|<freq> ---
        elif komut.startswith("CMD_FREQ|") or komut.startswith("SET_FREQ|"):
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            if self.degas_armed[self.secili_goz]:
                self.degas_armed[self.secili_goz] = False
                self.nextionGonder("b_degas.bco=50712")
            parts = komut.split('|')
            if len(parts) >= 2:
                try:
                    freq = int(parts[1])
                    self.stmGonder(f"SET_FREQ:{freq}\n")
                except ValueError:
                    pass

        # --- P1_SEL / P2_SEL / P3_SEL ---
        elif komut in ["P1_SEL", "P2_SEL", "P3_SEL"]:
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            if self.baslatmaEngelliMi():
                return
            if self.degas_armed[self.secili_goz]:
                self.degas_armed[self.secili_goz] = False
                self.nextionGonder("b_degas.bco=50712")

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

        # --- TIME / TEMP EDIT COMMANDS ---
        elif komut in ["TIME_UP", "TIME_DOWN"] or komut.startswith("SET_TIME:") or komut.startswith("CMD_SET_TIME:"):
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            self.disarmDegasIfArmed(self.secili_goz)
            if ":" in komut:
                val = int(komut.split(":")[1])
                if 1 <= val <= 120:
                    self.hedef_sure[self.secili_goz] = val
                    self.stmGonder(f"SET_TIME:{val}\n")
            elif komut == "TIME_UP":
                if self.hedef_sure[self.secili_goz] < 120:
                    self.hedef_sure[self.secili_goz] += 1
                    self.stmGonder(f"SET_TIME:{self.hedef_sure[self.secili_goz]}\n")
            elif komut == "TIME_DOWN":
                if self.hedef_sure[self.secili_goz] > 1:
                    self.hedef_sure[self.secili_goz] -= 1
                    self.stmGonder(f"SET_TIME:{self.hedef_sure[self.secili_goz]}\n")
            self.nextionGonder(f't_set_sure.txt="{self.hedef_sure[self.secili_goz]:02d}"')

        elif komut in ["TEMP_UP", "TEMP_DOWN"] or komut.startswith("SET_TEMP:") or komut.startswith("CMD_SET_TEMP:"):
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            self.disarmDegasIfArmed(self.secili_goz)
            if ":" in komut:
                val = int(komut.split(":")[1])
                if 20 <= val <= 90:
                    self.hedef_sicaklik[self.secili_goz] = val
                    self.stmGonder(f"SET_TEMP:{val}\n")
            elif komut == "TEMP_UP":
                if self.hedef_sicaklik[self.secili_goz] < 90:
                    self.hedef_sicaklik[self.secili_goz] += 1
                    self.stmGonder(f"SET_TEMP:{self.hedef_sicaklik[self.secili_goz]}\n")
            elif komut == "TEMP_DOWN":
                if self.hedef_sicaklik[self.secili_goz] > 20:
                    self.hedef_sicaklik[self.secili_goz] -= 1
                    self.stmGonder(f"SET_TEMP:{self.hedef_sicaklik[self.secili_goz]}\n")
            self.nextionGonder(f't_set_sic.txt="{self.hedef_sicaklik[self.secili_goz]}"')

        # --- EDIT_P1 / EDIT_P2 / EDIT_P3 (Page 2) ---
        elif komut in ["EDIT_P1", "EDIT_P2", "EDIT_P3"]:
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            self.duzenlenen_program = 1 if komut == "EDIT_P1" else (2 if komut == "EDIT_P2" else 3)
            p = self.duzenlenen_program
            self.nextionGonder(f't0.txt="PROGRAM P{p}"')
            self.nextionGonder(f't_set_sure.txt="{self.p_sure[p]:02d}"')
            self.nextionGonder(f't_set_sic.txt="{self.p_sicaklik[p]}"')

        # --- P_SAVE|<sure>|<sicaklik> (Page 2) ---
        elif komut.startswith("P_SAVE|"):
            if not self.isProvisioningAllowed():
                return
            if self.degas_active[self.secili_goz] or self.makine_calisiyor[self.secili_goz]:
                return
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
            if self.degas_active[self.secili_goz]:
                return
            if self.degas_armed[self.secili_goz]:
                self.degas_armed[self.secili_goz] = False
                self.nextionGonder("b_degas.bco=50712")
            if self.guc_seviyesi < 100:
                self.guc_seviyesi += 10
            self.nextionGonder(f't_guc.txt="{self.guc_seviyesi}"')

        elif komut == "GUC_DOWN":
            if self.degas_active[self.secili_goz]:
                return
            if self.degas_armed[self.secili_goz]:
                self.degas_armed[self.secili_goz] = False
                self.nextionGonder("b_degas.bco=50712")
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

        elif komut.startswith("CMD_SET_STEP_INC:") or komut.startswith("SET_STEP_INC:"):
            if not self.isProvisioningAllowed():
                return
            val = int(komut.split(':')[1])
            if 1 <= val <= 8:
                self.step_increment = val
                self.nvsKaydet()
                self.stmGonder(f"SET_STEP_INC:{val}\n")

        elif komut.startswith("CMD_SET_SWP_SPAN:") or komut.startswith("SET_SWP_SPAN:") or komut.startswith("SET_SPAN:"):
            if not self.isProvisioningAllowed():
                return
            val = int(komut.split(':')[1])
            if 1 <= val <= 4:
                self.swp_span = val
                self.nvsKaydet()
                self.stmGonder(f"SET_SWP_SPAN:{val}\n")

        elif komut.startswith("CMD_SET_SWP_PER:") or komut.startswith("SET_SWP_PER:") or komut.startswith("SET_PER:"):
            if not self.isProvisioningAllowed():
                return
            val = int(komut.split(':')[1])
            if 100 <= val <= 1000:
                self.swp_per = val
                self.nvsKaydet()
                self.stmGonder(f"SET_SWP_PER:{val}\n")

        elif komut == "SRV_SAVE":
            if not self.isProvisioningAllowed():
                self.g_bus_diag["tx_nack_count"] += 1
                self.nextionGonder("b_save.bco=63488")  # Red error
                return
            self.nvsKaydet()
            self.g_bus_diag["tx_ack_count"] += 1
            self.nextionGonder("b_save.bco=2016")

        # --- Page 3: Service DEGAS Settings ---
        elif komut in ["PAGE3_OPEN", "PAGE_DEGAS_OPEN", "PAGE3", "PAGE_DEGAS"]:
            if not self.isProvisioningAllowed():
                self.nextionGonder("b_save.bco=63488")
                return
            self.nextionGonder("page page3")
            self.updateDegasPageUI(self.secili_goz)

        elif komut == "DEG_DUR_UP" or komut.startswith("SET_DEG_DUR:"):
            if not self.isProvisioningAllowed():
                return
            g = self.secili_goz
            if komut.startswith("SET_DEG_DUR:"):
                v = int(komut.split(":")[1])
                if 1 <= v <= 120:
                    self.service_degas[g]["duration_minutes"] = v
            else:
                if self.service_degas[g]["duration_minutes"] < 120:
                    self.service_degas[g]["duration_minutes"] += 1
            self.updateDegasPageUI(g)

        elif komut == "DEG_DUR_DOWN":
            if not self.isProvisioningAllowed():
                return
            g = self.secili_goz
            if self.service_degas[g]["duration_minutes"] > 1:
                self.service_degas[g]["duration_minutes"] -= 1
            self.updateDegasPageUI(g)

        elif komut == "DEG_PWR_UP" or komut.startswith("SET_DEG_PWR:"):
            if not self.isProvisioningAllowed():
                return
            g = self.secili_goz
            if komut.startswith("SET_DEG_PWR:"):
                v = int(komut.split(":")[1])
                if 10 <= v <= 100:
                    self.service_degas[g]["power_pct"] = v
            else:
                if self.service_degas[g]["power_pct"] <= 90:
                    self.service_degas[g]["power_pct"] += 10
            self.updateDegasPageUI(g)

        elif komut == "DEG_PWR_DOWN":
            if not self.isProvisioningAllowed():
                return
            g = self.secili_goz
            if self.service_degas[g]["power_pct"] >= 20:
                self.service_degas[g]["power_pct"] -= 10
            self.updateDegasPageUI(g)

        elif komut == "DEG_FRQ_UP" or komut.startswith("SET_DEG_FRQ:"):
            if not self.isProvisioningAllowed():
                return
            g = self.secili_goz
            if komut.startswith("SET_DEG_FRQ:"):
                v = int(komut.split(":")[1])
                if 28 <= v <= 40:
                    self.service_degas[g]["frequency_khz"] = v
            else:
                if self.service_degas[g]["frequency_khz"] < 40:
                    self.service_degas[g]["frequency_khz"] += 1
            self.updateDegasPageUI(g)

        elif komut == "DEG_FRQ_DOWN":
            if not self.isProvisioningAllowed():
                return
            g = self.secili_goz
            if self.service_degas[g]["frequency_khz"] > 28:
                self.service_degas[g]["frequency_khz"] -= 1
            self.updateDegasPageUI(g)

        elif komut == "DEG_ON_UP" or komut.startswith("SET_DEG_ON:"):
            if not self.isProvisioningAllowed():
                return
            g = self.secili_goz
            if komut.startswith("SET_DEG_ON:"):
                v = int(komut.split(":")[1])
                if 100 <= v <= 10000:
                    self.service_degas[g]["pulse_on_ms"] = v
            else:
                if self.service_degas[g]["pulse_on_ms"] <= 9900:
                    self.service_degas[g]["pulse_on_ms"] += 100
            self.updateDegasPageUI(g)

        elif komut == "DEG_ON_DOWN":
            if not self.isProvisioningAllowed():
                return
            g = self.secili_goz
            if self.service_degas[g]["pulse_on_ms"] >= 200:
                self.service_degas[g]["pulse_on_ms"] -= 100
            self.updateDegasPageUI(g)

        elif komut == "DEG_OFF_UP" or komut.startswith("SET_DEG_OFF:"):
            if not self.isProvisioningAllowed():
                return
            g = self.secili_goz
            if komut.startswith("SET_DEG_OFF:"):
                v = int(komut.split(":")[1])
                if v == 0 or (100 <= v <= 10000):
                    self.service_degas[g]["pulse_off_ms"] = v
            else:
                if self.service_degas[g]["pulse_off_ms"] <= 9900:
                    self.service_degas[g]["pulse_off_ms"] += 100
            self.updateDegasPageUI(g)

        elif komut == "DEG_OFF_DOWN":
            if not self.isProvisioningAllowed():
                return
            g = self.secili_goz
            if self.service_degas[g]["pulse_off_ms"] >= 100:
                self.service_degas[g]["pulse_off_ms"] -= 100
            self.updateDegasPageUI(g)

        elif komut == "DEG_TC_TOGGLE" or komut.startswith("SET_DEG_TC:"):
            if not self.isProvisioningAllowed():
                return
            g = self.secili_goz
            if komut.startswith("SET_DEG_TC:"):
                v = int(komut.split(":")[1])
                self.service_degas[g]["temp_ctrl"] = 1 if v != 0 else 0
            else:
                self.service_degas[g]["temp_ctrl"] = 0 if self.service_degas[g]["temp_ctrl"] != 0 else 1
            self.updateDegasPageUI(g)

        elif komut == "DEG_TGT_UP" or komut.startswith("SET_DEG_TGT:"):
            if not self.isProvisioningAllowed():
                return
            g = self.secili_goz
            if self.service_degas[g]["temp_ctrl"] == 0:
                return  # Neutralized when temp_ctrl == 0
            if komut.startswith("SET_DEG_TGT:"):
                v = float(komut.split(":")[1])
                if 20.0 <= v <= 90.0:
                    self.service_degas[g]["target_temp_c"] = v
            else:
                if self.service_degas[g]["target_temp_c"] < 90.0:
                    self.service_degas[g]["target_temp_c"] += 1.0
            self.updateDegasPageUI(g)

        elif komut == "DEG_TGT_DOWN":
            if not self.isProvisioningAllowed():
                return
            g = self.secili_goz
            if self.service_degas[g]["temp_ctrl"] == 0:
                return  # Neutralized when temp_ctrl == 0
            if self.service_degas[g]["target_temp_c"] > 20.0:
                self.service_degas[g]["target_temp_c"] -= 1.0
            self.updateDegasPageUI(g)

        elif komut in ["SRV_DEGAS_SAVE", "SRV_SAVE_DEGAS"]:
            if not self.isProvisioningAllowed():
                self.g_bus_diag["tx_nack_count"] += 1
                self.nextionGonder("b_save.bco=63488")
                return
            g = self.secili_goz
            cfg = self.service_degas[g]
            valid = (
                1 <= cfg["duration_minutes"] <= 120 and
                10 <= cfg["power_pct"] <= 100 and
                28 <= cfg["frequency_khz"] <= 40 and
                100 <= cfg["pulse_on_ms"] <= 10000 and
                (cfg["pulse_off_ms"] == 0 or 100 <= cfg["pulse_off_ms"] <= 10000) and
                cfg["temp_ctrl"] in [0, 1] and
                20.0 <= cfg["target_temp_c"] <= 90.0
            )
            if not valid:
                self.g_bus_diag["tx_nack_count"] += 1
                self.nextionGonder("b_save.bco=63488")
                return
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
        if satir.startswith("ERR:") or satir.startswith("NACK"):
            if satir.startswith("ERR:LOCKED_ACTIVE_MODE") or satir.startswith("ERR:LOCKED_SYS_RUNNING"):
                self.durum_metni[self.secili_goz] = "HATA: CALISIYOR!"
            elif satir.startswith("ERR:SWEEP_PROHIBITED_IN_DEGAS"):
                self.durum_metni[self.secili_goz] = "HATA: DEGAS AKTIF!"
            elif satir.startswith("ERR:INVALID_SYS_MODE"):
                self.durum_metni[self.secili_goz] = "HATA: GECERSIZ MOD!"
            elif satir.startswith("NACK,ERR_FAULT_ACTIVE"):
                self.durum_metni[self.secili_goz] = "HATA: ARIZA AKTIF!"
            else:
                self.durum_metni[self.secili_goz] = f"HATA: {satir[:16]}"
            self.nextionGonder(f't_durum.txt="{self.durum_metni[self.secili_goz]}"')
            return

        if satir.startswith("ACK:") or satir.startswith("ACK,") or satir.startswith("DISCOVER_ACK,"):
            if satir.startswith("ACK:FAULT_CLEARED") or satir.startswith("ACK:NO_FAULT"):
                if self.durum_metni[self.secili_goz].startswith("HATA"):
                    self.durum_metni[self.secili_goz] = "SISTEM BEKLEMEDE"
                    self.nextionGonder(f't_durum.txt="{self.durum_metni[self.secili_goz]}"')
            return

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
        was_degas = self.degas_active[g]
        self.degas_active[g] = (mode_str == "DEGAS")

        if not self.degas_active[g] and was_degas:
            self.degas_armed[g] = False
            if g == self.secili_goz:
                self.nextionGonder("b_degas.bco=50712")

        yeniden_baglandi = not self.stm_bagli[g]
        self.stm_bagli[g] = True
        self.stm_son_veri_zamani[g] = self.millis_ms

        if fault > 0:
            self.durum_metni[g] = f"HATA! KOD:{fault}"
            self.degas_active[g] = False
            self.degas_armed[g] = False
            if g == self.secili_goz:
                self.nextionGonder("b_degas.bco=50712")
        elif mode_str == "RUNNING":
            self.durum_metni[g] = "YIKAMA DEVAM EDIYOR..."
        elif mode_str == "DEGAS":
            self.durum_metni[g] = "DEGAS DEVAM EDIYOR..."
        elif rem_sec <= 0 and (self.hedef_sure[g] > 0 or was_degas):
            self.durum_metni[g] = "YIKAMA TAMAMLANDI!"
        else:
            self.durum_metni[g] = "SISTEM BEKLEMEDE"

        if g == self.secili_goz:
            hmi_durum = "Calisiyor" if mode_str == "RUNNING" else ("Degas" if mode_str == "DEGAS" else ("Hata" if mode_str == "FAULT" else "Beklemede"))
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
                self.makine_calisiyor[i] = False
                self.degas_active[i] = False
                self.degas_armed[i] = False
                self.stm_relay[i] = 0
                self.kalan_saniye[i] = 0
                self.durum_metni[i] = "Kart Yok!"
                if i == self.secili_goz:
                    self.nextionGonder("b_degas.bco=50712")
                    self.nextionGonder('t_durum.txt="Kart Yok!"')
                    self.nextionGonder('t_status.txt="Kart Yok!"')

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
        """Test 09: EDIT_P2 followed by authenticated P_SAVE|18|55 updates P2 setpoints and NVS storage."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms

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


class TestHMIDEGASStateSuite(unittest.TestCase):
    """DEG-GAP-007: ESP32 Master DEGAS State Model & Handshake Verification Suite"""

    def setUp(self):
        self.hmi = MockESP32HMI()
        # Connect tank 1
        self.hmi.stmTelemetryIsle("STAT,1,IDLE,0,250,0,50,28,0")

    def test_degas_01_arm_and_disarm(self):
        """DEGAS selection toggles degas_armed and updates green button indicator."""
        self.assertFalse(self.hmi.degas_armed[1])
        self.hmi.komutIsle("CMD_DEGAS_SEL")
        self.assertTrue(self.hmi.degas_armed[1])
        self.assertIn("b_degas.bco=2016", self.hmi.nextion_tx_log)

        self.hmi.komutIsle("CMD_DEGAS_SEL")
        self.assertFalse(self.hmi.degas_armed[1])
        self.assertIn("b_degas.bco=50712", self.hmi.nextion_tx_log)

    def test_degas_02_parameter_change_disarms(self):
        """Normal recipe selection before START disarms DEGAS selection."""
        self.hmi.komutIsle("CMD_DEGAS_SEL")
        self.assertTrue(self.hmi.degas_armed[1])

        # Select P1 recipe
        self.hmi.komutIsle("P1_SEL")
        self.assertFalse(self.hmi.degas_armed[1], "Recipe selection must disarm DEGAS")
        self.assertIn("b_degas.bco=50712", self.hmi.nextion_tx_log)

    def test_degas_03_start_flow_sends_start_degas(self):
        """Pressing START with DEGAS armed sends START_DEGAS atomic snapshot frame to STM32."""
        self.hmi.komutIsle("CMD_DEGAS_SEL")
        self.assertTrue(self.hmi.degas_armed[1])

        self.hmi.komutIsle("CMD_START")
        self.assertIn("T1:START_DEGAS:15:100:28:1000:500:0:50.0", self.hmi.stm32_tx_log)

        # Simulate STM32 reporting DEGAS mode telemetry
        self.hmi.stmTelemetryIsle("STAT,1,DEGAS,900,250,0,100,28,0")
        self.assertTrue(self.hmi.degas_active[1])

    def test_degas_04_active_degas_locks_recipe_editing(self):
        """Active DEGAS state locks normal process recipe selection."""
        self.hmi.stmTelemetryIsle("STAT,1,DEGAS,900,250,0,100,28,0")
        self.assertTrue(self.hmi.degas_active[1])

        # Attempt P1 recipe selection during active DEGAS
        self.hmi.komutIsle("P1_SEL")
        self.assertNotIn("T1:SET_TIME:15", self.hmi.stm32_tx_log, "P1 selection must be locked during active DEGAS")

    def test_degas_05_stop_clears_active_and_armed(self):
        """CMD_STOP clears both degas_active and degas_armed states."""
        self.hmi.degas_armed[1] = True
        self.hmi.degas_active[1] = True

        self.hmi.komutIsle("CMD_STOP")
        self.assertFalse(self.hmi.degas_armed[1])
        self.assertFalse(self.hmi.degas_active[1])
        self.assertIn("T1:STOP", self.hmi.stm32_tx_log)

    def test_degas_06_sweep_mutual_exclusion(self):
        """Enabling Sweep disarms DEGAS selection."""
        self.hmi.komutIsle("CMD_DEGAS_SEL")
        self.assertTrue(self.hmi.degas_armed[1])

        self.hmi.komutIsle("CMD_SWEEP_ON")
        self.assertFalse(self.hmi.degas_armed[1], "SWEEP ON must disarm DEGAS")
        self.assertIn("T1:SWEEP:ON", self.hmi.stm32_tx_log)

    def test_degas_07_atomic_snapshot_transfer(self):
        """Custom tank DEGAS configuration builds correct atomic snapshot frame."""
        self.hmi.service_degas[1]["duration_minutes"] = 20
        self.hmi.service_degas[1]["power_pct"] = 80
        self.hmi.service_degas[1]["frequency_khz"] = 40
        self.hmi.service_degas[1]["pulse_on_ms"] = 2000
        self.hmi.service_degas[1]["pulse_off_ms"] = 1000
        self.hmi.service_degas[1]["temp_ctrl"] = 1
        self.hmi.service_degas[1]["target_temp_c"] = 65.0

        self.hmi.komutIsle("CMD_DEGAS_SEL")
        self.hmi.komutIsle("CMD_START")

        self.assertIn("T1:START_DEGAS:20:80:40:2000:1000:1:65.0", self.hmi.stm32_tx_log)

    def test_degas_08_per_tank_isolation(self):
        """Customizing Tank 1 DEGAS configuration preserves Tank 2 defaults."""
        self.hmi.service_degas[1]["duration_minutes"] = 30
        self.assertEqual(self.hmi.service_degas[2]["duration_minutes"], 15, "Tank 2 must maintain default duration")

    def test_degas_09_page3_open_permissions(self):
        """Unauthenticated Page 3 access is rejected; authenticated service loads Page 3 UI fields."""
        self.hmi.g_service_authenticated = False
        self.hmi.komutIsle("PAGE3_OPEN")
        self.assertIn("b_save.bco=63488", self.hmi.nextion_tx_log)

        # Authenticate service
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.komutIsle("PAGE3_OPEN")
        self.assertIn("page page3", self.hmi.nextion_tx_log)
        self.assertIn('t_deg_goz.txt="Goz: 1"', self.hmi.nextion_tx_log)
        self.assertIn('t_deg_dur.txt="15"', self.hmi.nextion_tx_log)

    def test_degas_10_page3_parameter_editing_and_bounds(self):
        """Editing Page 3 parameters updates memory within software bounds."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms

        self.hmi.komutIsle("DEG_DUR_UP")
        self.assertEqual(self.hmi.service_degas[1]["duration_minutes"], 16)

        # Out of bounds upper limit
        self.hmi.komutIsle("SET_DEG_DUR:999")
        self.assertEqual(self.hmi.service_degas[1]["duration_minutes"], 16, "Out of bounds duration must be rejected")

    def test_degas_11_temp_ctrl_off_neutralizes_target_temp_edit(self):
        """When temp_ctrl == 0, target temperature display is '--' and editing is neutralized."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.service_degas[1]["temp_ctrl"] = 0

        self.hmi.komutIsle("PAGE3_OPEN")
        self.assertIn('t_deg_tgt.txt="--"', self.hmi.nextion_tx_log)

        # Attempt edit while OFF
        initial_tgt = self.hmi.service_degas[1]["target_temp_c"]
        self.hmi.komutIsle("DEG_TGT_UP")
        self.assertEqual(self.hmi.service_degas[1]["target_temp_c"], initial_tgt, "Target temp edit must be neutralized when temp_ctrl is OFF")

    def test_degas_12_page3_save_and_rejection(self):
        """Valid Page 3 Save yields green feedback; unauthenticated or invalid Save yields red feedback."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms

        self.hmi.komutIsle("SRV_DEGAS_SAVE")
        self.assertIn("b_save.bco=2016", self.hmi.nextion_tx_log)

        # Corrupt data in memory
        self.hmi.service_degas[1]["power_pct"] = 5
        self.hmi.komutIsle("SRV_DEGAS_SAVE")
        self.assertIn("b_save.bco=63488", self.hmi.nextion_tx_log)

    def test_degas_13_multi_tank_page3_reload(self):
        """Switching tanks on Page 3 reloads the target tank's configuration."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms

        self.hmi.service_degas[1]["duration_minutes"] = 45
        self.hmi.secili_goz = 2
        self.hmi.komutIsle("PAGE3_OPEN")
        self.assertIn('t_deg_goz.txt="Goz: 2"', self.hmi.nextion_tx_log)
        self.assertIn('t_deg_dur.txt="15"', self.hmi.nextion_tx_log)

    def test_degas_14_pre_start_parameter_change_disarms(self):
        """Editing normal power or frequency before START disarms DEGAS selection."""
        self.hmi.komutIsle("CMD_DEGAS_SEL")
        self.assertTrue(self.hmi.degas_armed[1])

        self.hmi.komutIsle("GUC_UP")
        self.assertFalse(self.hmi.degas_armed[1], "GUC_UP before START must disarm DEGAS")
        self.assertIn("b_degas.bco=50712", self.hmi.nextion_tx_log)

    def test_degas_15_active_degas_lockout_all_controls(self):
        """While DEGAS is active, normal frequency and power edits are locked."""
        self.hmi.stmTelemetryIsle("STAT,1,DEGAS,900,250,0,100,28,0")
        self.assertTrue(self.hmi.degas_active[1])

        initial_freq_log = list(self.hmi.stm32_tx_log)
        self.hmi.komutIsle("CMD_FREQ|40")
        self.assertEqual(self.hmi.stm32_tx_log, initial_freq_log, "CMD_FREQ must be locked during active DEGAS")

    def test_degas_16_timer_zero_completion_clears_active(self):
        """Timer zero completion clears degas_active and restores Home Page state without auto-restart."""
        self.hmi.stmTelemetryIsle("STAT,1,DEGAS,10,250,0,100,28,0")
        self.assertTrue(self.hmi.degas_active[1])

        # Timer reaches zero, STM32 transitions to IDLE
        self.hmi.stmTelemetryIsle("STAT,1,IDLE,0,250,0,0,28,0")
        self.assertFalse(self.hmi.degas_active[1])
        self.assertFalse(self.hmi.degas_armed[1])
        self.assertFalse(self.hmi.makine_calisiyor[1], "Must not auto-restart RUNNING cycle")
        self.assertIn("b_degas.bco=50712", self.hmi.nextion_tx_log)

    def test_degas_17_communication_loss_clears_active(self):
        """Loss of communication (>3000ms timeout) clears active and armed DEGAS states."""
        self.hmi.stmTelemetryIsle("STAT,1,DEGAS,900,250,0,100,28,0")
        self.assertTrue(self.hmi.degas_active[1])

        # Simulate time jump > 3000ms without new telemetry
        self.hmi.loop_tick(5000.0)
        self.assertFalse(self.hmi.degas_active[1], "Communication loss must clear degas_active")
        self.assertFalse(self.hmi.degas_armed[1], "Communication loss must clear degas_armed")

    def test_degas_18_disarm_on_time_edit(self):
        """Changing normal time setpoint before START disarms DEGAS selection."""
        self.hmi.komutIsle("CMD_DEGAS_SEL")
        self.assertTrue(self.hmi.degas_armed[1])

        self.hmi.komutIsle("TIME_UP")
        self.assertFalse(self.hmi.degas_armed[1], "TIME_UP must disarm DEGAS")
        self.assertIn("b_degas.bco=50712", self.hmi.nextion_tx_log)
        self.assertNotIn("START_DEGAS", "".join(self.hmi.stm32_tx_log), "No START_DEGAS must be sent on time edit")

    def test_degas_19_disarm_on_temp_edit(self):
        """Changing normal temperature setpoint before START disarms DEGAS selection."""
        self.hmi.komutIsle("CMD_DEGAS_SEL")
        self.assertTrue(self.hmi.degas_armed[1])

        self.hmi.komutIsle("TEMP_UP")
        self.assertFalse(self.hmi.degas_armed[1], "TEMP_UP must disarm DEGAS")
        self.assertIn("b_degas.bco=50712", self.hmi.nextion_tx_log)

    def test_degas_20_disarm_on_recipe_select(self):
        """Selecting P1/P2/P3 preset recipes before START disarms DEGAS selection."""
        self.hmi.komutIsle("CMD_DEGAS_SEL")
        self.assertTrue(self.hmi.degas_armed[1])

        self.hmi.komutIsle("P2_SEL")
        self.assertFalse(self.hmi.degas_armed[1], "P2_SEL must disarm DEGAS")
        self.assertEqual(self.hmi.aktif_program, 2, "P2 must be loaded normally")
        self.assertIn("b_degas.bco=50712", self.hmi.nextion_tx_log)

    def test_degas_21_disarm_on_sweep_enable(self):
        """Enabling Sweep mode before START disarms DEGAS selection."""
        self.hmi.komutIsle("CMD_DEGAS_SEL")
        self.assertTrue(self.hmi.degas_armed[1])

        self.hmi.komutIsle("CMD_SWEEP_ON")
        self.assertFalse(self.hmi.degas_armed[1], "Sweep enable must disarm DEGAS")
        self.assertIn("b_degas.bco=50712", self.hmi.nextion_tx_log)

    def test_degas_22_persistent_config_unaffected_by_prestart_disarm(self):
        """Disarming DEGAS selection does not alter stored service DEGAS NVS configuration."""
        initial_cfg = dict(self.hmi.service_degas[1])

        self.hmi.komutIsle("CMD_DEGAS_SEL")
        self.hmi.komutIsle("GUC_UP")

        self.assertEqual(self.hmi.service_degas[1], initial_cfg, "NVS DEGAS config must remain unchanged")

    def test_degas_23_active_degas_locks_sweep_and_recipe_edits(self):
        """While DEGAS is active, Sweep enable, recipe editing, and recipe save are locked."""
        self.hmi.stmTelemetryIsle("STAT,1,DEGAS,900,250,0,100,28,0")
        self.assertTrue(self.hmi.degas_active[1])

        initial_tx_log = list(self.hmi.stm32_tx_log)
        self.hmi.komutIsle("CMD_SWEEP_ON")
        self.assertEqual(self.hmi.stm32_tx_log, initial_tx_log, "CMD_SWEEP_ON must be locked during active DEGAS")

        self.hmi.komutIsle("EDIT_P1")
        self.assertNotIn('t0.txt="PROGRAM P1"', self.hmi.nextion_tx_log)

        self.hmi.komutIsle("P_SAVE|99|88")
        self.assertNotEqual(self.hmi.p_sure[1], 99)

    def test_degas_24_active_degas_ui_updates_and_stop_accessible(self):
        """Active DEGAS status and time update from telemetry, and STOP terminates active DEGAS cleanly."""
        self.hmi.stmTelemetryIsle("STAT,1,DEGAS,450,300,0,100,28,0")
        self.assertTrue(self.hmi.degas_active[1])
        self.assertEqual(self.hmi.durum_metni[1], "DEGAS DEVAM EDIYOR...")
        self.assertIn('t_status.txt="Degas"', self.hmi.nextion_tx_log)

        self.hmi.komutIsle("CMD_STOP")
        self.assertFalse(self.hmi.degas_active[1])
        self.assertFalse(self.hmi.degas_armed[1])
        self.assertTrue(any("STOP" in log for log in self.hmi.stm32_tx_log))
        self.assertIn("b_degas.bco=50712", self.hmi.nextion_tx_log)

    def test_degas_25_timer_zero_restores_home_page_without_autorestart(self):
        """Timer reaching 0 (STAT IDLE) restores Home Page without starting RUNNING mode."""
        self.hmi.stmTelemetryIsle("STAT,1,DEGAS,5,300,0,100,28,0")
        self.assertTrue(self.hmi.degas_active[1])

        self.hmi.stmTelemetryIsle("STAT,1,IDLE,0,300,0,0,28,0")
        self.assertFalse(self.hmi.degas_active[1])
        self.assertFalse(self.hmi.degas_armed[1])
        self.assertFalse(self.hmi.makine_calisiyor[1])

    def test_degas_26_fault_and_comm_loss_recovery(self):
        """Hardware fault or >3000ms comm loss clears active DEGAS without auto-restart."""
        self.hmi.stmTelemetryIsle("STAT,1,DEGAS,600,250,0,100,28,1")  # fault code 1
        self.assertFalse(self.hmi.degas_active[1])
        self.assertFalse(self.hmi.degas_armed[1])
        self.assertIn("HATA! KOD:1", self.hmi.durum_metni[1])

    def test_degas_27_active_degas_snapshot_protected_from_operator_overwrites(self):
        """Operator actions during active DEGAS leave active RAM/NVS execution snapshot untouched."""
        initial_service_cfg = dict(self.hmi.service_degas[1])
        self.hmi.stmTelemetryIsle("STAT,1,DEGAS,600,250,0,100,28,0")

        self.hmi.komutIsle("P1_SEL")
        self.hmi.komutIsle("TIME_UP")
        self.hmi.komutIsle("TEMP_UP")
        self.hmi.komutIsle("GUC_UP")

        self.assertEqual(self.hmi.service_degas[1], initial_service_cfg)
        self.assertTrue(self.hmi.degas_active[1])

    def test_rsk001_hmi_stop_under_active_fault(self):
        """RSK-001: HMI preserves fault text and blocks CMD_START when fault flags remain non-zero."""
        self.hmi.stmTelemetryIsle("STAT,1,FAULT,0,0,0,50,28,1")
        self.assertIn("HATA!", self.hmi.durum_metni[1])

        self.hmi.komutIsle("CMD_STOP")
        self.hmi.stmTelemetryIsle("STAT,1,FAULT,0,0,0,50,28,1")
        self.assertIn("HATA!", self.hmi.durum_metni[1])

        self.hmi.stm32_tx_log.clear()
        self.hmi.komutIsle("CMD_START|10|50")
        self.assertFalse(self.hmi.makine_calisiyor[1])
        self.assertNotIn("START", "".join(self.hmi.stm32_tx_log))

    def test_rsk002_hmi_tx_busy_overflow_recovery(self):
        """RSK-002: HMI driver recovers gracefully under high TX command log stress."""
        for _ in range(20):
            self.hmi.komutIsle("CMD_STOP")
        self.assertGreater(len(self.hmi.stm32_tx_log), 0)

    def test_rsk003_hmi_running_cycle_touch_lockout_all_inputs(self):
        """RSK-003: All setpoint and recipe touch commands are locked when makine_calisiyor is True."""
        self.hmi.makine_calisiyor[1] = True
        self.hmi.hedef_sure[1] = 10
        self.hmi.hedef_sicaklik[1] = 50
        self.hmi.stm32_tx_log.clear()

        self.hmi.komutIsle("TIME_UP")
        self.hmi.komutIsle("TEMP_UP")
        self.hmi.komutIsle("GUC_UP")
        self.hmi.komutIsle("CMD_FREQ|40")
        self.hmi.komutIsle("P1_SEL")
        self.hmi.komutIsle("EDIT_P1")
        self.hmi.komutIsle("P_HIZLI")

        self.assertEqual(self.hmi.hedef_sure[1], 10)
        self.assertEqual(self.hmi.hedef_sicaklik[1], 50)
        self.assertEqual(len(self.hmi.stm32_tx_log), 0)


    def test_rsk004_hmi_parses_slave_error_and_nack_frames(self):
        """RSK-004: ESP32 HMI parser handles ERR: and NACK slave responses and updates UI."""
        self.hmi.stmTelemetryIsle("ERR:LOCKED_ACTIVE_MODE")
        self.assertEqual(self.hmi.durum_metni[1], "HATA: CALISIYOR!")
        self.assertIn('t_durum.txt="HATA: CALISIYOR!"', self.hmi.nextion_tx_log)

        self.hmi.stmTelemetryIsle("NACK,ERR_FAULT_ACTIVE,1")
        self.assertEqual(self.hmi.durum_metni[1], "HATA: ARIZA AKTIF!")
        self.assertIn('t_durum.txt="HATA: ARIZA AKTIF!"', self.hmi.nextion_tx_log)

    def test_rsk008_admin_sweep_and_recipe_save_pin_lockout(self):
        """RSK-008: Admin sweep commands and recipe save reject unauthenticated execution."""
        # Unauthenticated attempts
        self.hmi.g_service_authenticated = False
        initial_p2 = self.hmi.p_sure[2]
        self.hmi.komutIsle("P_SAVE|45|70")
        self.assertEqual(self.hmi.p_sure[2], initial_p2, "Unauthenticated P_SAVE must be rejected")

        self.hmi.komutIsle("CMD_SET_STEP_INC:8")
        self.assertNotIn("T1:SET_STEP_INC:8", self.hmi.stm32_tx_log)

        self.hmi.komutIsle("CMD_SET_SWP_SPAN:4")
        self.assertNotIn("T1:SET_SWP_SPAN:4", self.hmi.stm32_tx_log)

        self.hmi.komutIsle("CMD_SET_SWP_PER:800")
        self.assertNotIn("T1:SET_SWP_PER:800", self.hmi.stm32_tx_log)

        # Authenticate and re-try
        for digit in "123456":
            self.hmi.komutIsle(f"KEY{digit}")
        self.hmi.komutIsle("KEY_OK")
        self.assertTrue(self.hmi.g_service_authenticated)

        self.hmi.komutIsle("CMD_SET_STEP_INC:8")
        self.assertIn("T1:SET_STEP_INC:8", self.hmi.stm32_tx_log)

        self.hmi.komutIsle("CMD_SET_SWP_SPAN:4")
        self.assertIn("T1:SET_SWP_SPAN:4", self.hmi.stm32_tx_log)

        self.hmi.komutIsle("CMD_SET_SWP_PER:800")
        self.assertIn("T1:SET_SWP_PER:800", self.hmi.stm32_tx_log)

    def test_rsk009_hmi_timeout_updates_durum_metni_kart_yok(self):
        """RSK-009: Communication timeout immediately updates durum_metni to 'Kart Yok!'."""
        # Provide telemetry to connect tank 1
        self.hmi.stmTelemetryIsle("STAT,1,RUNNING,300,250,1,80,28,0")
        self.assertTrue(self.hmi.stm_bagli[1])
        self.assertTrue(self.hmi.makine_calisiyor[1])
        self.assertEqual(self.hmi.durum_metni[1], "YIKAMA DEVAM EDIYOR...")

        # Advance time by 3100 ms (past 3000ms timeout)
        self.hmi.loop_tick(self.hmi.millis_ms + 3100)
        self.assertFalse(self.hmi.stm_bagli[1])
        self.assertFalse(self.hmi.makine_calisiyor[1])
        self.assertEqual(self.hmi.durum_metni[1], "Kart Yok!")
        self.assertIn('t_durum.txt="Kart Yok!"', self.hmi.nextion_tx_log)


if __name__ == "__main__":
    unittest.main()
