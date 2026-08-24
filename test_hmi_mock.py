"""
Phase 5.4 Offline Nextion HMI Simulation & Verification Suite
EAGLEULTRASONİK Project — ESP32 Master Firmware Emulator & New Page Architecture Tests

Emulates Nextion HMI touch screen protocol, menu pages (0, 2, 3, 4, 5, 6, 7),
command dispatcher, service authentication ("123456"), 5-minute inactivity session timeout,
NVS recipe & per-tank persistence, 3000ms offline card connection watchdog,
provisioning interlocks, Sweep / DEGAS mutual exclusions, and cyclic service navigation.

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

    NEXTION_COLOR_RED = 63488
    NEXTION_COLOR_GREEN = 2016
    NEXTION_COLOR_DEFAULT = 50712

    def __init__(self):
        self.millis_ms: float = 0.0

        # System state variables
        self.secili_goz: int = 1
        self.temp_goz: int = 1
        self.aktif_sayfa: int = 0
        self.current_service_page: int = 5
        self.aktif_program: int = 0
        self.duzenlenen_program: int = 1

        # Preset recipe templates (P1, P2, P3)
        self.p_sure = [0, 15, 20, 25]
        self.p_sicaklik = [0, 40, 50, 60]
        self.p_sweep = [0, 0, 0, 0]  # Stored sweep_enabled (0=OFF, 1=ON)

        # Page 2 Recipe Draft Edit State
        self.edit_p_sure = [0, 15, 20, 25]
        self.edit_p_sicaklik = [0, 40, 50, 60]
        self.edit_p_sweep = [0, 0, 0, 0]

        # Service auth state
        self.girilen_sifre: str = ""
        self.dogru_sifre: str = "123456"
        self.g_service_authenticated: bool = False
        self.service_auth_time: float = 0.0

        # Global settings
        self.max_goz_sayisi: int = 3

        # Tank-scoped service configuration arrays (1..10)
        self.guc_seviyesi = [50] * self.MAX_GOZ
        self.kart_id = [i if i > 0 else 1 for i in range(self.MAX_GOZ)]

        # Tank-scoped Sweep configuration
        self.service_sweep = [
            {
                "enabled": 0,
                "span_khz": 2,
                "period_ms": 400,
                "step_increment": 4
            }
            for _ in range(self.MAX_GOZ)
        ]

        # Tank-scoped DEGAS configuration
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

        # Service Draft Buffers (Page 5, 6, 7, 8)
        self.edit_max_goz_sayisi = 3
        self.edit_guc_seviyesi = [50] * self.MAX_GOZ
        self.edit_kart_id = [i if i > 0 else 1 for i in range(self.MAX_GOZ)]
        self.edit_service_sweep = [
            {
                "enabled": 0,
                "span_khz": 2,
                "period_ms": 400,
                "step_increment": 4
            }
            for _ in range(self.MAX_GOZ)
        ]
        self.edit_service_degas = [
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

        # Per-tank runtime state arrays (index 1..10)
        self.makine_calisiyor = [False] * self.MAX_GOZ
        self.degas_armed = [False] * self.MAX_GOZ
        self.degas_active = [False] * self.MAX_GOZ
        self.runtime_sweep = [False] * self.MAX_GOZ
        self.durum_metni = ["SISTEM BEKLEMEDE"] * self.MAX_GOZ
        self.hedef_sure = [0] * self.MAX_GOZ
        self.hedef_sicaklik = [0] * self.MAX_GOZ
        self.kalan_saniye = [0] * self.MAX_GOZ
        self.anlik_sicaklik = [24.0] * self.MAX_GOZ

        self.stm_fault = [0] * self.MAX_GOZ
        self.stm_relay = [0] * self.MAX_GOZ
        self.stm_pwr = [0] * self.MAX_GOZ
        self.stm_freq = [28] * self.MAX_GOZ
        self.stm_prov_state = [2] * self.MAX_GOZ
        self.stm_bagli = [False] * self.MAX_GOZ
        self.stm_son_veri_zamani = [0.0] * self.MAX_GOZ

        self.sonGuncellemeZamani: float = 0.0

        # Bus Diagnostics Counters
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
            "pS1": 15, "pT1": 40, "pSw1": 0,
            "pS2": 20, "pT2": 50, "pSw2": 0,
            "pS3": 25, "pT3": 60, "pSw3": 0,
            "maxgoz": 3
        }
        for g in range(1, self.MAX_GOZ):
            self.nvs_ultra[f"guc_{g}"] = 50
            self.nvs_ultra[f"kid_{g}"] = g
            self.nvs_ultra[f"sw_en_{g}"] = 0
            self.nvs_ultra[f"sw_sp_{g}"] = 2
            self.nvs_ultra[f"sw_pr_{g}"] = 400
            self.nvs_ultra[f"sw_st_{g}"] = 4

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
        target_id = self.kart_id[self.secili_goz] if (1 <= self.secili_goz < self.MAX_GOZ and self.kart_id[self.secili_goz] >= 1) else self.secili_goz
        adresli = f"T{target_id}:{komut}"
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
            self.debug_tx_log.append("--> HATA: SERVIS OTURUM SURESI DOLDU (Timeout > 5dk)")
            return False
        if self.isAnyTankRunning():
            self.debug_tx_log.append("--> HATA: CALISAN TANK VAR! PROVISIONING KILITLI (SYS_MODE_RUNNING INTERLOCK)")
            return False
        return True

    def updatePage0ButtonColors(self):
        deg_color = self.NEXTION_COLOR_GREEN if (self.degas_armed[self.secili_goz] or self.degas_active[self.secili_goz]) else self.NEXTION_COLOR_DEFAULT
        self.nextionGonder(f"b_degas.bco={deg_color}")
        self.nextionGonder(f"b_deg.bco={deg_color}")

        swe_color = self.NEXTION_COLOR_GREEN if self.runtime_sweep[self.secili_goz] else self.NEXTION_COLOR_DEFAULT
        self.nextionGonder(f"b_sweep.bco={swe_color}")
        self.nextionGonder(f"b_swe.bco={swe_color}")

        self.nextionGonder(f"b_prog_p1.bco={self.NEXTION_COLOR_GREEN if self.aktif_program == 1 else self.NEXTION_COLOR_DEFAULT}")
        self.nextionGonder(f"b_p1.bco={self.NEXTION_COLOR_GREEN if self.aktif_program == 1 else self.NEXTION_COLOR_DEFAULT}")

        self.nextionGonder(f"b_prog_p2.bco={self.NEXTION_COLOR_GREEN if self.aktif_program == 2 else self.NEXTION_COLOR_DEFAULT}")
        self.nextionGonder(f"b_p2.bco={self.NEXTION_COLOR_GREEN if self.aktif_program == 2 else self.NEXTION_COLOR_DEFAULT}")

        self.nextionGonder(f"b_prog_p3.bco={self.NEXTION_COLOR_GREEN if self.aktif_program == 3 else self.NEXTION_COLOR_DEFAULT}")
        self.nextionGonder(f"b_p3.bco={self.NEXTION_COLOR_GREEN if self.aktif_program == 3 else self.NEXTION_COLOR_DEFAULT}")

        self.nextionGonder(f"b_prog_fp.bco={self.NEXTION_COLOR_GREEN if self.aktif_program == 4 else self.NEXTION_COLOR_DEFAULT}")
        self.nextionGonder(f"b_fp.bco={self.NEXTION_COLOR_GREEN if self.aktif_program == 4 else self.NEXTION_COLOR_DEFAULT}")

    def updatePage0UI(self):
        self.nextionGonder(f'b_goz_sec.txt="Goz: {self.secili_goz}"')
        self.nextionGonder(f'b_goz.txt="Goz: {self.secili_goz}"')
        self.nextionGonder(f't_set_sure.txt="{self.hedef_sure[self.secili_goz]:02d}"')
        self.nextionGonder(f't_set_sic.txt="{self.hedef_sicaklik[self.secili_goz]}"')
        freq_txt = "40k" if self.stm_freq[self.secili_goz] == 40 else "28k"
        self.nextionGonder(f'b_freq.txt="{freq_txt}"')
        self.nextionGonder(f'b_frq.txt="{freq_txt}"')
        self.updatePage0ButtonColors()

    def initRecipeEditBuffers(self):
        for i in range(1, 4):
            self.edit_p_sure[i] = self.p_sure[i]
            self.edit_p_sicaklik[i] = self.p_sicaklik[i]
            self.edit_p_sweep[i] = self.p_sweep[i]

    def discardRecipeEditBuffers(self, prog: int = 0):
        if 1 <= prog <= 3:
            self.edit_p_sure[prog] = self.p_sure[prog]
            self.edit_p_sicaklik[prog] = self.p_sicaklik[prog]
            self.edit_p_sweep[prog] = self.p_sweep[prog]
        else:
            self.initRecipeEditBuffers()

    def updatePage2UI(self, prog: int):
        if prog < 1 or prog > 3:
            prog = 1
        self.nextionGonder(f't_prog_baslik.txt="PROGRAM P{prog}"')
        self.nextionGonder(f't0.txt="PROGRAM P{prog}"')
        self.nextionGonder(f't_set_sure.txt="{self.edit_p_sure[prog]:02d}"')
        self.nextionGonder(f't_set_sic.txt="{self.edit_p_sicaklik[prog]}"')

        swe_color = self.NEXTION_COLOR_GREEN if self.edit_p_sweep[prog] != 0 else self.NEXTION_COLOR_DEFAULT
        self.nextionGonder(f"b_edit_sweep.bco={swe_color}")
        self.nextionGonder(f"b_swe.bco={swe_color}")

        self.nextionGonder(f"b_edit_p1.bco={self.NEXTION_COLOR_GREEN if prog == 1 else self.NEXTION_COLOR_DEFAULT}")
        self.nextionGonder(f"b_p1.bco={self.NEXTION_COLOR_GREEN if prog == 1 else self.NEXTION_COLOR_DEFAULT}")

        self.nextionGonder(f"b_edit_p2.bco={self.NEXTION_COLOR_GREEN if prog == 2 else self.NEXTION_COLOR_DEFAULT}")
        self.nextionGonder(f"b_p2.bco={self.NEXTION_COLOR_GREEN if prog == 2 else self.NEXTION_COLOR_DEFAULT}")

        self.nextionGonder(f"b_edit_p3.bco={self.NEXTION_COLOR_GREEN if prog == 3 else self.NEXTION_COLOR_DEFAULT}")
        self.nextionGonder(f"b_p3.bco={self.NEXTION_COLOR_GREEN if prog == 3 else self.NEXTION_COLOR_DEFAULT}")

    def initServiceEditBuffers(self):
        self.edit_max_goz_sayisi = self.max_goz_sayisi
        self.edit_guc_seviyesi = list(self.guc_seviyesi)
        self.edit_kart_id = list(self.kart_id)
        self.edit_service_sweep = [dict(s) for s in self.service_sweep]
        self.edit_service_degas = [dict(d) for d in self.service_degas]

    def discardServiceEditBuffers(self, g: int = 0):
        if 1 <= g < self.MAX_GOZ:
            self.edit_guc_seviyesi[g] = self.guc_seviyesi[g]
            self.edit_kart_id[g] = self.kart_id[g]
            self.edit_service_sweep[g] = dict(self.service_sweep[g])
            self.edit_service_degas[g] = dict(self.service_degas[g])
        else:
            self.initServiceEditBuffers()

    def updatePage5UI(self, g: int):
        if g < 1 or g >= self.MAX_GOZ:
            return
        committed_goz = self.kart_id[g] if self.kart_id[g] >= 1 else g
        draft_id = self.edit_kart_id[g] if self.edit_kart_id[g] >= 1 else g
        self.nextionGonder(f't_srv_goz.txt="{committed_goz}"')
        self.nextionGonder(f't_srv_guc.txt="{self.edit_guc_seviyesi[g]}"')
        self.nextionGonder(f't_srv_id.txt="{draft_id}"')
        self.nextionGonder(f't_srv_max.txt="{self.edit_max_goz_sayisi}"')
        # Backward compatibility
        self.nextionGonder(f't0.txt="SERVIS AYARLARI - GOZ {committed_goz}"')
        self.nextionGonder(f't_goz_num.txt="{g}"')
        self.nextionGonder(f't_guc.txt="{self.edit_guc_seviyesi[g]}"')
        self.nextionGonder(f't_id.txt="{draft_id}"')
        self.nextionGonder(f't_max.txt="{self.edit_max_goz_sayisi}"')

    def updatePage6UI(self, g: int):
        if g < 1 or g >= self.MAX_GOZ:
            return
        cfg = self.edit_service_sweep[g]
        committed_goz = self.kart_id[g] if self.kart_id[g] >= 1 else g
        self.nextionGonder(f't_swp_goz.txt="{committed_goz}"')
        self.nextionGonder(f't_swp_span.txt="{cfg["span_khz"]}"')
        self.nextionGonder(f't_swp_per.txt="{cfg["period_ms"]}"')
        self.nextionGonder(f't_swp_step.txt="{cfg["step_increment"]}"')
        # Backward compatibility
        self.nextionGonder(f't_goz_num.txt="{g}"')
        self.nextionGonder(f't_swp_period.txt="{cfg["period_ms"]}"')

    def updatePage7UI(self, g: int):
        if g < 1 or g >= self.MAX_GOZ:
            return
        cfg = self.edit_service_degas[g]
        committed_goz = self.kart_id[g] if self.kart_id[g] >= 1 else g
        self.nextionGonder(f't_deg_goz.txt="{committed_goz}"')
        self.nextionGonder(f't_deg_dur.txt="{cfg["duration_minutes"]}"')
        self.nextionGonder(f't_deg_pow.txt="{cfg["power_pct"]}"')
        self.nextionGonder(f't_deg_freq.txt="{cfg["frequency_khz"]}"')
        # Backward compatibility
        self.nextionGonder(f't_goz_num.txt="{g}"')
        self.nextionGonder(f't_deg_pwr.txt="{cfg["power_pct"]}"')
        self.nextionGonder(f't_deg_frq.txt="{cfg["frequency_khz"]}"')

    def updatePage8UI(self, g: int):
        if g < 1 or g >= self.MAX_GOZ:
            return
        cfg = self.edit_service_degas[g]
        self.nextionGonder(f't_deg_pon.txt="{cfg["pulse_on_ms"]}"')
        self.nextionGonder(f't_deg_poff.txt="{cfg["pulse_off_ms"]}"')
        tc_str = "ACIK" if cfg["temp_ctrl"] != 0 else "KAPALI"
        self.nextionGonder(f't_deg_tctrl.txt="{tc_str}"')
        if cfg["temp_ctrl"] != 0:
            self.nextionGonder(f't_deg_temp.txt="{int(cfg["target_temp_c"])}"')
            self.nextionGonder(f't_deg_tgt.txt="{int(cfg["target_temp_c"])}"')
        else:
            self.nextionGonder('t_deg_temp.txt="--"')
            self.nextionGonder('t_deg_tgt.txt="--"')
        # Backward compatibility
        self.nextionGonder(f't_deg_on.txt="{cfg["pulse_on_ms"]}"')
        self.nextionGonder(f't_deg_off.txt="{cfg["pulse_off_ms"]}"')
        self.nextionGonder(f't_deg_tc.txt="{"ON" if cfg["temp_ctrl"] != 0 else "OFF"}"')

    def updateDegasPageUI(self, g: int):
        self.updatePage7UI(g)

    def disarmDegasIfArmed(self, g: int):
        if 1 <= g < self.MAX_GOZ:
            if self.degas_armed[g] and not self.degas_active[g]:
                self.degas_armed[g] = False
                if g == self.secili_goz:
                    self.nextionGonder(f"b_deg.bco={self.NEXTION_COLOR_DEFAULT}")
                    self.nextionGonder(f"b_degas.bco={self.NEXTION_COLOR_DEFAULT}")

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
        g = self.secili_goz
        self.stmGonder(f"SET_TIME:{self.hedef_sure[g]}\n")
        self.stmGonder(f"SET_TEMP:{self.hedef_sicaklik[g]}\n")
        self.stmGonder(f"SET_POWER:{self.guc_seviyesi[g]}\n")
        self.stmGonder(f"SET_FREQ:{self.stm_freq[g]}\n")
        self.stmGonder(f"SET_STEP_INC:{self.service_sweep[g]['step_increment']}\n")
        self.stmGonder(f"SET_SWP_SPAN:{self.service_sweep[g]['span_khz']}\n")
        self.stmGonder(f"SET_SWP_PER:{self.service_sweep[g]['period_ms']}\n")
        if self.runtime_sweep[g]:
            self.stmGonder("SWEEP:ON\n")
        else:
            self.stmGonder("SWEEP:OFF\n")

    def nvsKaydet(self):
        for i in range(1, 4):
            self.nvs_ultra[f"pS{i}"] = self.p_sure[i]
            self.nvs_ultra[f"pT{i}"] = self.p_sicaklik[i]
            self.nvs_ultra[f"pSw{i}"] = self.p_sweep[i]
        self.nvs_ultra["maxgoz"] = self.max_goz_sayisi
        for g in range(1, self.MAX_GOZ):
            self.nvs_ultra[f"guc_{g}"] = self.guc_seviyesi[g]
            self.nvs_ultra[f"kid_{g}"] = self.kart_id[g]
            self.nvs_ultra[f"sw_en_{g}"] = self.service_sweep[g]["enabled"]
            self.nvs_ultra[f"sw_sp_{g}"] = self.service_sweep[g]["span_khz"]
            self.nvs_ultra[f"sw_pr_{g}"] = self.service_sweep[g]["period_ms"]
            self.nvs_ultra[f"sw_st_{g}"] = self.service_sweep[g]["step_increment"]

    def nvsKaydetGoz(self, g: int):
        if 1 <= g < self.MAX_GOZ:
            self.nvs_ultra[f"guc_{g}"] = self.guc_seviyesi[g]
            self.nvs_ultra[f"kid_{g}"] = self.kart_id[g]
            self.nvs_ultra["maxgoz"] = self.max_goz_sayisi

    def sweepNvsKaydet(self, g: int):
        if 1 <= g < self.MAX_GOZ:
            self.nvs_ultra[f"sw_en_{g}"] = self.service_sweep[g]["enabled"]
            self.nvs_ultra[f"sw_sp_{g}"] = self.service_sweep[g]["span_khz"]
            self.nvs_ultra[f"sw_pr_{g}"] = self.service_sweep[g]["period_ms"]
            self.nvs_ultra[f"sw_st_{g}"] = self.service_sweep[g]["step_increment"]

    def degasNvsKaydet(self, g: int):
        if 1 <= g < self.MAX_GOZ:
            cfg = self.service_degas[g]
            self.nvs_ultra[f"dg_dur_{g}"] = cfg["duration_minutes"]
            self.nvs_ultra[f"dg_pwr_{g}"] = cfg["power_pct"]
            self.nvs_ultra[f"dg_frq_{g}"] = cfg["frequency_khz"]
            self.nvs_ultra[f"dg_pon_{g}"] = cfg["pulse_on_ms"]
            self.nvs_ultra[f"dg_poff_{g}"] = cfg["pulse_off_ms"]
            self.nvs_ultra[f"dg_tc_{g}"] = cfg["temp_ctrl"]
            self.nvs_ultra[f"dg_tgt_{g}"] = cfg["target_temp_c"]

    def degasNvsYukle(self):
        for g in range(1, self.MAX_GOZ):
            self.service_degas[g]["duration_minutes"] = self.nvs_ultra.get(f"dg_dur_{g}", self.service_degas[g]["duration_minutes"])
            self.service_degas[g]["power_pct"] = self.nvs_ultra.get(f"dg_pwr_{g}", self.service_degas[g]["power_pct"])
            self.service_degas[g]["frequency_khz"] = self.nvs_ultra.get(f"dg_frq_{g}", self.service_degas[g]["frequency_khz"])
            self.service_degas[g]["pulse_on_ms"] = self.nvs_ultra.get(f"dg_pon_{g}", self.service_degas[g]["pulse_on_ms"])
            self.service_degas[g]["pulse_off_ms"] = self.nvs_ultra.get(f"dg_poff_{g}", self.service_degas[g]["pulse_off_ms"])
            self.service_degas[g]["temp_ctrl"] = self.nvs_ultra.get(f"dg_tc_{g}", self.service_degas[g]["temp_ctrl"])
            self.service_degas[g]["target_temp_c"] = self.nvs_ultra.get(f"dg_tgt_{g}", self.service_degas[g]["target_temp_c"])

    def nvsYukle(self):
        for i in range(1, 4):
            self.p_sure[i] = self.nvs_ultra.get(f"pS{i}", self.p_sure[i])
            self.p_sicaklik[i] = self.nvs_ultra.get(f"pT{i}", self.p_sicaklik[i])
            self.p_sweep[i] = self.nvs_ultra.get(f"pSw{i}", self.p_sweep[i])
        self.max_goz_sayisi = self.nvs_ultra.get("maxgoz", self.max_goz_sayisi)
        for g in range(1, self.MAX_GOZ):
            self.guc_seviyesi[g] = self.nvs_ultra.get(f"guc_{g}", self.guc_seviyesi[g])
            self.kart_id[g] = self.nvs_ultra.get(f"kid_{g}", self.kart_id[g])
            self.service_sweep[g]["enabled"] = self.nvs_ultra.get(f"sw_en_{g}", self.service_sweep[g]["enabled"])
            self.service_sweep[g]["span_khz"] = self.nvs_ultra.get(f"sw_sp_{g}", self.service_sweep[g]["span_khz"])
            self.service_sweep[g]["period_ms"] = self.nvs_ultra.get(f"sw_pr_{g}", self.service_sweep[g]["period_ms"])
            self.service_sweep[g]["step_increment"] = self.nvs_ultra.get(f"sw_st_{g}", self.service_sweep[g]["step_increment"])
        self.degasNvsYukle()

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

        # =========================================================================
        # PAGE 0 & GENERAL OPERATION COMMANDS
        # =========================================================================

        if komut in ["PAGE0_OPEN", "PAGE0"]:
            self.aktif_sayfa = 0
            self.nextionGonder("page page0")
            self.updatePage0UI()
            return

        # --- DEGAS TOGGLE (Page 0 b_deg / b_degas) ---
        elif komut in ["CMD_DEGAS_SEL", "CMD_DEGAS_SELECT", "DEGAS_SEL", "b_deg", "b_degas", "CMD_DEGAS_TOGGLE"]:
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            self.degas_armed[self.secili_goz] = not self.degas_armed[self.secili_goz]
            if self.degas_armed[self.secili_goz]:
                self.aktif_program = 0
                self.runtime_sweep[self.secili_goz] = False
                self.stmGonder("SWEEP:OFF\n")
                self.hedef_sure[self.secili_goz] = self.service_degas[self.secili_goz]["duration_minutes"]
                self.hedef_sicaklik[self.secili_goz] = int(self.service_degas[self.secili_goz]["target_temp_c"])
                self.durum_metni[self.secili_goz] = "DEGAS SECILDI. START BEKLENIYOR"
                self.updatePage0UI()
            else:
                self.durum_metni[self.secili_goz] = "SISTEM BEKLEMEDE"
                self.updatePage0UI()

        elif komut in ["CMD_DEGAS_DESELECT", "DEGAS_DESEL"]:
            self.degas_armed[self.secili_goz] = False
            self.durum_metni[self.secili_goz] = "SISTEM BEKLEMEDE"
            self.updatePage0UI()

        # --- SWEEP TOGGLE (Page 0 b_sweep / b_swe) ---
        elif komut in ["CMD_SWEEP_TOGGLE", "SWP_TOGGLE", "PAGE0_SWP_TOGGLE"] or (self.aktif_sayfa == 0 and komut in ["b_sweep", "b_swe"]):
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            if self.degas_armed[self.secili_goz]:
                self.degas_armed[self.secili_goz] = False
            self.runtime_sweep[self.secili_goz] = not self.runtime_sweep[self.secili_goz]
            if self.runtime_sweep[self.secili_goz]:
                self.stmGonder("SWEEP:ON\n")
            else:
                self.stmGonder("SWEEP:OFF\n")
            self.updatePage0ButtonColors()

        elif komut in ["CMD_SWEEP_ON", "CMD_SWEEP|ON"]:
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            if self.degas_armed[self.secili_goz]:
                self.degas_armed[self.secili_goz] = False
            self.runtime_sweep[self.secili_goz] = True
            self.stmGonder("SWEEP:ON\n")
            self.updatePage0ButtonColors()

        elif komut in ["CMD_SWEEP_OFF", "CMD_SWEEP|OFF"]:
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            self.runtime_sweep[self.secili_goz] = False
            self.stmGonder("SWEEP:OFF\n")
            self.updatePage0ButtonColors()

        # --- HIZLI PROGRAM (FP) (Page 0) ---
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
            self.stmGonder(f"SET_POWER:{self.guc_seviyesi[self.secili_goz]}\n")
            self.stmGonder("START\n")

        # --- CMD_START | CMD_START|<sure>|<sicaklik> ---
        elif komut.startswith("CMD_START"):
            if self.degas_active[self.secili_goz] or "HATA!" in self.durum_metni[self.secili_goz]:
                return
            if self.baslatmaEngelliMi():
                return
            if self.degas_armed[self.secili_goz]:
                g = self.secili_goz
                self.degas_active[g] = True
                self.makine_calisiyor[g] = False
                cfg = self.service_degas[g]
                self.kalan_saniye[g] = cfg['duration_minutes'] * 60
                self.durum_metni[g] = "DEGAS DEVAM EDIYOR..."
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
                        self.stmGonder(f"SET_POWER:{self.guc_seviyesi[self.secili_goz]}\n")
                        self.stmGonder(f"SET_STEP_INC:{self.service_sweep[self.secili_goz]['step_increment']}\n")
                        self.stmGonder(f"SET_SWP_SPAN:{self.service_sweep[self.secili_goz]['span_khz']}\n")
                        self.stmGonder(f"SET_SWP_PER:{self.service_sweep[self.secili_goz]['period_ms']}\n")
                        self.stmGonder(f"SET_FREQ:{self.stm_freq[self.secili_goz]}\n")
                        if self.runtime_sweep[self.secili_goz]:
                            self.stmGonder("SWEEP:ON\n")
                        else:
                            self.stmGonder("SWEEP:OFF\n")
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
            if self.degas_armed[self.secili_goz]:
                self.durum_metni[self.secili_goz] = "DEGAS DURDURULDU. START BEKLENIYOR"
            else:
                self.durum_metni[self.secili_goz] = "SISTEM DURDURULDU"
            self.nextionGonder(f't_durum.txt="{self.durum_metni[self.secili_goz]}"')
            self.stmGonder("STOP\n")
            if self.aktif_sayfa == 0:
                self.updatePage0UI()

        # --- FREKANS TOGGLE (Page 0 b_freq / CMD_FREQ_TOGGLE) ---
        elif komut in ["CMD_FREQ_TOGGLE", "FREQ_TOGGLE", "b_freq", "b_frq"]:
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            if self.degas_armed[self.secili_goz]:
                self.degas_armed[self.secili_goz] = False
                self.nextionGonder(f"b_deg.bco={self.NEXTION_COLOR_DEFAULT}")
                self.nextionGonder(f"b_degas.bco={self.NEXTION_COLOR_DEFAULT}")
            yeni_freq = 28 if self.stm_freq[self.secili_goz] == 40 else 40
            self.stm_freq[self.secili_goz] = yeni_freq
            self.stmGonder(f"SET_FREQ:{yeni_freq}\n")
            freq_metni = "40k" if yeni_freq == 40 else "28k"
            self.nextionGonder(f'b_freq.txt="{freq_metni}"')
            self.nextionGonder(f'b_frq.txt="{freq_metni}"')
            self.updatePage0ButtonColors()

        # --- CMD_FREQ|<freq> ---
        elif komut.startswith("CMD_FREQ|") or komut.startswith("SET_FREQ|"):
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            if self.degas_armed[self.secili_goz]:
                self.degas_armed[self.secili_goz] = False
                self.nextionGonder(f"b_deg.bco={self.NEXTION_COLOR_DEFAULT}")
                self.nextionGonder(f"b_degas.bco={self.NEXTION_COLOR_DEFAULT}")
            parts = komut.split('|')
            if len(parts) >= 2:
                try:
                    freq = int(parts[1])
                    if freq in [28, 40]:
                        self.stm_freq[self.secili_goz] = freq
                        self.stmGonder(f"SET_FREQ:{freq}\n")
                        freq_metni = "40k" if freq == 40 else "28k"
                        self.nextionGonder(f'b_freq.txt="{freq_metni}"')
                        self.nextionGonder(f'b_frq.txt="{freq_metni}"')
                        self.updatePage0ButtonColors()
                except ValueError:
                    pass

        # --- P1_SEL / P2_SEL / P3_SEL (Page 0) ---
        elif komut in ["P1_SEL", "P2_SEL", "P3_SEL"]:
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            if self.baslatmaEngelliMi():
                return
            if self.degas_armed[self.secili_goz]:
                self.degas_armed[self.secili_goz] = False
                self.nextionGonder(f"b_deg.bco={self.NEXTION_COLOR_DEFAULT}")
                self.nextionGonder(f"b_degas.bco={self.NEXTION_COLOR_DEFAULT}")

            self.aktif_program = 1 if komut == "P1_SEL" else (2 if komut == "P2_SEL" else 3)
            self.hedef_sure[self.secili_goz] = self.p_sure[self.aktif_program]
            self.hedef_sicaklik[self.secili_goz] = self.p_sicaklik[self.aktif_program]
            self.runtime_sweep[self.secili_goz] = (self.p_sweep[self.aktif_program] != 0)
            self.kalan_saniye[self.secili_goz] = 0
            self.makine_calisiyor[self.secili_goz] = False
            self.durum_metni[self.secili_goz] = f"P{self.aktif_program} SECILDI. START BEKLENIYOR"

            self.updatePage0UI()
            self.nextionGonder(f't_durum.txt="{self.durum_metni[self.secili_goz]}"')

            self.stmGonder(f"SET_TIME:{self.hedef_sure[self.secili_goz]}\n")
            self.stmGonder(f"SET_TEMP:{self.hedef_sicaklik[self.secili_goz]}\n")
            self.stmGonder(f"SET_FREQ:{self.stm_freq[self.secili_goz]}\n")
            if self.runtime_sweep[self.secili_goz]:
                self.stmGonder("SWEEP:ON\n")
            else:
                self.stmGonder("SWEEP:OFF\n")

        # --- MANUAL_MODE (Page 0) ---
        elif komut in ["MANUAL_MODE", "MAN_SEL", "P0_SEL"]:
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            self.aktif_program = 0
            freq_metni = "40k" if self.stm_freq[self.secili_goz] == 40 else "28k"
            self.nextionGonder(f'b_freq.txt="{freq_metni}"')
            self.nextionGonder(f'b_frq.txt="{freq_metni}"')
            self.updatePage0ButtonColors()
            if self.degas_armed[self.secili_goz]:
                self.durum_metni[self.secili_goz] = "DEGAS SECILDI. START BEKLENIYOR"
            else:
                self.durum_metni[self.secili_goz] = "MANUEL MOD SECILDI"
            self.nextionGonder(f't_durum.txt="{self.durum_metni[self.secili_goz]}"')

        # --- TIME / TEMP EDIT COMMANDS (Page 0) ---
        elif komut in ["TIME_UP", "TIME_DOWN"] or komut.startswith("SET_TIME:") or komut.startswith("CMD_SET_TIME:"):
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
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

        elif (self.aktif_sayfa == 0 or self.aktif_sayfa == 1) and (komut in ["TEMP_UP", "TEMP_DOWN"] or komut.startswith("SET_TEMP:") or komut.startswith("CMD_SET_TEMP:")):
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
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

        # =========================================================================
        # PAGE 2: PROGRAM / RECIPE STORAGE (DRAFT BUFFER ARCHITECTURE)
        # =========================================================================
        elif komut in ["PAGE2_OPEN", "PAGE2", "b_programlar"]:
            self.aktif_sayfa = 2
            self.initRecipeEditBuffers()
            self.nextionGonder("page page2")
            self.updatePage2UI(self.duzenlenen_program)

        elif komut in ["EDIT_P1", "EDIT_P2", "EDIT_P3"] or (self.aktif_sayfa == 2 and komut in ["b_edit_p1", "b_edit_p2", "b_edit_p3", "b_p1", "b_p2", "b_p3"]):
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            if komut in ["EDIT_P1", "b_edit_p1", "b_p1"]:
                self.duzenlenen_program = 1
            elif komut in ["EDIT_P2", "b_edit_p2", "b_p2"]:
                self.duzenlenen_program = 2
            elif komut in ["EDIT_P3", "b_edit_p3", "b_p3"]:
                self.duzenlenen_program = 3

            # Freshly copy real/master values to draft for this program
            p = self.duzenlenen_program
            self.edit_p_sure[p] = self.p_sure[p]
            self.edit_p_sicaklik[p] = self.p_sicaklik[p]
            self.edit_p_sweep[p] = self.p_sweep[p]

            self.updatePage2UI(self.duzenlenen_program)

        elif komut in ["P_TIME_UP", "PAGE2_TIME_UP", "EDIT_TIME_UP", "b_edit_sure_up"] or (self.aktif_sayfa == 2 and komut in ["TIME_UP", "b_sure_up", "b_time_up", "b_up"]):
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            p = self.duzenlenen_program
            if self.edit_p_sure[p] < 120:
                self.edit_p_sure[p] += 1
            self.updatePage2UI(self.duzenlenen_program)

        elif komut in ["P_TIME_DOWN", "PAGE2_TIME_DOWN", "EDIT_TIME_DOWN", "b_edit_sure_down"] or (self.aktif_sayfa == 2 and komut in ["TIME_DOWN", "b_sure_down", "b_time_down", "b_down"]):
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            p = self.duzenlenen_program
            if self.edit_p_sure[p] > 1:
                self.edit_p_sure[p] -= 1
            self.updatePage2UI(self.duzenlenen_program)

        elif komut in ["P_TEMP_UP", "PAGE2_TEMP_UP", "EDIT_TEMP_UP", "b_edit_sic_up"] or (self.aktif_sayfa == 2 and komut in ["TEMP_UP", "b_sic_up", "b_temp_up"]):
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            p = self.duzenlenen_program
            if self.edit_p_sicaklik[p] < 90:
                self.edit_p_sicaklik[p] += 1
            self.updatePage2UI(self.duzenlenen_program)

        elif komut in ["P_TEMP_DOWN", "PAGE2_TEMP_DOWN", "EDIT_TEMP_DOWN", "b_edit_sic_down"] or (self.aktif_sayfa == 2 and komut in ["TEMP_DOWN", "b_sic_down", "b_temp_down"]):
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            p = self.duzenlenen_program
            if self.edit_p_sicaklik[p] > 20:
                self.edit_p_sicaklik[p] -= 1
            self.updatePage2UI(self.duzenlenen_program)

        elif komut in ["EDIT_SWEEP_TOG", "P_SWEEP_TOGGLE", "EDIT_SWEEP_TOGGLE", "PAGE2_SWEEP_TOGGLE", "b_edit_sweep"] or (self.aktif_sayfa == 2 and komut in ["b_swe", "b_sweep"]):
            if self.makine_calisiyor[self.secili_goz] or self.degas_active[self.secili_goz]:
                return
            p = self.duzenlenen_program
            self.edit_p_sweep[p] = 0 if self.edit_p_sweep[p] != 0 else 1
            self.updatePage2UI(self.duzenlenen_program)

        elif komut.startswith("P_SAVE|") or komut == "P_SAVE" or komut == "P_KAYDET" or (self.aktif_sayfa == 2 and komut in ["b_save", "b_kaydet"]):
            if self.degas_active[self.secili_goz] or self.makine_calisiyor[self.secili_goz]:
                return
            p = self.duzenlenen_program
            parts = komut.split('|')
            if len(parts) >= 3:
                try:
                    s_sure = int(parts[1])
                    s_sic = int(parts[2])
                    if 1 <= s_sure <= 120:
                        self.edit_p_sure[p] = s_sure
                    if 20 <= s_sic <= 90:
                        self.edit_p_sicaklik[p] = s_sic
                    if len(parts) >= 4:
                        self.edit_p_sweep[p] = 1 if int(parts[3]) != 0 else 0
                except ValueError:
                    pass
            # Commit draft to master
            self.p_sure[p] = self.edit_p_sure[p]
            self.p_sicaklik[p] = self.edit_p_sicaklik[p]
            self.p_sweep[p] = self.edit_p_sweep[p]
            self.nvsKaydet()
            self.nextionGonder(f"b_save.bco={self.NEXTION_COLOR_GREEN}")
            self.nextionGonder(f"b_save.bco={self.NEXTION_COLOR_DEFAULT}")
            self.updatePage2UI(self.duzenlenen_program)

        elif self.aktif_sayfa == 2 and komut in ["BACK", "b_back", "PAGE2_BACK"]:
            self.discardRecipeEditBuffers()
            self.aktif_sayfa = 3
            self.nextionGonder("page page3")
            self.nextionGonder('t0.txt="AYARLAR"')

        # =========================================================================
        # PAGE 1: KART/GÖZ SEÇİMİ
        # =========================================================================
        elif komut in ["PAGE1_OPEN", "PAGE1"]:
            self.aktif_sayfa = 1
            self.temp_goz = self.secili_goz
            self.nextionGonder("page page1")
            self.nextionGonder(f't_secili_goz.txt="{self.temp_goz}"')
            self.nextionGonder(f't0.txt="{self.temp_goz}"')
            self.nextionGonder(f't_goz.txt="{self.temp_goz}"')

        elif komut == "TANK_UP" or (self.aktif_sayfa == 1 and komut in ["UP", "b_up"]) or komut == "PAGE1_UP":
            if self.temp_goz < self.max_goz_sayisi:
                self.temp_goz += 1
            self.nextionGonder(f't_secili_goz.txt="{self.temp_goz}"')
            self.nextionGonder(f't0.txt="{self.temp_goz}"')
            self.nextionGonder(f't_goz.txt="{self.temp_goz}"')

        elif komut == "TANK_DOWN" or (self.aktif_sayfa == 1 and komut in ["DOWN", "b_down"]) or komut == "PAGE1_DOWN":
            if self.temp_goz > 1:
                self.temp_goz -= 1
            self.nextionGonder(f't_secili_goz.txt="{self.temp_goz}"')
            self.nextionGonder(f't0.txt="{self.temp_goz}"')
            self.nextionGonder(f't_goz.txt="{self.temp_goz}"')

        elif komut in ["TANK_SEL_OK", "SEL", "PAGE1_OK", "PAGE1_SEL"] or (self.aktif_sayfa == 1 and komut == "b_ok"):
            self.secili_goz = self.temp_goz
            self.aktif_sayfa = 0
            self.nextionGonder("page page0")
            self.updatePage0UI()
            self.stmSetpointleriGonder()

        elif komut in ["BACK", "PAGE1_BACK"] or (self.aktif_sayfa == 1 and komut == "b_back"):
            self.temp_goz = self.secili_goz
            self.aktif_sayfa = 0
            self.nextionGonder("page page0")
            self.updatePage0UI()

        # =========================================================================
        # PAGE 3: MAIN SETTINGS MENU
        # =========================================================================
        elif komut in ["PAGE3_OPEN", "PAGE3", "b_set", "b_ayarlar"]:
            self.aktif_sayfa = 3
            self.nextionGonder("page page3")
            self.nextionGonder('t0.txt="AYARLAR"')

        # =========================================================================
        # PAGE 4: ŞİFRE VE KLAVYE
        # =========================================================================
        elif komut in ["PAGE4_OPEN", "PAGE4", "b_servis"]:
            self.aktif_sayfa = 4
            self.girilen_sifre = ""
            self.nextionGonder("page page4")
            self.nextionGonder('t_sifre.txt=""')
            self.nextionGonder('t_pass.txt=""')
            self.nextionGonder('t0.txt="SIFRE GIRIN"')

        elif komut == "KEY_BACK" or (self.aktif_sayfa == 4 and komut in ["b_back", "BACK"]):
            self.aktif_sayfa = 3
            self.nextionGonder("page page3")
            self.nextionGonder('t0.txt="AYARLAR"')
            self.nextionGonder("page page3")

        elif komut in [f"KEY{i}" for i in range(10)]:
            digit = komut[3:]
            if len(self.girilen_sifre) < 6:
                self.girilen_sifre += digit
                self.nextionGonder(f't_pass.txt="{self.girilen_sifre}"')

        elif komut in ["KEY_DEL", "b_del"]:
            if len(self.girilen_sifre) > 0:
                self.girilen_sifre = self.girilen_sifre[:-1]
                self.nextionGonder(f't_pass.txt="{self.girilen_sifre}"')

        elif komut in ["KEY_SPC", "b_space"]:
            pass

        elif komut in ["KEY_OK", "PAGE4_OK"] or (self.aktif_sayfa == 4 and komut == "b_ok"):
            if self.girilen_sifre == self.dogru_sifre:
                self.girilen_sifre = ""
                self.g_service_authenticated = True
                self.service_auth_time = self.millis_ms
                self.current_service_page = 5
                self.aktif_sayfa = 5
                self.nextionGonder("page page5")
                self.updatePage5UI(self.secili_goz)
            else:
                self.girilen_sifre = ""
                self.g_service_authenticated = False
                self.nextionGonder('t_pass.txt="HATALI!"')

        # =========================================================================
        # SERVICE MENU NAVIGATION (PAGES 5, 6, 7, 8 CYCLIC GROUP)
        # =========================================================================
        elif komut in ["NAV_FORWARD", "b_forwoard", "PAGE_FORWARD"]:
            if not self.isProvisioningAllowed():
                self.nextionGonder(f"b_save.bco={self.NEXTION_COLOR_RED}")
                return
            if self.current_service_page == 5:
                self.current_service_page = 6
                self.aktif_sayfa = 6
                self.nextionGonder("page page6")
                self.updatePage6UI(self.secili_goz)
            elif self.current_service_page == 6:
                self.current_service_page = 7
                self.aktif_sayfa = 7
                self.nextionGonder("page page7")
                self.updatePage7UI(self.secili_goz)
            elif self.current_service_page == 7:
                self.current_service_page = 8
                self.aktif_sayfa = 8
                self.nextionGonder("page page8")
                self.updatePage8UI(self.secili_goz)
            elif self.current_service_page == 8:
                self.current_service_page = 5
                self.aktif_sayfa = 5
                self.nextionGonder("page page5")
                self.updatePage5UI(self.secili_goz)

        elif komut in ["NAV_BACK", "PAGE_BACK"] or (self.aktif_sayfa >= 5 and komut == "b_back"):
            if not self.isProvisioningAllowed():
                self.nextionGonder(f"b_save.bco={self.NEXTION_COLOR_RED}")
                return
            if self.current_service_page == 5:
                self.current_service_page = 8
                self.aktif_sayfa = 8
                self.nextionGonder("page page8")
                self.updatePage8UI(self.secili_goz)
            elif self.current_service_page == 8:
                self.current_service_page = 7
                self.aktif_sayfa = 7
                self.nextionGonder("page page7")
                self.updatePage7UI(self.secili_goz)
            elif self.current_service_page == 7:
                self.current_service_page = 6
                self.aktif_sayfa = 6
                self.nextionGonder("page page6")
                self.updatePage6UI(self.secili_goz)
            elif self.current_service_page == 6:
                self.current_service_page = 5
                self.aktif_sayfa = 5
                self.nextionGonder("page page5")
                self.updatePage5UI(self.secili_goz)

        elif komut in ["SRV_DISCARD", "SERVICE_EXIT", "b_exit", "SRV_EXIT", "EXIT_SERVICE", "SRV_BACK"]:
            self.discardServiceEditBuffers()
            self.g_service_authenticated = False
            self.aktif_sayfa = 0
            self.nextionGonder("page page0")
            self.updatePage0UI()

        # =========================================================================
        # PAGE 5: SERVICE SETTINGS / SELECTED TANK (TANK-SCOPED)
        # =========================================================================
        elif komut in ["PAGE5_OPEN", "PAGE5"]:
            self.current_service_page = 5
            self.aktif_sayfa = 5
            self.updatePage5UI(self.secili_goz)

        elif komut in ["PAGE8_OPEN", "PAGE8"]:
            self.current_service_page = 8
            self.aktif_sayfa = 8
            self.updatePage8UI(self.secili_goz)

        elif komut in ["SRV_TANK_UP", "b_srv_goz_up", "b_swp_goz_up", "b_deg_goz_up",
                       "PAGE5_GOZ_UP", "PAGE6_GOZ_UP", "PAGE7_GOZ_UP", "PAGE8_GOZ_UP",
                       "SWP_GOZ_UP", "DEG_GOZ_UP", "SRV_GOZ_INC",
                       "GOZ_UP", "b_up", "UP"]:
            if not self.isProvisioningAllowed():
                return
            if komut in ["b_swp_goz_up", "PAGE6_GOZ_UP", "SWP_GOZ_UP"]:
                self.current_service_page = 6
                self.aktif_sayfa = 6
            elif komut in ["b_deg_goz_up", "PAGE7_GOZ_UP", "DEG_GOZ_UP"]:
                self.current_service_page = 7
                self.aktif_sayfa = 7
            elif komut in ["b_srv_goz_up", "PAGE5_GOZ_UP"]:
                self.current_service_page = 5
                self.aktif_sayfa = 5

            if self.secili_goz < self.edit_max_goz_sayisi:
                self.secili_goz += 1
            else:
                self.secili_goz = 1
            if self.current_service_page == 5:
                self.updatePage5UI(self.secili_goz)
            elif self.current_service_page == 6:
                self.updatePage6UI(self.secili_goz)
            elif self.current_service_page == 7:
                self.updatePage7UI(self.secili_goz)
            elif self.current_service_page == 8:
                self.updatePage8UI(self.secili_goz)
            else:
                if self.aktif_sayfa == 6:
                    self.updatePage6UI(self.secili_goz)
                elif self.aktif_sayfa == 7:
                    self.updatePage7UI(self.secili_goz)
                elif self.aktif_sayfa == 8:
                    self.updatePage8UI(self.secili_goz)
                else:
                    self.updatePage5UI(self.secili_goz)

        elif komut in ["SRV_TANK_DOWN", "b_srv_goz_down", "b_swp_goz_down", "b_deg_goz_down",
                       "PAGE5_GOZ_DOWN", "PAGE6_GOZ_DOWN", "PAGE7_GOZ_DOWN", "PAGE8_GOZ_DOWN",
                       "SWP_GOZ_DOWN", "DEG_GOZ_DOWN", "SRV_GOZ_DEC",
                       "GOZ_DOWN", "b_down", "DOWN"]:
            if not self.isProvisioningAllowed():
                return
            if komut in ["b_swp_goz_down", "PAGE6_GOZ_DOWN", "SWP_GOZ_DOWN"]:
                self.current_service_page = 6
                self.aktif_sayfa = 6
            elif komut in ["b_deg_goz_down", "PAGE7_GOZ_DOWN", "DEG_GOZ_DOWN"]:
                self.current_service_page = 7
                self.aktif_sayfa = 7
            elif komut in ["b_srv_goz_down", "PAGE5_GOZ_DOWN"]:
                self.current_service_page = 5
                self.aktif_sayfa = 5

            if self.secili_goz > 1:
                self.secili_goz -= 1
            else:
                self.secili_goz = self.edit_max_goz_sayisi
            if self.current_service_page == 5:
                self.updatePage5UI(self.secili_goz)
            elif self.current_service_page == 6:
                self.updatePage6UI(self.secili_goz)
            elif self.current_service_page == 7:
                self.updatePage7UI(self.secili_goz)
            elif self.current_service_page == 8:
                self.updatePage8UI(self.secili_goz)
            else:
                if self.aktif_sayfa == 6:
                    self.updatePage6UI(self.secili_goz)
                elif self.aktif_sayfa == 7:
                    self.updatePage7UI(self.secili_goz)
                elif self.aktif_sayfa == 8:
                    self.updatePage8UI(self.secili_goz)
                else:
                    self.updatePage5UI(self.secili_goz)

        elif komut in ["SRV_GUC_UP", "GUC_UP", "b_guc_up"]:
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 5
            self.aktif_sayfa = 5
            if self.edit_guc_seviyesi[self.secili_goz] < 100:
                self.edit_guc_seviyesi[self.secili_goz] += 10
            self.updatePage5UI(self.secili_goz)

        elif komut in ["SRV_GUC_DOWN", "GUC_DOWN", "b_guc_down"]:
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 5
            self.aktif_sayfa = 5
            if self.edit_guc_seviyesi[self.secili_goz] > 10:
                self.edit_guc_seviyesi[self.secili_goz] -= 10
            self.updatePage5UI(self.secili_goz)

        elif komut in ["SRV_ID_UP", "b_srv_id_up", "ID_UP", "b_id_up"]:
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 5
            self.aktif_sayfa = 5
            old_id = self.edit_kart_id[self.secili_goz]
            if old_id < self.edit_max_goz_sayisi:
                new_id = old_id + 1
                for g in range(1, self.MAX_GOZ):
                    if g != self.secili_goz and self.edit_kart_id[g] == new_id:
                        self.edit_kart_id[g] = old_id
                self.edit_kart_id[self.secili_goz] = new_id
            self.updatePage5UI(self.secili_goz)

        elif komut in ["SRV_ID_DOWN", "b_srv_id_down", "ID_DOWN", "b_id_down"]:
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 5
            self.aktif_sayfa = 5
            old_id = self.edit_kart_id[self.secili_goz]
            if old_id > 1:
                new_id = old_id - 1
                for g in range(1, self.MAX_GOZ):
                    if g != self.secili_goz and self.edit_kart_id[g] == new_id:
                        self.edit_kart_id[g] = old_id
                self.edit_kart_id[self.secili_goz] = new_id
            self.updatePage5UI(self.secili_goz)

        elif komut in ["SRV_MAX_UP", "MAX_UP", "b_max_up"]:
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 5
            self.aktif_sayfa = 5
            if self.edit_max_goz_sayisi < self.MAX_GOZ - 1:
                self.edit_max_goz_sayisi += 1
                duplicate = any(self.edit_kart_id[g] == self.edit_kart_id[self.edit_max_goz_sayisi] for g in range(1, self.edit_max_goz_sayisi))
                if duplicate or self.edit_kart_id[self.edit_max_goz_sayisi] < 1 or self.edit_kart_id[self.edit_max_goz_sayisi] > self.edit_max_goz_sayisi:
                    self.edit_kart_id[self.edit_max_goz_sayisi] = self.edit_max_goz_sayisi
            self.updatePage5UI(self.secili_goz)

        elif komut in ["SRV_MAX_DOWN", "MAX_DOWN", "b_max_down"]:
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 5
            self.aktif_sayfa = 5
            if self.edit_max_goz_sayisi > 1:
                self.edit_max_goz_sayisi -= 1
            self.updatePage5UI(self.secili_goz)

        # =========================================================================
        # PAGE 6: SWEEP SERVICE SETTINGS (TANK-SCOPED)
        # =========================================================================
        elif komut in ["PAGE6_OPEN", "PAGE6"]:
            self.current_service_page = 6
            self.aktif_sayfa = 6
            self.updatePage6UI(self.secili_goz)

        elif komut in ["PAGE6_SWP_TOGGLE", "SWP_CFG_TOGGLE", "SWEEP_CFG_TOGGLE"]:
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 6
            self.aktif_sayfa = 6
            g = self.secili_goz
            self.edit_service_sweep[g]["enabled"] = 0 if self.edit_service_sweep[g]["enabled"] else 1
            self.updatePage6UI(g)

        elif komut in ["SRV_SPAN_UP", "b_span_up", "SWP_SPAN_UP"] or komut.startswith("SET_SWP_SPAN:") or komut.startswith("CMD_SET_SWP_SPAN:") or komut.startswith("SET_SPAN:"):
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 6
            self.aktif_sayfa = 6
            g = self.secili_goz
            if ":" in komut:
                val = int(komut.split(":")[1])
                if 1 <= val <= 4:
                    self.edit_service_sweep[g]["span_khz"] = val
            else:
                if self.edit_service_sweep[g]["span_khz"] < 4:
                    self.edit_service_sweep[g]["span_khz"] += 1
            self.updatePage6UI(g)

        elif komut in ["SRV_SPAN_DOWN", "b_span_down", "SWP_SPAN_DOWN"]:
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 6
            self.aktif_sayfa = 6
            g = self.secili_goz
            if self.edit_service_sweep[g]["span_khz"] > 1:
                self.edit_service_sweep[g]["span_khz"] -= 1
            self.updatePage6UI(g)

        elif komut in ["SRV_PER_UP", "b_per_up", "SWP_PER_UP"] or komut.startswith("SET_SWP_PER:") or komut.startswith("CMD_SET_SWP_PER:") or komut.startswith("SET_PER:"):
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 6
            self.aktif_sayfa = 6
            g = self.secili_goz
            if ":" in komut:
                val = int(komut.split(":")[1])
                if 100 <= val <= 1000:
                    self.edit_service_sweep[g]["period_ms"] = val
            else:
                if self.edit_service_sweep[g]["period_ms"] <= 900:
                    self.edit_service_sweep[g]["period_ms"] += 100
            self.updatePage6UI(g)

        elif komut in ["SRV_PER_DOWN", "b_per_down", "SWP_PER_DOWN"]:
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 6
            self.aktif_sayfa = 6
            g = self.secili_goz
            if self.edit_service_sweep[g]["period_ms"] >= 200:
                self.edit_service_sweep[g]["period_ms"] -= 100
            self.updatePage6UI(g)

        elif komut in ["SRV_STEP_UP", "b_step_up", "SWP_STEP_UP"] or komut.startswith("SET_STEP_INC:") or komut.startswith("CMD_SET_STEP_INC:") or komut.startswith("SET_SWP_STEP:"):
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 6
            self.aktif_sayfa = 6
            g = self.secili_goz
            if ":" in komut:
                val = int(komut.split(":")[1])
                if 1 <= val <= 8:
                    self.edit_service_sweep[g]["step_increment"] = val
            else:
                if self.edit_service_sweep[g]["step_increment"] < 8:
                    self.edit_service_sweep[g]["step_increment"] += 1
            self.updatePage6UI(g)

        elif komut in ["SRV_STEP_DOWN", "b_step_down", "SWP_STEP_DOWN"]:
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 6
            self.aktif_sayfa = 6
            g = self.secili_goz
            if self.edit_service_sweep[g]["step_increment"] > 1:
                self.edit_service_sweep[g]["step_increment"] -= 1
            self.updatePage6UI(g)

        # =========================================================================
        # PAGE 7 & PAGE 8: DEGAS SERVICE SETTINGS (TANK-SCOPED)
        # =========================================================================
        elif komut in ["PAGE7_OPEN", "PAGE7", "PAGE_DEGAS_OPEN", "PAGE_DEGAS"]:
            self.current_service_page = 7
            self.aktif_sayfa = 7
            self.updatePage7UI(self.secili_goz)

        elif komut in ["SRV_DDUR_UP", "DEG_DUR_UP", "b_dur_up"] or komut.startswith("SET_DEG_DUR:"):
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 7
            self.aktif_sayfa = 7
            g = self.secili_goz
            if komut.startswith("SET_DEG_DUR:"):
                v = int(komut.split(":")[1])
                if 1 <= v <= 120:
                    self.edit_service_degas[g]["duration_minutes"] = v
            else:
                if self.edit_service_degas[g]["duration_minutes"] < 120:
                    self.edit_service_degas[g]["duration_minutes"] += 1
            self.updatePage7UI(g)

        elif komut in ["SRV_DDUR_DOWN", "DEG_DUR_DOWN", "b_dur_down"]:
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 7
            self.aktif_sayfa = 7
            g = self.secili_goz
            if self.edit_service_degas[g]["duration_minutes"] > 1:
                self.edit_service_degas[g]["duration_minutes"] -= 1
            self.updatePage7UI(g)

        elif komut in ["SRV_DPOW_UP", "DEG_PWR_UP", "b_pwr_up"] or komut.startswith("SET_DEG_PWR:"):
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 7
            self.aktif_sayfa = 7
            g = self.secili_goz
            if komut.startswith("SET_DEG_PWR:"):
                v = int(komut.split(":")[1])
                if 10 <= v <= 100:
                    self.edit_service_degas[g]["power_pct"] = v
            else:
                if self.edit_service_degas[g]["power_pct"] <= 90:
                    self.edit_service_degas[g]["power_pct"] += 10
            self.updatePage7UI(g)

        elif komut in ["SRV_DPOW_DOWN", "DEG_PWR_DOWN", "b_pwr_down"]:
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 7
            self.aktif_sayfa = 7
            g = self.secili_goz
            if self.edit_service_degas[g]["power_pct"] >= 20:
                self.edit_service_degas[g]["power_pct"] -= 10
            self.updatePage7UI(g)

        elif komut in ["SRV_DFREQ_UP", "DEG_FRQ_UP", "b_frq_up"] or komut.startswith("SET_DEG_FRQ:"):
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 7
            self.aktif_sayfa = 7
            g = self.secili_goz
            if komut.startswith("SET_DEG_FRQ:"):
                v = int(komut.split(":")[1])
                if 28 <= v <= 40:
                    self.edit_service_degas[g]["frequency_khz"] = v
            else:
                if self.edit_service_degas[g]["frequency_khz"] < 40:
                    self.edit_service_degas[g]["frequency_khz"] += 1
            self.updatePage7UI(g)

        elif komut in ["SRV_DFREQ_DOWN", "DEG_FRQ_DOWN", "b_frq_down"]:
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 7
            self.aktif_sayfa = 7
            g = self.secili_goz
            if self.edit_service_degas[g]["frequency_khz"] > 28:
                self.edit_service_degas[g]["frequency_khz"] -= 1
            self.updatePage7UI(g)

        elif komut in ["SRV_DPON_UP", "DEG_ON_UP", "b_on_up"] or komut.startswith("SET_DEG_ON:"):
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 8
            self.aktif_sayfa = 8
            g = self.secili_goz
            if komut.startswith("SET_DEG_ON:"):
                v = int(komut.split(":")[1])
                if 100 <= v <= 10000:
                    self.edit_service_degas[g]["pulse_on_ms"] = v
            else:
                if self.edit_service_degas[g]["pulse_on_ms"] <= 9900:
                    self.edit_service_degas[g]["pulse_on_ms"] += 100
            self.updatePage8UI(g)

        elif komut in ["SRV_DPON_DOWN", "DEG_ON_DOWN", "b_on_down"]:
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 8
            self.aktif_sayfa = 8
            g = self.secili_goz
            if self.edit_service_degas[g]["pulse_on_ms"] >= 200:
                self.edit_service_degas[g]["pulse_on_ms"] -= 100
            self.updatePage8UI(g)

        elif komut in ["SRV_DPOFF_UP", "DEG_OFF_UP", "b_off_up"] or komut.startswith("SET_DEG_OFF:"):
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 8
            self.aktif_sayfa = 8
            g = self.secili_goz
            if komut.startswith("SET_DEG_OFF:"):
                v = int(komut.split(":")[1])
                if v == 0 or (100 <= v <= 10000):
                    self.edit_service_degas[g]["pulse_off_ms"] = v
            else:
                if self.edit_service_degas[g]["pulse_off_ms"] <= 9900:
                    self.edit_service_degas[g]["pulse_off_ms"] += 100
            self.updatePage8UI(g)

        elif komut in ["SRV_DPOFF_DOWN", "DEG_OFF_DOWN", "b_off_down"]:
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 8
            self.aktif_sayfa = 8
            g = self.secili_goz
            if self.edit_service_degas[g]["pulse_off_ms"] >= 100:
                self.edit_service_degas[g]["pulse_off_ms"] -= 100
            self.updatePage8UI(g)

        elif komut in ["SRV_DTCTRL_TOG", "DEG_TC_TOGGLE", "b_tc_toggle"] or komut.startswith("SET_DEG_TC:"):
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 8
            self.aktif_sayfa = 8
            g = self.secili_goz
            if komut.startswith("SET_DEG_TC:"):
                v = int(komut.split(":")[1])
                self.edit_service_degas[g]["temp_ctrl"] = 1 if v != 0 else 0
            else:
                self.edit_service_degas[g]["temp_ctrl"] = 0 if self.edit_service_degas[g]["temp_ctrl"] != 0 else 1
            self.updatePage8UI(g)

        elif komut in ["SRV_DTEMP_UP", "DEG_TGT_UP", "b_tgt_up"] or komut.startswith("SET_DEG_TGT:"):
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 8
            self.aktif_sayfa = 8
            g = self.secili_goz
            if self.edit_service_degas[g]["temp_ctrl"] == 0:
                return
            if komut.startswith("SET_DEG_TGT:"):
                v = float(komut.split(":")[1])
                if 20.0 <= v <= 90.0:
                    self.edit_service_degas[g]["target_temp_c"] = v
            else:
                if self.edit_service_degas[g]["target_temp_c"] < 90.0:
                    self.edit_service_degas[g]["target_temp_c"] += 1.0
            self.updatePage8UI(g)

        elif komut in ["SRV_DTEMP_DOWN", "DEG_TGT_DOWN", "b_tgt_down"]:
            if not self.isProvisioningAllowed():
                return
            self.current_service_page = 8
            self.aktif_sayfa = 8
            g = self.secili_goz
            if self.edit_service_degas[g]["temp_ctrl"] == 0:
                return
            if komut.startswith("SET_DEG_TGT:"):
                v = float(komut.split(":")[1])
                if 20.0 <= v <= 90.0:
                    self.edit_service_degas[g]["target_temp_c"] = v
            else:
                if self.edit_service_degas[g]["target_temp_c"] > 20.0:
                    self.edit_service_degas[g]["target_temp_c"] -= 1.0
            self.updatePage8UI(g)

        # =========================================================================
        # ATOMIC SERVICE SAVE (SRV_SAVE & PAGE-SPECIFIC ALIASES)
        # =========================================================================
        elif komut in ["SRV_SAVE", "PAGE5_SAVE", "PAGE6_SAVE", "PAGE7_SAVE", "PAGE8_SAVE", "SRV_PAGE5_SAVE", "SWP_SAVE", "SRV_SWEEP_SAVE", "SRV_DEGAS_SAVE", "SRV_SAVE_DEGAS"]:
            if not self.isProvisioningAllowed():
                self.g_bus_diag["tx_nack_count"] += 1
                self.nextionGonder(f"b_save.bco={self.NEXTION_COLOR_RED}")
                return

            for g in range(1, self.MAX_GOZ):
                cfg = self.edit_service_degas[g]
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
                    self.nextionGonder(f"b_save.bco={self.NEXTION_COLOR_RED}")
                    return

            self.max_goz_sayisi = self.edit_max_goz_sayisi
            eski_goz = self.secili_goz
            hedef_yeni_goz = self.edit_kart_id[eski_goz]

            for g in range(1, self.MAX_GOZ):
                self.guc_seviyesi[g] = self.edit_guc_seviyesi[g]
                self.kart_id[g] = self.edit_kart_id[g]
                self.service_sweep[g] = dict(self.edit_service_sweep[g])
                self.service_degas[g] = dict(self.edit_service_degas[g])
                self.nvsKaydetGoz(g)
                self.sweepNvsKaydet(g)
                self.degasNvsKaydet(g)

            if self.secili_goz > self.max_goz_sayisi:
                self.secili_goz = self.max_goz_sayisi
            if self.secili_goz < 1:
                self.secili_goz = 1
            self.initServiceEditBuffers()
            self.stmSetpointleriGonder()

            self.g_bus_diag["tx_ack_count"] += 1
            self.nextionGonder(f"b_save.bco={self.NEXTION_COLOR_GREEN}")
            self.nextionGonder(f"b_save.bco={self.NEXTION_COLOR_DEFAULT}")
            if self.current_service_page == 5:
                self.updatePage5UI(self.secili_goz)
            elif self.current_service_page == 6:
                self.updatePage6UI(self.secili_goz)
            elif self.current_service_page == 7:
                self.updatePage7UI(self.secili_goz)
            elif self.current_service_page == 8:
                self.updatePage8UI(self.secili_goz)

        # =========================================================================
        # DIAGNOSTICS & HIL UTILITIES
        # =========================================================================
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

        yeniden_baglandi = not self.stm_bagli[g]
        self.stm_bagli[g] = True
        self.stm_son_veri_zamani[g] = self.millis_ms

        if fault > 0:
            self.durum_metni[g] = f"HATA! KOD:{fault}"
            self.degas_active[g] = False
            self.degas_armed[g] = False
            if g == self.secili_goz:
                self.nextionGonder(f"b_deg.bco={self.NEXTION_COLOR_DEFAULT}")
                self.nextionGonder(f"b_degas.bco={self.NEXTION_COLOR_DEFAULT}")
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
                    self.nextionGonder(f"b_deg.bco={self.NEXTION_COLOR_DEFAULT}")
                    self.nextionGonder(f"b_degas.bco={self.NEXTION_COLOR_DEFAULT}")
                    self.nextionGonder('t_durum.txt="Kart Yok!"')
                    self.nextionGonder('t_status.txt="Kart Yok!"')

        # Periodic 1000ms Nextion Display Refresh
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
# THE BASELINE OFFLINE HMI MOCK UNITTEST SUITE
# =============================================================================
class TestHMIMockSuite(unittest.TestCase):

    def setUp(self):
        self.hmi = MockESP32HMI()
        self.hmi.millis_ms = 1000.0
        self.hmi.sonGuncellemeZamani = 1000.0
        self.hmi.stmTelemetryIsle("STAT,1,IDLE,0,250,0,50,28,0")

    def test_01_p_hizli_command(self):
        self.hmi.komutIsle("P_HIZLI")
        self.assertTrue(self.hmi.makine_calisiyor[1])
        self.assertEqual(self.hmi.hedef_sure[1], 5)
        self.assertEqual(self.hmi.hedef_sicaklik[1], 30)
        self.assertIn("T1:START", self.hmi.stm32_tx_log)
        self.assertIn('t_set_sure.txt="05"', self.hmi.nextion_tx_log)

    def test_02_cmd_start_command(self):
        self.hmi.komutIsle("CMD_START|10|50")
        self.assertTrue(self.hmi.makine_calisiyor[1])
        self.assertEqual(self.hmi.hedef_sure[1], 10)
        self.assertEqual(self.hmi.hedef_sicaklik[1], 50)
        self.assertEqual(self.hmi.kalan_saniye[1], 600)
        self.assertIn("T1:START", self.hmi.stm32_tx_log)

    def test_03_cmd_stop_command(self):
        self.hmi.makine_calisiyor[1] = True
        self.hmi.komutIsle("CMD_STOP")
        self.assertFalse(self.hmi.makine_calisiyor[1])
        self.assertIn("T1:STOP", self.hmi.stm32_tx_log)

    def test_04_cmd_freq_command(self):
        self.hmi.komutIsle("CMD_FREQ|40")
        self.assertIn("T1:SET_FREQ:40", self.hmi.stm32_tx_log)

    def test_05_preset_recipe_selection(self):
        self.hmi.komutIsle("P2_SEL")
        self.assertEqual(self.hmi.hedef_sure[1], 20)
        self.assertEqual(self.hmi.hedef_sicaklik[1], 50)
        self.assertIn("T1:SET_TIME:20", self.hmi.stm32_tx_log)
        self.assertIn("T1:SET_TEMP:50", self.hmi.stm32_tx_log)

    def test_06_correct_password_login(self):
        for d in "123456":
            self.hmi.komutIsle(f"KEY{d}")
        self.hmi.komutIsle("KEY_OK")
        self.assertTrue(self.hmi.g_service_authenticated)
        self.assertIn("page page5", self.hmi.nextion_tx_log)

    def test_07_invalid_password_rejection(self):
        for d in "000000":
            self.hmi.komutIsle(f"KEY{d}")
        self.hmi.komutIsle("KEY_OK")
        self.assertFalse(self.hmi.g_service_authenticated)
        self.assertIn('t_pass.txt="HATALI!"', self.hmi.nextion_tx_log)

    def test_08_service_session_5min_timeout(self):
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = 1000.0
        self.hmi.loop_tick(241000.0)
        self.assertTrue(self.hmi.g_service_authenticated)
        self.hmi.loop_tick(302000.0)
        self.assertFalse(self.hmi.g_service_authenticated)

    def test_09_recipe_editing_and_save(self):
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
        self.hmi.nvs_ultra["pS3"] = 35
        self.hmi.nvs_ultra["pT3"] = 75
        self.hmi.nvsYukle()
        self.assertEqual(self.hmi.p_sure[3], 35)
        self.assertEqual(self.hmi.p_sicaklik[3], 75)

    def test_11_offline_card_start_rejection(self):
        self.hmi.loop_tick(5500.0)
        self.assertFalse(self.hmi.isKartBagli(1))
        self.hmi.komutIsle("CMD_START|10|50")
        self.assertFalse(self.hmi.makine_calisiyor[1])
        self.assertEqual(self.hmi.durum_metni[1], "Kart Yok!")
        self.assertIn('t_durum.txt="Kart Yok!"', self.hmi.nextion_tx_log)

    def test_12_reconnect_telemetry_recovery(self):
        self.hmi.loop_tick(5500.0)
        self.assertFalse(self.hmi.isKartBagli(1))
        self.hmi.stmTelemetryIsle("STAT,1,IDLE,0,250,0,50,28,0")
        self.assertTrue(self.hmi.isKartBagli(1))
        self.hmi.komutIsle("CMD_START|10|50")
        self.assertTrue(self.hmi.makine_calisiyor[1])

    def test_13_pt100_error_display_formatting(self):
        self.hmi.stmTelemetryIsle("STAT,1,FAULT,0,0,0,50,28,1")
        self.hmi.loop_tick(2500.0)
        self.assertIn('t_anlik_sic.txt="--.-"', self.hmi.nextion_tx_log)

    def test_14_unauthenticated_provisioning_rejection(self):
        self.hmi.g_service_authenticated = False
        self.hmi.komutIsle("SRV_SAVE")
        self.assertIn("b_save.bco=63488", self.hmi.nextion_tx_log)

    def test_15_running_tank_provisioning_rejection(self):
        self.hmi.g_service_authenticated = True
        self.hmi.makine_calisiyor[1] = True
        self.hmi.komutIsle("SRV_SAVE")
        self.assertIn("b_save.bco=63488", self.hmi.nextion_tx_log)

    def test_16_tank_selection_up_down_clamping(self):
        self.hmi.max_goz_sayisi = 3
        self.hmi.komutIsle("PAGE1_OPEN")
        self.hmi.komutIsle("UP")
        self.assertEqual(self.hmi.temp_goz, 2)
        self.hmi.komutIsle("UP")
        self.assertEqual(self.hmi.temp_goz, 3)
        self.hmi.komutIsle("UP")
        self.assertEqual(self.hmi.temp_goz, 3)
        self.hmi.komutIsle("DOWN")
        self.hmi.komutIsle("DOWN")
        self.hmi.komutIsle("DOWN")
        self.assertEqual(self.hmi.temp_goz, 1)

    def test_17_tank_selection_confirm_and_sync(self):
        self.hmi.max_goz_sayisi = 3
        self.hmi.komutIsle("PAGE1_OPEN")
        self.hmi.komutIsle("UP")
        self.hmi.komutIsle("SEL")
        self.assertEqual(self.hmi.secili_goz, 2)
        self.assertIn('b_goz.txt="Goz: 2"', self.hmi.nextion_tx_log)
        self.assertIn("T2:SET_TIME:0", self.hmi.stm32_tx_log)

    def test_18_multiline_concatenated_input_safety(self):
        self.hmi.stmTelemetryIsle("STAT,1,IDLE,0,250,0,50,28,0")
        self.hmi.komutIsle("P1_SEL\nCMD_START|15|45")
        self.assertTrue(self.hmi.makine_calisiyor[1])
        self.assertEqual(self.hmi.hedef_sure[1], 15)
        self.assertEqual(self.hmi.hedef_sicaklik[1], 45)


# =============================================================================
# NEW HMI ARCHITECTURE COMPREHENSIVE VERIFICATION SUITE (Section 16 Domains)
# =============================================================================
class TestHMINewArchitectureSuite(unittest.TestCase):
    """Verifies all 15 explicit test requirements in prompt section 16."""

    def setUp(self):
        self.hmi = MockESP32HMI()
        self.hmi.millis_ms = 1000.0
        # Connect both Tank 1 and Tank 2
        self.hmi.stmTelemetryIsle("STAT,1,IDLE,0,250,0,50,28,0")
        self.hmi.stmTelemetryIsle("STAT,2,IDLE,0,250,0,50,28,0")

    def test_arch_01_page0_degas_toggle(self):
        """1. Page 0 DEGAS toggle single button state & visual sync."""
        self.assertFalse(self.hmi.degas_armed[1])
        self.hmi.komutIsle("b_deg")
        self.assertTrue(self.hmi.degas_armed[1])
        self.assertIn("b_deg.bco=2016", self.hmi.nextion_tx_log)

        self.hmi.komutIsle("b_deg")
        self.assertFalse(self.hmi.degas_armed[1])
        self.assertIn("b_deg.bco=50712", self.hmi.nextion_tx_log)

    def test_arch_02_page0_sweep_toggle(self):
        """2. Page 0 Sweep toggle single button state & stmSweep."""
        self.assertFalse(self.hmi.runtime_sweep[1])
        self.hmi.komutIsle("b_swe")
        self.assertTrue(self.hmi.runtime_sweep[1])
        self.assertIn("b_swe.bco=2016", self.hmi.nextion_tx_log)
        self.assertIn("T1:SWEEP:ON", self.hmi.stm32_tx_log)

        self.hmi.komutIsle("b_swe")
        self.assertFalse(self.hmi.runtime_sweep[1])
        self.assertIn("b_swe.bco=50712", self.hmi.nextion_tx_log)
        self.assertIn("T1:SWEEP:OFF", self.hmi.stm32_tx_log)

    def test_arch_03_p1_p2_p3_stored_sweep_state(self):
        """3. Selecting P1/P2/P3 loads duration, temp, and stored sweep state into b_swe."""
        self.hmi.p_sweep[1] = 1  # P1 has Sweep ON
        self.hmi.p_sweep[2] = 0  # P2 has Sweep OFF

        # Select P1
        self.hmi.komutIsle("P1_SEL")
        self.assertTrue(self.hmi.runtime_sweep[1])
        self.assertIn("b_swe.bco=2016", self.hmi.nextion_tx_log)
        self.assertIn("T1:SWEEP:ON", self.hmi.stm32_tx_log)

        # Select P2
        self.hmi.komutIsle("P2_SEL")
        self.assertFalse(self.hmi.runtime_sweep[1])
        self.assertIn("b_swe.bco=50712", self.hmi.nextion_tx_log)
        self.assertIn("T1:SWEEP:OFF", self.hmi.stm32_tx_log)

    def test_arch_04_page2_sweep_save_load(self):
        """4. Page 2 Sweep toggle, recipe save to NVS, and reload persistence."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms

        self.hmi.komutIsle("EDIT_P3")
        self.assertEqual(self.hmi.duzenlenen_program, 3)
        self.assertEqual(self.hmi.p_sweep[3], 0)
        self.assertEqual(self.hmi.edit_p_sweep[3], 0)

        # Toggle sweep on Page 2 (mutates draft, master remains 0)
        self.hmi.komutIsle("PAGE2_SWEEP_TOGGLE")
        self.assertEqual(self.hmi.edit_p_sweep[3], 1)
        self.assertEqual(self.hmi.p_sweep[3], 0)
        self.assertIn("b_swe.bco=2016", self.hmi.nextion_tx_log)

        # Save recipe (commits draft to master and NVS)
        self.hmi.komutIsle("P_SAVE|30|65")
        self.assertEqual(self.hmi.p_sweep[3], 1)
        self.assertEqual(self.hmi.nvs_ultra["pSw3"], 1)

        # New instance reload from NVS
        fresh_hmi = MockESP32HMI()
        fresh_hmi.nvs_ultra = dict(self.hmi.nvs_ultra)
        fresh_hmi.nvsYukle()
        self.assertEqual(fresh_hmi.p_sweep[3], 1)

    def test_arch_05_tank_selection_changing_page5_values(self):
        """5. Tank selection changes Page 5 values to selected tank's values."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.guc_seviyesi[1] = 60
        self.hmi.guc_seviyesi[2] = 90
        self.hmi.kart_id[1] = 5
        self.hmi.kart_id[2] = 8
        self.hmi.initServiceEditBuffers()

        self.hmi.secili_goz = 1
        self.hmi.komutIsle("PAGE5_OPEN")
        self.assertIn('t_goz_num.txt="1"', self.hmi.nextion_tx_log)
        self.assertIn('t_guc.txt="60"', self.hmi.nextion_tx_log)
        self.assertIn('t_id.txt="5"', self.hmi.nextion_tx_log)

        # Navigate to Tank 2 using b_up / PAGE5_GOZ_UP
        self.hmi.komutIsle("PAGE5_GOZ_UP")
        self.assertEqual(self.hmi.secili_goz, 2)
        self.assertIn('t_goz_num.txt="2"', self.hmi.nextion_tx_log)
        self.assertIn('t_guc.txt="90"', self.hmi.nextion_tx_log)
        self.assertIn('t_id.txt="8"', self.hmi.nextion_tx_log)

    def test_arch_06_tank1_and_tank2_configuration_isolation(self):
        """6. Tank 1 and Tank 2 configurations are strictly isolated (no cross-contamination)."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms

        self.hmi.secili_goz = 1
        self.hmi.komutIsle("GUC_UP")  # Tank 1: 60% in draft
        self.hmi.komutIsle("PAGE5_SAVE")

        self.hmi.komutIsle("PAGE5_GOZ_UP")  # To Tank 2
        self.assertEqual(self.hmi.guc_seviyesi[2], 50, "Tank 2 power must remain untouched at default 50%")

    def test_arch_07_page_5_6_7_cyclic_forward_navigation(self):
        """7. Cyclic forward navigation: Page 5 -> Page 6 -> Page 7 -> Page 5."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.secili_goz = 2

        self.hmi.komutIsle("PAGE5_OPEN")
        self.assertEqual(self.hmi.current_service_page, 5)

        # 5 -> 6
        self.hmi.komutIsle("b_forwoard")
        self.assertEqual(self.hmi.current_service_page, 6)
        self.assertIn("page page6", self.hmi.nextion_tx_log)
        self.assertIn('t_goz_num.txt="2"', self.hmi.nextion_tx_log)

        # 6 -> 7
        self.hmi.komutIsle("b_forwoard")
        self.assertEqual(self.hmi.current_service_page, 7)
        self.assertIn("page page7", self.hmi.nextion_tx_log)
        self.assertIn('t_goz_num.txt="2"', self.hmi.nextion_tx_log)

        # 7 -> 8
        self.hmi.komutIsle("b_forwoard")
        self.assertEqual(self.hmi.current_service_page, 8)
        self.assertIn("page page8", self.hmi.nextion_tx_log)
        self.assertIn('t_goz_num.txt="2"', self.hmi.nextion_tx_log)

        # 8 -> 5
        self.hmi.komutIsle("b_forwoard")
        self.assertEqual(self.hmi.current_service_page, 5)
        self.assertIn("page page5", self.hmi.nextion_tx_log)
        self.assertIn('t_goz_num.txt="2"', self.hmi.nextion_tx_log)

    def test_arch_08_page_7_6_5_cyclic_back_navigation(self):
        """8. Cyclic back navigation: Page 5 -> Page 8 -> Page 7 -> Page 6 -> Page 5."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.secili_goz = 3

        self.hmi.komutIsle("PAGE5_OPEN")
        self.assertEqual(self.hmi.current_service_page, 5)

        # 5 -> 8
        self.hmi.komutIsle("b_back")
        self.assertEqual(self.hmi.current_service_page, 8)
        self.assertIn("page page8", self.hmi.nextion_tx_log)
        self.assertIn('t_goz_num.txt="3"', self.hmi.nextion_tx_log)

        # 8 -> 7
        self.hmi.komutIsle("b_back")
        self.assertEqual(self.hmi.current_service_page, 7)
        self.assertIn("page page7", self.hmi.nextion_tx_log)
        self.assertIn('t_goz_num.txt="3"', self.hmi.nextion_tx_log)

        # 7 -> 6
        self.hmi.komutIsle("b_back")
        self.assertEqual(self.hmi.current_service_page, 6)
        self.assertIn("page page6", self.hmi.nextion_tx_log)
        self.assertIn('t_goz_num.txt="3"', self.hmi.nextion_tx_log)

        # 6 -> 5
        self.hmi.komutIsle("b_back")
        self.assertEqual(self.hmi.current_service_page, 5)
        self.assertIn("page page5", self.hmi.nextion_tx_log)
        self.assertIn('t_goz_num.txt="3"', self.hmi.nextion_tx_log)

    def test_arch_09_exit_service_menu_to_page3(self):
        """9. Exit from Page 5, 6, 7, or 8 returns to Page 0 and locks auth."""
        for page in [5, 6, 7, 8]:
            self.hmi.g_service_authenticated = True
            self.hmi.service_auth_time = self.hmi.millis_ms
            self.hmi.current_service_page = page
            self.hmi.komutIsle("b_exit")
            self.assertIn("page page0", self.hmi.nextion_tx_log)
            self.assertFalse(self.hmi.g_service_authenticated)

    def test_arch_10_page6_selected_tank_sweep_isolation(self):
        """10. Page 6 selected tank Sweep settings isolation."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms

        self.hmi.secili_goz = 1
        self.hmi.komutIsle("PAGE6_OPEN")
        self.hmi.komutIsle("SWP_SPAN_UP")  # Tank 1: span=3 in draft
        self.assertEqual(self.hmi.edit_service_sweep[1]["span_khz"], 3)
        self.hmi.komutIsle("PAGE6_SAVE")
        self.assertEqual(self.hmi.service_sweep[1]["span_khz"], 3)

        self.hmi.secili_goz = 2
        self.hmi.komutIsle("PAGE6_OPEN")
        self.assertEqual(self.hmi.service_sweep[2]["span_khz"], 2, "Tank 2 sweep span must remain default 2 kHz")

    def test_arch_11_page7_selected_tank_degas_isolation(self):
        """11. Page 7 selected tank DEGAS configuration isolation."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms

        self.hmi.secili_goz = 1
        self.hmi.komutIsle("PAGE7_OPEN")
        self.hmi.komutIsle("DEG_DUR_UP")  # Tank 1: 16 min in draft
        self.assertEqual(self.hmi.edit_service_degas[1]["duration_minutes"], 16)
        self.hmi.komutIsle("PAGE7_SAVE")
        self.assertEqual(self.hmi.service_degas[1]["duration_minutes"], 16)

        self.hmi.secili_goz = 2
        self.hmi.komutIsle("PAGE7_OPEN")
        self.assertEqual(self.hmi.service_degas[2]["duration_minutes"], 15, "Tank 2 DEGAS duration must remain default 15 min")

    def test_arch_12_reload_selected_tank_after_navigation(self):
        """12. Navigating through 5->6->7 retains active tank selection and reloads correct tank fields."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms

        self.hmi.secili_goz = 2
        self.hmi.service_degas[2]["duration_minutes"] = 45
        self.hmi.service_sweep[2]["span_khz"] = 4
        self.hmi.initServiceEditBuffers()

        self.hmi.komutIsle("PAGE5_OPEN")
        self.hmi.komutIsle("b_forwoard")  # to Page 6
        self.assertIn('t_swp_span.txt="4"', self.hmi.nextion_tx_log)

        self.hmi.komutIsle("b_forwoard")  # to Page 7
        self.assertIn('t_deg_dur.txt="45"', self.hmi.nextion_tx_log)

    def test_arch_13_service_authentication_protection(self):
        """13. Service authentication protects saving and configuration commands."""
        self.hmi.g_service_authenticated = False
        self.hmi.komutIsle("SRV_SAVE")
        self.assertIn("b_save.bco=63488", self.hmi.nextion_tx_log)

        self.hmi.komutIsle("NAV_FORWARD")
        self.assertIn("b_save.bco=63488", self.hmi.nextion_tx_log)

        self.hmi.komutIsle("SRV_GUC_UP")
        # Unauthenticated mutation must not occur
        self.assertEqual(self.hmi.edit_guc_seviyesi[1], 50)

    def test_arch_14_active_process_lockouts(self):
        """14. Active process locks protected setpoints and service edits."""
        self.hmi.makine_calisiyor[1] = True
        self.hmi.komutIsle("P1_SEL")
        self.assertNotIn("T1:SET_TIME:15", self.hmi.stm32_tx_log)

        self.hmi.komutIsle("CMD_FREQ|40")
        self.assertNotIn("T1:SET_FREQ:40", self.hmi.stm32_tx_log)

    def test_arch_15_degas_sweep_mutual_exclusion(self):
        """15. DEGAS and Sweep mutual exclusion invariants."""
        # Arm DEGAS -> Sweep is turned OFF
        self.hmi.runtime_sweep[1] = True
        self.hmi.komutIsle("b_deg")
        self.assertTrue(self.hmi.degas_armed[1])
        self.assertFalse(self.hmi.runtime_sweep[1])
        self.assertIn("T1:SWEEP:OFF", self.hmi.stm32_tx_log)

        # Toggle Sweep ON -> Disarms DEGAS
        self.hmi.komutIsle("b_swe")
        self.assertTrue(self.hmi.runtime_sweep[1])
        self.assertFalse(self.hmi.degas_armed[1])
        self.assertIn("b_deg.bco=50712", self.hmi.nextion_tx_log)

    def test_arch_16_page0_freq_toggle(self):
        """16. Page 0 Frequency Toggle (28k <-> 40k) and Interlock / UI Sync."""
        # 1. Initial boot: Tank 1 freq is 28 kHz, Page 0 shows 28k
        self.hmi.secili_goz = 1
        self.hmi.komutIsle("PAGE0_OPEN")
        self.assertEqual(self.hmi.stm_freq[1], 28)
        self.assertIn('b_freq.txt="28k"', self.hmi.nextion_tx_log)

        # 2. Toggle -> 40 kHz, emits T1:SET_FREQ:40, UI updates to 40k
        self.hmi.komutIsle("CMD_FREQ_TOGGLE")
        self.assertEqual(self.hmi.stm_freq[1], 40)
        self.assertIn("T1:SET_FREQ:40", self.hmi.stm32_tx_log)
        self.assertIn('b_freq.txt="40k"', self.hmi.nextion_tx_log)

        # 3. Toggle again -> 28 kHz, emits T1:SET_FREQ:28, UI updates to 28k
        self.hmi.komutIsle("b_freq")
        self.assertEqual(self.hmi.stm_freq[1], 28)
        self.assertIn("T1:SET_FREQ:28", self.hmi.stm32_tx_log)
        self.assertIn('b_freq.txt="28k"', self.hmi.nextion_tx_log)

        # 4. DEGAS armed -> frequency toggle disarms DEGAS
        self.hmi.komutIsle("b_deg")
        self.assertTrue(self.hmi.degas_armed[1])
        self.hmi.komutIsle("CMD_FREQ_TOGGLE")
        self.assertFalse(self.hmi.degas_armed[1], "Frequency toggle must disarm DEGAS")
        self.assertEqual(self.hmi.stm_freq[1], 40)

        # 5. RUNNING interlock lockout
        self.hmi.makine_calisiyor[1] = True
        self.hmi.komutIsle("CMD_FREQ_TOGGLE")
        self.assertEqual(self.hmi.stm_freq[1], 40, "Frequency must NOT change while RUNNING")
        self.hmi.makine_calisiyor[1] = False

        # 6. DEGAS active interlock lockout
        self.hmi.degas_active[1] = True
        self.hmi.komutIsle("CMD_FREQ_TOGGLE")
        self.assertEqual(self.hmi.stm_freq[1], 40, "Frequency must NOT change while DEGAS active")
        self.hmi.degas_active[1] = False

        # 7. Tank isolation: Tank 2 frequency is distinct from Tank 1
        self.hmi.stm_freq[2] = 28
        self.hmi.secili_goz = 2
        self.hmi.komutIsle("PAGE0_OPEN")
        self.assertIn('b_freq.txt="28k"', self.hmi.nextion_tx_log)
        self.assertEqual(self.hmi.stm_freq[1], 40, "Tank 1 frequency must remain 40 kHz")

    def test_arch_17_freq_start_stop_sweep_lifecycle(self):
        """17. Targeted validation for frequency lifecycle (TEST-01 to TEST-10)."""
        # TEST-01: 28k -> START -> STOP -> 28k
        self.hmi.secili_goz = 1
        self.hmi.stm_freq[1] = 28
        self.hmi.hedef_sure[1] = 10
        self.hmi.hedef_sicaklik[1] = 40
        self.hmi.komutIsle("CMD_START|10|40")
        self.assertTrue(self.hmi.makine_calisiyor[1])
        self.assertIn("T1:SET_FREQ:28", self.hmi.stm32_tx_log)
        self.hmi.komutIsle("CMD_STOP")
        self.assertFalse(self.hmi.makine_calisiyor[1])
        self.assertEqual(self.hmi.stm_freq[1], 28, "TEST-01: 28k must be retained after STOP")
        self.assertIn('b_freq.txt="28k"', self.hmi.nextion_tx_log)

        # TEST-02: 40k -> START -> STOP -> 40k
        self.hmi.komutIsle("CMD_FREQ_TOGGLE")  # Toggle to 40k
        self.assertEqual(self.hmi.stm_freq[1], 40)
        self.hmi.stm32_tx_log.clear()
        self.hmi.komutIsle("CMD_START|10|40")
        self.assertTrue(self.hmi.makine_calisiyor[1])
        self.assertIn("T1:SET_FREQ:40", self.hmi.stm32_tx_log, "TEST-07: START must dispatch SET_FREQ:40")
        self.hmi.komutIsle("CMD_STOP")
        self.assertFalse(self.hmi.makine_calisiyor[1])
        self.assertEqual(self.hmi.stm_freq[1], 40, "TEST-02 & TEST-08: 40k must be retained after STOP without resetting to 28")
        self.assertIn('b_freq.txt="40k"', self.hmi.nextion_tx_log)

        # TEST-03: 40k -> START -> STOP -> START -> 40k
        self.hmi.stm32_tx_log.clear()
        self.hmi.komutIsle("CMD_START|15|45")
        self.assertIn("T1:SET_FREQ:40", self.hmi.stm32_tx_log, "TEST-03: Second START must still dispatch SET_FREQ:40")
        self.assertEqual(self.hmi.stm_freq[1], 40)
        self.hmi.komutIsle("CMD_STOP")
        self.assertEqual(self.hmi.stm_freq[1], 40)

        # TEST-04: 40k -> Sweep OFF -> Sweep ON -> Sweep OFF -> 40k
        self.hmi.runtime_sweep[1] = False
        self.hmi.komutIsle("b_swe")  # Sweep ON
        self.assertTrue(self.hmi.runtime_sweep[1])
        self.assertEqual(self.hmi.stm_freq[1], 40, "TEST-09: Sweep ON must retain 40k center frequency state")
        self.hmi.komutIsle("b_swe")  # Sweep OFF
        self.assertFalse(self.hmi.runtime_sweep[1])
        self.assertEqual(self.hmi.stm_freq[1], 40, "TEST-04 & TEST-10: Sweep OFF must restore manual 40k state")
        self.assertIn('b_freq.txt="40k"', self.hmi.nextion_tx_log)

        # TEST-05: 28k -> Sweep ON -> Sweep OFF -> 28k
        self.hmi.komutIsle("CMD_FREQ_TOGGLE")  # Toggle to 28k
        self.assertEqual(self.hmi.stm_freq[1], 28)
        self.hmi.komutIsle("b_swe")  # Sweep ON
        self.assertTrue(self.hmi.runtime_sweep[1])
        self.assertEqual(self.hmi.stm_freq[1], 28)
        self.hmi.komutIsle("b_swe")  # Sweep OFF
        self.assertFalse(self.hmi.runtime_sweep[1])
        self.assertEqual(self.hmi.stm_freq[1], 28, "TEST-05: Sweep OFF must restore manual 28k state")
        self.assertIn('b_freq.txt="28k"', self.hmi.nextion_tx_log)

        # TEST-06: Page 1 Göz A (Tank 1) = 40k, Göz B (Tank 2) = 28k
        self.hmi.stm_freq[1] = 40
        self.hmi.stm_freq[2] = 28
        self.hmi.secili_goz = 1
        self.hmi.komutIsle("PAGE0_OPEN")
        self.assertIn('b_freq.txt="40k"', self.hmi.nextion_tx_log)

        self.hmi.secili_goz = 2
        self.hmi.komutIsle("PAGE0_OPEN")
        self.assertIn('b_freq.txt="28k"', self.hmi.nextion_tx_log)
        self.assertEqual(self.hmi.stm_freq[1], 40, "TEST-06: Tank 1 frequency must remain isolated at 40k")

    # =========================================================================
    # PAGE 5 SERVICE MANAGEMENT SUITE (TESTS 1 - 11)
    # =========================================================================
    def test_page5_01_open(self):
        """TEST 1: Page5 open -> t_goz_num = Goz: 1, t_id = kart 1'in gerçek ID'si."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.kart_id[1] = 1
        self.hmi.secili_goz = 1
        self.hmi.initServiceEditBuffers()
        self.hmi.komutIsle("PAGE5_OPEN")
        self.assertIn('t_goz_num.txt="1"', self.hmi.nextion_tx_log)
        self.assertIn('t_id.txt="1"', self.hmi.nextion_tx_log)
        self.assertEqual(self.hmi.secili_goz, 1)

    def test_page5_02_b_up(self):
        """TEST 2: b_up -> t_goz_num = 2, t_id = kart 2'nin gerçek ID'si."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.kart_id[1] = 1
        self.hmi.kart_id[2] = 2
        self.hmi.secili_goz = 1
        self.hmi.initServiceEditBuffers()
        self.hmi.komutIsle("PAGE5_OPEN")
        self.hmi.komutIsle("b_up")
        self.assertEqual(self.hmi.secili_goz, 2)
        self.assertIn('t_goz_num.txt="2"', self.hmi.nextion_tx_log)
        self.assertIn('t_id.txt="2"', self.hmi.nextion_tx_log)

    def test_page5_03_b_up_second_time(self):
        """TEST 3: b_up -> t_goz_num = 3, t_id = kart 3'ün gerçek ID'si."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.kart_id[2] = 2
        self.hmi.kart_id[3] = 3
        self.hmi.secili_goz = 2
        self.hmi.initServiceEditBuffers()
        self.hmi.komutIsle("b_up")
        self.assertEqual(self.hmi.secili_goz, 3)
        self.assertIn('t_goz_num.txt="3"', self.hmi.nextion_tx_log)
        self.assertIn('t_id.txt="3"', self.hmi.nextion_tx_log)

    def test_page5_04_b_down(self):
        """TEST 4: b_down -> t_goz_num = 2, t_id = kart 2'nin gerçek ID'si."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.kart_id[2] = 2
        self.hmi.secili_goz = 3
        self.hmi.initServiceEditBuffers()
        self.hmi.komutIsle("b_down")
        self.assertEqual(self.hmi.secili_goz, 2)
        self.assertIn('t_goz_num.txt="2"', self.hmi.nextion_tx_log)
        self.assertIn('t_id.txt="2"', self.hmi.nextion_tx_log)

    def test_page5_05_kart2_b_id_up(self):
        """TEST 5: kart 2 seçiliyken b_id_up -> t_goz_num = 2, t_id = yeni ID, secili_goz değişmemeli."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.max_goz_sayisi = 10
        self.hmi.stm_bagli[2] = True
        self.hmi.stm_son_veri_zamani[2] = self.hmi.millis_ms
        self.hmi.secili_goz = 2
        self.hmi.kart_id[2] = 6
        self.hmi.initServiceEditBuffers()
        self.hmi.komutIsle("b_id_up")
        self.assertEqual(self.hmi.secili_goz, 2)
        self.assertEqual(self.hmi.edit_kart_id[2], 7)
        self.assertIn('t_id.txt="7"', self.hmi.nextion_tx_log)

    def test_page5_06_kart2_b_id_down(self):
        """TEST 6: kart 2 seçiliyken b_id_down -> t_goz_num = 2, t_id = önceki ID, secili_goz değişmemeli."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.max_goz_sayisi = 10
        self.hmi.stm_bagli[2] = True
        self.hmi.stm_son_veri_zamani[2] = self.hmi.millis_ms
        self.hmi.secili_goz = 2
        self.hmi.kart_id[2] = 7
        self.hmi.initServiceEditBuffers()
        self.hmi.komutIsle("b_id_down")
        self.assertEqual(self.hmi.secili_goz, 2)
        self.assertEqual(self.hmi.edit_kart_id[2], 6)
        self.assertIn('t_id.txt="6"', self.hmi.nextion_tx_log)

    def test_page5_07_kart2_id_change_and_navigate(self):
        """TEST 7: kart 2 ID değiştir -> kart 1'e git -> tekrar kart 2'ye git -> ID korunmalı."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.max_goz_sayisi = 10
        self.hmi.stm_bagli[1] = True
        self.hmi.stm_son_veri_zamani[1] = self.hmi.millis_ms
        self.hmi.stm_bagli[2] = True
        self.hmi.stm_son_veri_zamani[2] = self.hmi.millis_ms
        self.hmi.kart_id[1] = 1
        self.hmi.kart_id[2] = 6
        self.hmi.initServiceEditBuffers()
        self.hmi.secili_goz = 2
        self.hmi.komutIsle("PAGE5_OPEN")

        # Kart 2 ID'sini 7 yap (in draft)
        self.hmi.komutIsle("b_id_up")
        self.assertEqual(self.hmi.edit_kart_id[2], 7)

        # Kart 1'e git (b_down)
        self.hmi.komutIsle("b_down")
        self.assertEqual(self.hmi.secili_goz, 1)
        self.assertIn('t_goz_num.txt="1"', self.hmi.nextion_tx_log)
        self.assertIn('t_id.txt="1"', self.hmi.nextion_tx_log)

        # Tekrar Kart 2'ye git (b_up) - draft ID 7 korunmalı!
        self.hmi.komutIsle("b_up")
        self.assertEqual(self.hmi.secili_goz, 2)
        self.assertIn('t_goz_num.txt="2"', self.hmi.nextion_tx_log)
        self.assertIn('t_id.txt="7"', self.hmi.nextion_tx_log)

    def test_page5_08_id_collision_prevention_scenario(self):
        """TEST 8: ID collision: Bir kartı başka kartın mevcut ID'sine taşıdığında otomatik remapping/swap."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.kart_id[1] = 1
        self.hmi.kart_id[2] = 2
        self.hmi.kart_id[3] = 3
        self.hmi.initServiceEditBuffers()

        # Kart 1 seçili, b_id_up ile ID'yi 1 -> 2 yap (Kart 2 ile çakışma)
        self.hmi.secili_goz = 1
        self.hmi.komutIsle("b_id_up")
        self.assertEqual(self.hmi.edit_kart_id[1], 2)
        self.assertEqual(self.hmi.edit_kart_id[2], 1)  # Kart 2 otomatik olarak Kart 1'in eski ID'si 1'e taşındı
        self.assertEqual(self.hmi.edit_kart_id[3], 3)
        self.assertIn('t_id.txt="2"', self.hmi.nextion_tx_log)

    def test_page5_09_cyclic_nav_p5_p6_p5(self):
        """TEST 9: Page5 -> Page6 -> Page5 -> secili_goz korunmalı, t_goz_num doğru, t_id doğru."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.secili_goz = 2
        self.hmi.kart_id[2] = 8
        self.hmi.initServiceEditBuffers()

        self.hmi.komutIsle("PAGE5_OPEN")
        self.assertEqual(self.hmi.current_service_page, 5)

        # Page 5 -> Page 6
        self.hmi.komutIsle("b_forwoard")
        self.assertEqual(self.hmi.current_service_page, 6)
        self.assertIn("page page6", self.hmi.nextion_tx_log)
        self.assertIn('t_goz_num.txt="2"', self.hmi.nextion_tx_log)

        # Page 6 -> Page 5 (via NAV_BACK)
        self.hmi.komutIsle("NAV_BACK")
        self.assertEqual(self.hmi.current_service_page, 5)
        self.assertEqual(self.hmi.secili_goz, 2)
        self.assertIn('t_goz_num.txt="2"', self.hmi.nextion_tx_log)
        self.assertIn('t_id.txt="8"', self.hmi.nextion_tx_log)

    def test_page5_10_cyclic_nav_p5_p7_p5(self):
        """TEST 10: Page5 -> Page7 -> Page5 cycle."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.secili_goz = 3
        self.hmi.kart_id[3] = 9
        self.hmi.initServiceEditBuffers()

        self.hmi.komutIsle("PAGE5_OPEN")
        self.hmi.komutIsle("b_forwoard")  # 5 -> 6
        self.hmi.komutIsle("b_forwoard")  # 6 -> 7
        self.assertEqual(self.hmi.current_service_page, 7)
        self.assertIn('t_goz_num.txt="3"', self.hmi.nextion_tx_log)

        # 7 -> 8 -> 5
        self.hmi.komutIsle("b_forwoard")  # 7 -> 8
        self.hmi.komutIsle("b_forwoard")  # 8 -> 5
        self.assertEqual(self.hmi.current_service_page, 5)
        self.assertEqual(self.hmi.secili_goz, 3)
        self.assertIn('t_goz_num.txt="3"', self.hmi.nextion_tx_log)
        self.assertIn('t_id.txt="9"', self.hmi.nextion_tx_log)

    def test_page5_11_cyclic_nav_p5_p8_p5(self):
        """TEST 11: Page5 -> Page8 -> Page5 cycle."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.secili_goz = 2
        self.hmi.kart_id[2] = 4
        self.hmi.initServiceEditBuffers()

        self.hmi.komutIsle("PAGE5_OPEN")
        # 5 -> 8 (via NAV_BACK)
        self.hmi.komutIsle("NAV_BACK")
        self.assertEqual(self.hmi.current_service_page, 8)
        self.assertIn("page page8", self.hmi.nextion_tx_log)
        self.assertIn('t_goz_num.txt="2"', self.hmi.nextion_tx_log)

        # 8 -> 5 (via NAV_FORWARD)
        self.hmi.komutIsle("b_forwoard")
        self.assertEqual(self.hmi.current_service_page, 5)
        self.assertEqual(self.hmi.secili_goz, 2)
        self.assertIn('t_goz_num.txt="2"', self.hmi.nextion_tx_log)
        self.assertIn('t_id.txt="4"', self.hmi.nextion_tx_log)

    def test_page0_manual_mode_and_cmd_pipe(self):
        """TEST: Page 0 MANUAL_MODE, CMD_START|<sure>|<sic> and CMD_STOP parsing."""
        self.hmi.stmTelemetryIsle("STAT,1,IDLE,0,250,0,50,28,0")
        self.hmi.komutIsle("MANUAL_MODE")
        self.assertEqual(self.hmi.aktif_program, 0)
        self.assertEqual(self.hmi.durum_metni[1], "MANUEL MOD SECILDI")
        self.assertIn('t_durum.txt="MANUEL MOD SECILDI"', self.hmi.nextion_tx_log)

        # Pipe-delimited CMD_START
        self.hmi.komutIsle("CMD_START|18|55")
        self.assertTrue(self.hmi.makine_calisiyor[1])
        self.assertEqual(self.hmi.hedef_sure[1], 18)
        self.assertEqual(self.hmi.hedef_sicaklik[1], 55)
        self.assertIn("T1:START", self.hmi.stm32_tx_log)

        # CMD_STOP
        self.hmi.komutIsle("CMD_STOP")
        self.assertFalse(self.hmi.makine_calisiyor[1])
        self.assertIn("T1:STOP", self.hmi.stm32_tx_log)

    def test_service_new_aliases_and_atomic_save(self):
        """TEST: Service Page 5-8 new SRV_* command aliases and atomic SRV_SAVE."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.secili_goz = 1

        self.hmi.komutIsle("PAGE5_OPEN")
        self.hmi.komutIsle("SRV_GUC_UP")
        self.assertEqual(self.hmi.edit_guc_seviyesi[1], 60)
        self.hmi.komutIsle("SRV_ID_UP")
        self.assertEqual(self.hmi.edit_kart_id[1], 2)

        # Switch to Tank 2 with SRV_TANK_UP
        self.hmi.komutIsle("SRV_TANK_UP")
        self.assertEqual(self.hmi.secili_goz, 2)

        # Page 6: Sweep aliases
        self.hmi.komutIsle("PAGE6_OPEN")
        self.hmi.komutIsle("SRV_SPAN_UP")
        self.assertEqual(self.hmi.edit_service_sweep[2]["span_khz"], 3)
        self.hmi.komutIsle("SRV_PER_UP")
        self.assertEqual(self.hmi.edit_service_sweep[2]["period_ms"], 500)
        self.hmi.komutIsle("SRV_STEP_UP")
        self.assertEqual(self.hmi.edit_service_sweep[2]["step_increment"], 5)

        # Page 7 & 8: Degas aliases
        self.hmi.komutIsle("PAGE7_OPEN")
        self.hmi.komutIsle("SRV_DDUR_UP")
        self.assertEqual(self.hmi.edit_service_degas[2]["duration_minutes"], 16)
        self.hmi.komutIsle("SRV_DPOW_DOWN")
        self.assertEqual(self.hmi.edit_service_degas[2]["power_pct"], 90)
        self.hmi.komutIsle("SRV_DFREQ_UP")
        self.assertEqual(self.hmi.edit_service_degas[2]["frequency_khz"], 29)
        self.hmi.komutIsle("SRV_DPON_UP")
        self.assertEqual(self.hmi.edit_service_degas[2]["pulse_on_ms"], 1100)
        self.hmi.komutIsle("SRV_DPOFF_UP")
        self.assertEqual(self.hmi.edit_service_degas[2]["pulse_off_ms"], 600)
        self.hmi.komutIsle("SRV_DTCTRL_TOG")
        self.assertEqual(self.hmi.edit_service_degas[2]["temp_ctrl"], 1)
        self.hmi.komutIsle("SRV_DTEMP_UP")
        self.assertEqual(self.hmi.edit_service_degas[2]["target_temp_c"], 51.0)

        # Atomic SRV_SAVE
        self.hmi.komutIsle("SRV_SAVE")
        self.assertIn(f"b_save.bco={self.hmi.NEXTION_COLOR_GREEN}", self.hmi.nextion_tx_log)
        self.assertEqual(self.hmi.guc_seviyesi[1], 60)
        self.assertEqual(self.hmi.kart_id[1], 2)
        self.assertEqual(self.hmi.service_sweep[2]["span_khz"], 3)
        self.assertEqual(self.hmi.service_degas[2]["duration_minutes"], 16)
        self.assertEqual(self.hmi.nvs_ultra["guc_1"], 60)
        self.assertEqual(self.hmi.nvs_ultra["sw_sp_2"], 3)

    def test_page1_and_page2_and_v2_objects(self):
        """TEST: Page 1 tank selection, Page 2 EDIT_SWEEP_TOG, and Nextion V2 object names."""
        # Page 1
        self.hmi.komutIsle("PAGE1_OPEN")
        self.assertIn('t0.txt="1"', self.hmi.nextion_tx_log)
        self.hmi.komutIsle("TANK_UP")
        self.assertEqual(self.hmi.temp_goz, 2)
        self.assertIn('t0.txt="2"', self.hmi.nextion_tx_log)
        self.hmi.komutIsle("TANK_SEL_OK")
        self.assertEqual(self.hmi.secili_goz, 2)

        # Page 2
        self.hmi.komutIsle("PAGE2_OPEN")
        self.hmi.komutIsle("EDIT_P2")
        self.hmi.komutIsle("EDIT_SWEEP_TOG")
        self.assertEqual(self.hmi.edit_p_sweep[2], 1)  # draft toggled from 0 to 1
        self.assertEqual(self.hmi.p_sweep[2], 0)       # master unchanged until P_SAVE

        # Page 5-8 HMI objects check
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.komutIsle("PAGE5_OPEN")
        self.assertIn('t_srv_goz.txt="2"', self.hmi.nextion_tx_log)
        self.assertIn('t_srv_guc.txt="50"', self.hmi.nextion_tx_log)
        self.assertIn('t_srv_id.txt="2"', self.hmi.nextion_tx_log)

        self.hmi.komutIsle("PAGE6_OPEN")
        self.assertIn('t_swp_goz.txt="2"', self.hmi.nextion_tx_log)
        self.assertIn('t_swp_span.txt="2"', self.hmi.nextion_tx_log)
        self.assertIn('t_swp_per.txt="400"', self.hmi.nextion_tx_log)
        self.assertIn('t_swp_step.txt="4"', self.hmi.nextion_tx_log)

        self.hmi.komutIsle("PAGE7_OPEN")
        self.assertIn('t_deg_goz.txt="2"', self.hmi.nextion_tx_log)
        self.assertIn('t_deg_dur.txt="15"', self.hmi.nextion_tx_log)
        self.assertIn('t_deg_pow.txt="100"', self.hmi.nextion_tx_log)
        self.assertIn('t_deg_freq.txt="28"', self.hmi.nextion_tx_log)

        self.hmi.komutIsle("PAGE8_OPEN")
        self.assertIn('t_deg_pon.txt="1000"', self.hmi.nextion_tx_log)
        self.assertIn('t_deg_poff.txt="500"', self.hmi.nextion_tx_log)
        self.assertIn('t_deg_tctrl.txt="KAPALI"', self.hmi.nextion_tx_log)

    def test_sim_mirror_bypass_and_page_names(self):
        """TEST: isKartBagli and Page 0/2 button color persistence."""
        # Without STM veri, isKartBagli returns False
        self.hmi.millis_ms = 10000
        self.hmi.stm_son_veri_zamani[1] = 0
        self.assertFalse(self.hmi.isKartBagli(1))

        # With fresh telemetry, isKartBagli returns True
        self.hmi.stm_son_veri_zamani[1] = 10000
        self.assertTrue(self.hmi.isKartBagli(1))

        # Start process with active card
        self.hmi.hedef_sure[1] = 10
        self.hmi.komutIsle("CMD_START|10|45")
        self.assertTrue(self.hmi.makine_calisiyor[1])
        self.assertNotIn('t_durum.txt="Kart Yok!"', self.hmi.nextion_tx_log[-3:])

        # Page 0 button color checks for b_prog_p1, b_degas, b_sweep
        self.hmi.komutIsle("CMD_STOP")
        self.hmi.komutIsle("P1_SEL")
        self.assertIn(f"b_prog_p1.bco={self.hmi.NEXTION_COLOR_GREEN}", self.hmi.nextion_tx_log)
        self.assertIn(f"b_prog_p2.bco={self.hmi.NEXTION_COLOR_DEFAULT}", self.hmi.nextion_tx_log)

        # MANUAL_MODE resets all to default
        self.hmi.komutIsle("MANUAL_MODE")
        self.assertIn(f"b_prog_p1.bco={self.hmi.NEXTION_COLOR_DEFAULT}", self.hmi.nextion_tx_log)
        self.assertIn(f"b_prog_fp.bco={self.hmi.NEXTION_COLOR_DEFAULT}", self.hmi.nextion_tx_log)

    def test_page2_sweep_immediate_ui_update(self):
        """TEST: Page 2 SWEEP toggle responds immediately and updates both b_edit_sweep.bco and b_swe.bco."""
        self.hmi.komutIsle("PAGE2_OPEN")
        self.hmi.nextion_tx_log.clear()

        # Initial state: P1 sweep is 0
        self.assertEqual(self.hmi.edit_p_sweep[1], 0)
        self.assertEqual(self.hmi.p_sweep[1], 0)

        # Toggle sweep with EDIT_SWEEP_TOG (mutates draft immediately)
        self.hmi.komutIsle("EDIT_SWEEP_TOG")
        self.assertEqual(self.hmi.edit_p_sweep[1], 1)
        self.assertEqual(self.hmi.p_sweep[1], 0)
        self.assertIn(f"b_edit_sweep.bco={self.hmi.NEXTION_COLOR_GREEN}", self.hmi.nextion_tx_log)
        self.assertIn(f"b_swe.bco={self.hmi.NEXTION_COLOR_GREEN}", self.hmi.nextion_tx_log)

        # Toggle sweep with b_edit_sweep
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("b_edit_sweep")
        self.assertEqual(self.hmi.edit_p_sweep[1], 0)
        self.assertEqual(self.hmi.p_sweep[1], 0)
        self.assertIn(f"b_edit_sweep.bco={self.hmi.NEXTION_COLOR_DEFAULT}", self.hmi.nextion_tx_log)
        self.assertIn(f"b_swe.bco={self.hmi.NEXTION_COLOR_DEFAULT}", self.hmi.nextion_tx_log)

        # Toggle sweep with b_swe while on page 2
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("b_swe")
        self.assertEqual(self.hmi.edit_p_sweep[1], 1)
        self.assertEqual(self.hmi.p_sweep[1], 0)
        self.assertIn(f"b_edit_sweep.bco={self.hmi.NEXTION_COLOR_GREEN}", self.hmi.nextion_tx_log)
        self.assertIn(f"b_swe.bco={self.hmi.NEXTION_COLOR_GREEN}", self.hmi.nextion_tx_log)

    def test_auto_load_on_page_open_all_pages(self):
        """TEST: Auto-Load on Page Open mechanism updates UI elements on all pages immediately upon entry."""
        # Page 0
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("PAGE0_OPEN")
        self.assertEqual(self.hmi.aktif_sayfa, 0)
        self.assertIn("page page0", self.hmi.nextion_tx_log)
        self.assertTrue(any('b_goz' in cmd for cmd in self.hmi.nextion_tx_log))

        # Page 1
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("PAGE1_OPEN")
        self.assertEqual(self.hmi.aktif_sayfa, 1)
        self.assertIn("page page1", self.hmi.nextion_tx_log)
        self.assertTrue(any('t_secili_goz.txt' in cmd for cmd in self.hmi.nextion_tx_log))

        # Page 2
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("PAGE2_OPEN")
        self.assertEqual(self.hmi.aktif_sayfa, 2)
        self.assertIn("page page2", self.hmi.nextion_tx_log)
        self.assertTrue(any('t_prog_baslik.txt' in cmd or 't0.txt' in cmd for cmd in self.hmi.nextion_tx_log))

        # Page 3
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("PAGE3_OPEN")
        self.assertEqual(self.hmi.aktif_sayfa, 3)
        self.assertIn("page page3", self.hmi.nextion_tx_log)
        self.assertTrue(any('t0.txt="AYARLAR"' in cmd for cmd in self.hmi.nextion_tx_log))

        # Page 4
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("PAGE4_OPEN")
        self.assertEqual(self.hmi.aktif_sayfa, 4)
        self.assertIn("page page4", self.hmi.nextion_tx_log)
        self.assertTrue(any('t_sifre.txt=""' in cmd for cmd in self.hmi.nextion_tx_log))

        # Unlock service
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms

        # Page 5
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("PAGE5_OPEN")
        self.assertEqual(self.hmi.aktif_sayfa, 5)
        self.assertTrue(any('t_srv_guc.txt' in cmd or 't_guc.txt' in cmd for cmd in self.hmi.nextion_tx_log))

        # Page 6
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("PAGE6_OPEN")
        self.assertEqual(self.hmi.aktif_sayfa, 6)
        self.assertTrue(any('t_swp_span.txt' in cmd or 't_swp_period.txt' in cmd for cmd in self.hmi.nextion_tx_log))

        # Page 7
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("PAGE7_OPEN")
        self.assertEqual(self.hmi.aktif_sayfa, 7)
        self.assertTrue(any('t_deg_dur.txt' in cmd or 't_deg_pwr.txt' in cmd for cmd in self.hmi.nextion_tx_log))

        # Page 8
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("PAGE8_OPEN")
        self.assertEqual(self.hmi.aktif_sayfa, 8)
        self.assertTrue(any('t_deg_pon.txt' in cmd or 't_deg_on.txt' in cmd for cmd in self.hmi.nextion_tx_log))

    def test_page2_draft_memory_isolation(self):
        """TEST: Page 2 parameter changes must ONLY mutate draft buffers until P_SAVE commits them."""
        # Setup initial master recipe state
        self.hmi.p_sure[1] = 15
        self.hmi.p_sicaklik[1] = 40
        self.hmi.p_sweep[1] = 0

        # Open Page 2 -> Loads master recipe into draft buffers
        self.hmi.komutIsle("PAGE2_OPEN")
        self.assertEqual(self.hmi.duzenlenen_program, 1)
        self.assertEqual(self.hmi.edit_p_sure[1], 15)
        self.assertEqual(self.hmi.edit_p_sicaklik[1], 40)
        self.assertEqual(self.hmi.edit_p_sweep[1], 0)

        # Mutate draft on Page 2 (increment time, temp, sweep)
        self.hmi.komutIsle("P_TIME_UP")
        self.hmi.komutIsle("P_TEMP_UP")
        self.hmi.komutIsle("EDIT_SWEEP_TOG")

        # Draft values should have changed
        self.assertEqual(self.hmi.edit_p_sure[1], 16)
        self.assertEqual(self.hmi.edit_p_sicaklik[1], 41)
        self.assertEqual(self.hmi.edit_p_sweep[1], 1)

        # CRITICAL: Master variables must remain UNCHANGED before SAVE!
        self.assertEqual(self.hmi.p_sure[1], 15)
        self.assertEqual(self.hmi.p_sicaklik[1], 40)
        self.assertEqual(self.hmi.p_sweep[1], 0)

        # Discard on BACK
        self.hmi.komutIsle("BACK")
        self.assertEqual(self.hmi.aktif_sayfa, 3)
        self.assertEqual(self.hmi.p_sure[1], 15)
        self.assertEqual(self.hmi.p_sicaklik[1], 40)
        self.assertEqual(self.hmi.p_sweep[1], 0)
        # Re-opening Page 2 should reload original master values
        self.hmi.komutIsle("PAGE2_OPEN")
        self.assertEqual(self.hmi.edit_p_sure[1], 15)
        self.assertEqual(self.hmi.edit_p_sicaklik[1], 40)
        self.assertEqual(self.hmi.edit_p_sweep[1], 0)

        # Now test commit on P_SAVE / P_KAYDET
        self.hmi.komutIsle("P_TIME_UP")   # edit_p_sure = 16
        self.hmi.komutIsle("P_TEMP_UP")   # edit_p_sicaklik = 41
        self.hmi.komutIsle("EDIT_SWEEP_TOG") # edit_p_sweep = 1

        # Test switching to P2 without saving P1: P1 uncommitted edits discarded on return
        self.hmi.komutIsle("EDIT_P2")
        self.assertEqual(self.hmi.duzenlenen_program, 2)
        self.hmi.komutIsle("EDIT_P1")
        self.assertEqual(self.hmi.duzenlenen_program, 1)
        self.assertEqual(self.hmi.edit_p_sure[1], 15) # cleanly reloaded from p_sure[1]

        # Mutate again and commit with P_KAYDET
        self.hmi.komutIsle("P_TIME_UP")
        self.hmi.komutIsle("P_TEMP_UP")
        self.hmi.komutIsle("EDIT_SWEEP_TOG")
        self.hmi.komutIsle("P_KAYDET")

        # Master values must now be committed
        self.assertEqual(self.hmi.p_sure[1], 16)
        self.assertEqual(self.hmi.p_sicaklik[1], 41)
        self.assertEqual(self.hmi.p_sweep[1], 1)

    def test_service_global_draft_and_title_and_commit_discard(self):
        """TEST: Service mode global draft across Page 5,6,7,8 & tanks, Page 6 title, and global commit/discard."""
        # Authenticate service
        self.hmi.girilen_sifre = "123456"
        self.hmi.komutIsle("KEY_OK")
        self.assertTrue(self.hmi.g_service_authenticated)
        self.assertEqual(self.hmi.aktif_sayfa, 5)

        # Mutate Tank 1 draft power
        self.hmi.komutIsle("SRV_GUC_UP") # 50 -> 60
        self.hmi.komutIsle("SRV_GUC_UP") # 60 -> 70
        self.assertEqual(self.hmi.edit_guc_seviyesi[1], 70)
        self.assertEqual(self.hmi.guc_seviyesi[1], 50) # Master unchanged!

        # Switch to Tank 2 and mutate Tank 2 draft power
        self.hmi.komutIsle("SRV_TANK_UP")
        self.assertEqual(self.hmi.secili_goz, 2)
        self.hmi.komutIsle("SRV_GUC_UP") # 50 -> 60
        self.hmi.komutIsle("SRV_GUC_UP") # 60 -> 70
        self.hmi.komutIsle("SRV_GUC_UP") # 70 -> 80
        self.assertEqual(self.hmi.edit_guc_seviyesi[2], 80)
        self.assertEqual(self.hmi.guc_seviyesi[2], 50) # Master unchanged!

        # Navigate to Page 6 (Sweep settings)
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("NAV_FORWARD")
        self.assertEqual(self.hmi.aktif_sayfa, 6)
        # Verify Page 6 Title is untouched by ESP32, and t_swp_goz is updated
        self.assertFalse(any(cmd.startswith('t0.txt=') for cmd in self.hmi.nextion_tx_log))
        self.assertIn('t_swp_goz.txt="2"', self.hmi.nextion_tx_log)

        # Mutate Tank 2 sweep span in draft
        self.hmi.komutIsle("SRV_SPAN_UP") # 2 -> 3
        self.assertEqual(self.hmi.edit_service_sweep[2]["span_khz"], 3)
        self.assertEqual(self.hmi.service_sweep[2]["span_khz"], 2) # Master unchanged!

        # Navigate around through Page 7 -> 8 -> 5
        self.hmi.komutIsle("NAV_FORWARD") # Page 7
        self.assertEqual(self.hmi.aktif_sayfa, 7)
        self.hmi.komutIsle("NAV_FORWARD") # Page 8
        self.assertEqual(self.hmi.aktif_sayfa, 8)
        self.hmi.komutIsle("NAV_FORWARD") # Page 5
        self.assertEqual(self.hmi.aktif_sayfa, 5)

        # Verify all drafts preserved on return to Page 5
        self.assertEqual(self.hmi.edit_guc_seviyesi[1], 70)
        self.assertEqual(self.hmi.edit_guc_seviyesi[2], 80)
        self.assertEqual(self.hmi.edit_service_sweep[2]["span_khz"], 3)

        # Test SRV_SAVE (Global commit)
        self.hmi.komutIsle("SRV_SAVE")
        self.assertEqual(self.hmi.guc_seviyesi[1], 70)
        self.assertEqual(self.hmi.guc_seviyesi[2], 80)
        self.assertEqual(self.hmi.service_sweep[2]["span_khz"], 3)

        # Test SRV_DISCARD
        self.hmi.komutIsle("SRV_GUC_UP") # draft edited to 90
        self.assertEqual(self.hmi.edit_guc_seviyesi[2], 90)
        self.hmi.komutIsle("SRV_DISCARD")
        self.assertFalse(self.hmi.g_service_authenticated)
        self.assertEqual(self.hmi.aktif_sayfa, 0)
        self.assertIn("page page0", self.hmi.nextion_tx_log)
        self.assertEqual(self.hmi.guc_seviyesi[2], 80) # Uncommitted 90 discarded!

    def test_page6_autoload_and_tank_up_down_and_no_t0_overwrite(self):
        """TEST: Page 6 Auto-Load on PAGE6_OPEN, b_swp_goz_up/down responsiveness, and no t0 overwrite."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.secili_goz = 2
        self.hmi.edit_service_sweep[2]["span_khz"] = 4
        self.hmi.edit_service_sweep[2]["period_ms"] = 600
        self.hmi.edit_service_sweep[2]["step_increment"] = 6

        # 1. Page 6 Auto-Load on PAGE6_OPEN
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("PAGE6_OPEN")
        self.assertEqual(self.hmi.aktif_sayfa, 6)
        self.assertEqual(self.hmi.current_service_page, 6)
        # Verify UI updated with tank 2's data
        self.assertIn('t_swp_goz.txt="2"', self.hmi.nextion_tx_log)
        self.assertIn('t_swp_span.txt="4"', self.hmi.nextion_tx_log)
        self.assertIn('t_swp_per.txt="600"', self.hmi.nextion_tx_log)
        self.assertIn('t_swp_step.txt="6"', self.hmi.nextion_tx_log)
        # Verify t0 is NEVER overwritten on Page 6
        self.assertFalse(any(cmd.startswith('t0.txt=') for cmd in self.hmi.nextion_tx_log))

        # 2. Page 6 Tank Up via b_swp_goz_up
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("b_swp_goz_up")
        self.assertEqual(self.hmi.secili_goz, 3)
        self.assertIn('t_swp_goz.txt="3"', self.hmi.nextion_tx_log)
        self.assertFalse(any(cmd.startswith('t0.txt=') for cmd in self.hmi.nextion_tx_log))

        # 3. Page 6 Tank Down via b_swp_goz_down
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("b_swp_goz_down")
        self.assertEqual(self.hmi.secili_goz, 2)
        self.assertIn('t_swp_goz.txt="2"', self.hmi.nextion_tx_log)
        self.assertFalse(any(cmd.startswith('t0.txt=') for cmd in self.hmi.nextion_tx_log))

    # =========================================================================
    # TARGETED VERIFICATION SUITE: TEST-01 THROUGH TEST-08
    # =========================================================================

    def test_srv_sync_01_page5_tank4_nav_forward_to_page6(self):
        """TEST-01: Page 5'te secili_goz = 4 iken NAV_FORWARD ile Page 6 açıldığında t_swp_goz.txt="4" gönderilir."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.edit_max_goz_sayisi = 4
        self.hmi.secili_goz = 4
        self.hmi.current_service_page = 5
        self.hmi.aktif_sayfa = 5

        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("NAV_FORWARD")

        self.assertEqual(self.hmi.current_service_page, 6)
        self.assertEqual(self.hmi.aktif_sayfa, 6)
        self.assertEqual(self.hmi.secili_goz, 4)
        self.assertIn("page page6", self.hmi.nextion_tx_log)
        self.assertIn('t_swp_goz.txt="4"', self.hmi.nextion_tx_log)

    def test_srv_sync_02_page6_immediate_draft_values_without_button_press(self):
        """TEST-02: Page 5'ten Page 6'ya geçildiğinde hiçbir butona basılmadan tüm 4 Page 6 parametresi anında ekrana basılır."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.secili_goz = 3
        self.hmi.current_service_page = 5
        self.hmi.aktif_sayfa = 5

        self.hmi.edit_service_sweep[3]["span_khz"] = 3
        self.hmi.edit_service_sweep[3]["period_ms"] = 500
        self.hmi.edit_service_sweep[3]["step_increment"] = 5

        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("NAV_FORWARD")

        self.assertIn('t_swp_goz.txt="3"', self.hmi.nextion_tx_log)
        self.assertIn('t_swp_span.txt="3"', self.hmi.nextion_tx_log)
        self.assertIn('t_swp_per.txt="500"', self.hmi.nextion_tx_log)
        self.assertIn('t_swp_step.txt="5"', self.hmi.nextion_tx_log)

    def test_srv_sync_03_page6_srv_tank_up_updates_t_swp_goz(self):
        """TEST-03: Page 6'da SRV_TANK_UP basıldığında secili_goz artar ve t_swp_goz.txt anında güncellenir."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.edit_max_goz_sayisi = 4
        self.hmi.secili_goz = 1
        self.hmi.current_service_page = 6
        self.hmi.aktif_sayfa = 6

        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("SRV_TANK_UP")

        self.assertEqual(self.hmi.secili_goz, 2)
        self.assertEqual(self.hmi.current_service_page, 6)
        self.assertIn('t_swp_goz.txt="2"', self.hmi.nextion_tx_log)

        # Wrap around from 4 to 1
        self.hmi.secili_goz = 4
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("SRV_TANK_UP")
        self.assertEqual(self.hmi.secili_goz, 1)
        self.assertIn('t_swp_goz.txt="1"', self.hmi.nextion_tx_log)

    def test_srv_sync_04_page6_srv_tank_down_updates_t_swp_goz(self):
        """TEST-04: Page 6'da SRV_TANK_DOWN basıldığında secili_goz azalır ve t_swp_goz.txt anında güncellenir."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.edit_max_goz_sayisi = 4
        self.hmi.secili_goz = 3
        self.hmi.current_service_page = 6
        self.hmi.aktif_sayfa = 6

        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("SRV_TANK_DOWN")

        self.assertEqual(self.hmi.secili_goz, 2)
        self.assertEqual(self.hmi.current_service_page, 6)
        self.assertIn('t_swp_goz.txt="2"', self.hmi.nextion_tx_log)

        # Wrap around from 1 to max (4)
        self.hmi.secili_goz = 1
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("SRV_TANK_DOWN")
        self.assertEqual(self.hmi.secili_goz, 4)
        self.assertIn('t_swp_goz.txt="4"', self.hmi.nextion_tx_log)

    def test_srv_sync_05_page6_parameter_commands_preserve_state(self):
        """TEST-05: Page 6 açıkken gelen parametre komutlarının Page 6 state'ini ve seçili tankı bozmadığı doğrulanır."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.secili_goz = 2
        self.hmi.current_service_page = 6
        self.hmi.aktif_sayfa = 6
        self.hmi.edit_service_sweep[2]["span_khz"] = 2

        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("SRV_SPAN_UP")

        self.assertEqual(self.hmi.current_service_page, 6)
        self.assertEqual(self.hmi.aktif_sayfa, 6)
        self.assertEqual(self.hmi.secili_goz, 2)
        self.assertEqual(self.hmi.edit_service_sweep[2]["span_khz"], 3)
        self.assertIn('t_swp_span.txt="3"', self.hmi.nextion_tx_log)
        self.assertIn('t_swp_goz.txt="2"', self.hmi.nextion_tx_log)

    def test_srv_sync_06_page6_nav_forward_to_page7_degas(self):
        """TEST-06: Page 6'dan NAV_FORWARD ile Page 7'ye geçildiğinde seçili tank korunur ve DEGAS parametreleri yüklenir."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.secili_goz = 3
        self.hmi.current_service_page = 6
        self.hmi.aktif_sayfa = 6

        self.hmi.edit_service_degas[3]["duration_minutes"] = 25
        self.hmi.edit_service_degas[3]["power_pct"] = 80
        self.hmi.edit_service_degas[3]["frequency_khz"] = 35

        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("NAV_FORWARD")

        self.assertEqual(self.hmi.current_service_page, 7)
        self.assertEqual(self.hmi.aktif_sayfa, 7)
        self.assertEqual(self.hmi.secili_goz, 3)
        self.assertIn("page page7", self.hmi.nextion_tx_log)
        self.assertIn('t_deg_goz.txt="3"', self.hmi.nextion_tx_log)
        self.assertIn('t_deg_dur.txt="25"', self.hmi.nextion_tx_log)
        self.assertIn('t_deg_pow.txt="80"', self.hmi.nextion_tx_log)
        self.assertIn('t_deg_freq.txt="35"', self.hmi.nextion_tx_log)

    def test_srv_sync_07_page7_nav_forward_to_page8_pulse_temp(self):
        """TEST-07: Page 7'den NAV_FORWARD ile Page 8'e geçildiğinde seçili tank korunur ve darbe/sıcaklık parametreleri yüklenir."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.secili_goz = 3
        self.hmi.current_service_page = 7
        self.hmi.aktif_sayfa = 7

        self.hmi.edit_service_degas[3]["pulse_on_ms"] = 1200
        self.hmi.edit_service_degas[3]["pulse_off_ms"] = 600
        self.hmi.edit_service_degas[3]["temp_ctrl"] = 1
        self.hmi.edit_service_degas[3]["target_temp_c"] = 65.0

        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("NAV_FORWARD")

        self.assertEqual(self.hmi.current_service_page, 8)
        self.assertEqual(self.hmi.aktif_sayfa, 8)
        self.assertEqual(self.hmi.secili_goz, 3)
        self.assertIn("page page8", self.hmi.nextion_tx_log)
        self.assertIn('t_deg_pon.txt="1200"', self.hmi.nextion_tx_log)
        self.assertIn('t_deg_poff.txt="600"', self.hmi.nextion_tx_log)
        self.assertIn('t_deg_tctrl.txt="ACIK"', self.hmi.nextion_tx_log)
        self.assertIn('t_deg_temp.txt="65"', self.hmi.nextion_tx_log)

    def test_srv_sync_08_page8_nav_forward_to_page5_cycle(self):
        """TEST-08: Page 8'den NAV_FORWARD ile Page 5'e dönüldüğünde seçili tank ve Page 5 parametreleri eksiksiz geri gelir."""
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = self.hmi.millis_ms
        self.hmi.secili_goz = 3
        self.hmi.current_service_page = 8
        self.hmi.aktif_sayfa = 8

        self.hmi.edit_guc_seviyesi[3] = 90
        self.hmi.edit_kart_id[3] = 3
        self.hmi.edit_max_goz_sayisi = 4

        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("NAV_FORWARD")

        self.assertEqual(self.hmi.current_service_page, 5)
        self.assertEqual(self.hmi.aktif_sayfa, 5)
        self.assertEqual(self.hmi.secili_goz, 3)
        self.assertIn("page page5", self.hmi.nextion_tx_log)
        self.assertIn('t_srv_goz.txt="3"', self.hmi.nextion_tx_log)
        self.assertIn('t_srv_guc.txt="90"', self.hmi.nextion_tx_log)
        self.assertIn('t_srv_id.txt="3"', self.hmi.nextion_tx_log)
        self.assertIn('t_srv_max.txt="4"', self.hmi.nextion_tx_log)


class TestCardIdRuntimeMapping(unittest.TestCase):
    """TEST-01 to TEST-05: Real STM32 Card ID runtime mapping after provisioning/swap."""

    def setUp(self):
        self.hmi = MockESP32HMI()
        self.hmi.nvsYukle()
        self.hmi.millis_ms = 5000

    def test_01_goz1_id1_start_targets_T1(self):
        """TEST-01: Göz 1 -> ID 1: START sends commands with prefix T1:."""
        self.hmi.kart_id[1] = 1
        self.hmi.stm_son_veri_zamani[1] = 5000 # Göz 1 is online
        self.hmi.secili_goz = 1
        self.hmi.hedef_sure[1] = 15
        self.hmi.hedef_sicaklik[1] = 40
        self.hmi.stm32_tx_log.clear()

        self.hmi.komutIsle("CMD_START|15|40")
        self.assertTrue(self.hmi.makine_calisiyor[1])
        self.assertIn("T1:START", self.hmi.stm32_tx_log)
        self.assertIn("T1:SET_TIME:15", self.hmi.stm32_tx_log)
        self.assertIn("T1:SET_TEMP:40", self.hmi.stm32_tx_log)

    def test_02_goz1_id2_saved_start_targets_T2(self):
        """TEST-02: Göz 1 -> ID 2 (SAVED): Göz 1 online (Card 2 online) -> START sends T2:."""
        self.hmi.kart_id[1] = 2
        self.hmi.kart_id[2] = 1
        self.hmi.stm_son_veri_zamani[1] = 5000 # Göz 1 (Card 2) is online
        self.hmi.secili_goz = 1
        self.hmi.hedef_sure[1] = 10
        self.hmi.stm32_tx_log.clear()

        self.hmi.komutIsle("CMD_START|10|50")
        self.assertTrue(self.hmi.makine_calisiyor[1])
        self.assertIn("T2:START", self.hmi.stm32_tx_log)
        self.assertIn("T2:SET_TIME:10", self.hmi.stm32_tx_log)

    def test_03_goz1_id2_saved_card2_offline_blocks_start(self):
        """TEST-03: Göz 1 -> ID 2 (SAVED): Card 2 is offline -> START is blocked with Kart Yok!."""
        self.hmi.kart_id[1] = 2
        self.hmi.kart_id[2] = 1
        self.hmi.stm_son_veri_zamani[1] = 0 # Göz 1 (Card 2) has NO telemetry -> OFFLINE
        self.hmi.secili_goz = 1
        self.hmi.hedef_sure[1] = 10
        self.hmi.stm32_tx_log.clear()

        self.hmi.komutIsle("CMD_START|10|50")
        self.assertFalse(self.hmi.makine_calisiyor[1])
        self.assertEqual(self.hmi.durum_metni[1], "Kart Yok!")
        self.assertNotIn("T2:START", self.hmi.stm32_tx_log)
        self.assertNotIn("T1:START", self.hmi.stm32_tx_log)

    def test_04_goz2_id1_saved_start_targets_T1(self):
        """TEST-04: Göz 1 -> ID 2, Göz 2 -> ID 1: Göz 2 selected -> START sends T1:."""
        self.hmi.kart_id[1] = 2
        self.hmi.kart_id[2] = 1
        self.hmi.stm_son_veri_zamani[2] = 5000 # Göz 2 (Card 1) is online
        self.hmi.secili_goz = 2
        self.hmi.hedef_sure[2] = 20
        self.hmi.hedef_sicaklik[2] = 55
        self.hmi.stm32_tx_log.clear()

        self.hmi.komutIsle("CMD_START|20|55")
        self.assertTrue(self.hmi.makine_calisiyor[2])
        self.assertIn("T1:START", self.hmi.stm32_tx_log)
        self.assertIn("T1:SET_TIME:20", self.hmi.stm32_tx_log)
        self.assertIn("T1:SET_TEMP:55", self.hmi.stm32_tx_log)

    def test_05_all_commands_use_real_card_id_mapping(self):
        """TEST-05: STOP, SET_TIME, SET_TEMP, SET_POWER, SWEEP all use kart_id[secili_goz]."""
        self.hmi.kart_id[3] = 7
        self.hmi.stm_son_veri_zamani[3] = 5000
        self.hmi.secili_goz = 3
        self.hmi.stm32_tx_log.clear()

        self.hmi.stmGonder("SET_TIME:25\n")
        self.hmi.stmGonder("SET_TEMP:60\n")
        self.hmi.stmGonder("SET_POWER:80\n")
        self.hmi.stmGonder("SWEEP:ON\n")
        self.hmi.komutIsle("CMD_STOP")

        self.assertIn("T7:SET_TIME:25", self.hmi.stm32_tx_log)
        self.assertIn("T7:SET_TEMP:60", self.hmi.stm32_tx_log)
        self.assertIn("T7:SET_POWER:80", self.hmi.stm32_tx_log)
        self.assertIn("T7:SWEEP:ON", self.hmi.stm32_tx_log)
        self.assertIn("T7:STOP", self.hmi.stm32_tx_log)

    def test_bug_a_freq_toggle_preserves_program_selection(self):
        """BUG-A: Frequency toggle must NOT cancel program selection (P1/P2/P3/FP remain active)."""
        self.hmi.secili_goz = 1
        self.hmi.stm_son_veri_zamani[1] = 5000 # Node 1 online
        self.hmi.komutIsle("P1_SEL") # Loads P1 (15 dk / 40 C)
        self.assertEqual(self.hmi.aktif_program, 1)
        self.assertEqual(self.hmi.hedef_sure[1], 15)
        self.assertEqual(self.hmi.hedef_sicaklik[1], 40)

        # Clear TX logs
        self.hmi.nextion_tx_log.clear()

        # User toggles frequency (e.g. 28k -> 40k)
        self.hmi.komutIsle("CMD_FREQ_TOGGLE")
        self.assertEqual(self.hmi.stm_freq[1], 40)
        self.assertEqual(self.hmi.aktif_program, 1, "Program selection MUST be preserved on frequency toggle")
        self.assertIn('b_freq.txt="40k"', self.hmi.nextion_tx_log)
        self.assertIn('b_p1.bco=2016', self.hmi.nextion_tx_log) # P1 remains green

        # Toggle back to 28k
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("CMD_FREQ_TOGGLE")
        self.assertEqual(self.hmi.stm_freq[1], 28)
        self.assertEqual(self.hmi.aktif_program, 1)
        self.assertIn('b_freq.txt="28k"', self.hmi.nextion_tx_log)
        self.assertIn('b_p1.bco=2016', self.hmi.nextion_tx_log)

    def test_bug_b_manual_mode_preserves_frequency(self):
        """BUG-B: Manual mode entry (touching time/temp keypad) must preserve displayed frequency."""
        self.hmi.secili_goz = 1
        self.hmi.stm_son_veri_zamani[1] = 5000
        self.hmi.stm_freq[1] = 40 # 40 kHz active

        # User enters manual mode via keypad touch
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("MANUAL_MODE")
        self.assertEqual(self.hmi.aktif_program, 0)
        self.assertEqual(self.hmi.stm_freq[1], 40)

        # Ensure b_freq.txt="40k" is sent to Nextion to prevent keypad revert to static "28k"
        self.assertIn('b_freq.txt="40k"', self.hmi.nextion_tx_log)
        self.assertIn('b_frq.txt="40k"', self.hmi.nextion_tx_log)

        # START with manual values works seamlessly
        self.hmi.stm32_tx_log.clear()
        self.hmi.komutIsle("CMD_START|25|50")
        self.assertTrue(self.hmi.makine_calisiyor[1])
        self.assertEqual(self.hmi.hedef_sure[1], 25)
        self.assertEqual(self.hmi.hedef_sicaklik[1], 50)
        self.assertIn("T1:SET_TIME:25", self.hmi.stm32_tx_log)
        self.assertIn("T1:SET_TEMP:50", self.hmi.stm32_tx_log)
        self.assertIn("T1:SET_FREQ:40", self.hmi.stm32_tx_log)
        self.assertIn("T1:START", self.hmi.stm32_tx_log)

    def test_bug01_degas_stop_preserves_armed_selection(self):
        """BUG-01: DEGAS -> START -> STOP -> verify degas_armed remains True and next START restarts DEGAS."""
        self.hmi.secili_goz = 1
        self.hmi.stm_son_veri_zamani[1] = 5000
        
        # 1. Arm DEGAS
        self.hmi.komutIsle("CMD_DEGAS_SEL")
        self.assertTrue(self.hmi.degas_armed[1])
        self.assertFalse(self.hmi.degas_active[1])
        
        # 2. START DEGAS
        self.hmi.stm32_tx_log.clear()
        self.hmi.komutIsle("CMD_START")
        self.assertTrue(self.hmi.degas_active[1])
        self.assertTrue(self.hmi.degas_armed[1])
        self.assertIn("T1:START_DEGAS:15:100:28:1000:500:0:50.0", self.hmi.stm32_tx_log)
        
        # 3. STOP DEGAS
        self.hmi.stm32_tx_log.clear()
        self.hmi.komutIsle("CMD_STOP")
        self.assertFalse(self.hmi.degas_active[1])
        self.assertTrue(self.hmi.degas_armed[1])  # CRITICAL INVARIANT: degas_armed preserved!
        self.assertIn("T1:STOP", self.hmi.stm32_tx_log)
        
        # 4. Telemetry reports IDLE from STM32
        self.hmi.stmTelemetryIsle("STAT,1,IDLE,0,250,0,50,28,0")
        self.assertFalse(self.hmi.degas_active[1])
        self.assertTrue(self.hmi.degas_armed[1])  # CRITICAL INVARIANT: telemetry does not clear selection!
        
        # 5. Next START restarts DEGAS seamlessly without needing to re-arm
        self.hmi.stm32_tx_log.clear()
        self.hmi.komutIsle("CMD_START")
        self.assertTrue(self.hmi.degas_active[1])
        self.assertTrue(self.hmi.degas_armed[1])
        self.assertIn("T1:START_DEGAS:15:100:28:1000:500:0:50.0", self.hmi.stm32_tx_log)

    def test_bug02_frequency_preservation_across_manual_edit_and_cycle(self):
        """BUG-02: 40k -> manual TIME/TEMP edit -> verify 40k -> START -> STOP -> verify 40k."""
        self.hmi.secili_goz = 1
        self.hmi.stm_son_veri_zamani[1] = 5000
        
        # 1. Select 40 kHz
        self.hmi.komutIsle("CMD_FREQ|40")
        self.assertEqual(self.hmi.stm_freq[1], 40)
        
        # 2. Manual TIME/TEMP edit (simulating keypad touch)
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("MANUAL_MODE")
        self.hmi.komutIsle("SET_TIME:25")
        self.hmi.komutIsle("SET_TEMP:45")
        self.assertEqual(self.hmi.stm_freq[1], 40)
        self.assertIn('b_freq.txt="40k"', self.hmi.nextion_tx_log)
        
        # 3. START
        self.hmi.stm32_tx_log.clear()
        self.hmi.komutIsle("CMD_START|25|45")
        self.assertTrue(self.hmi.makine_calisiyor[1])
        self.assertIn("T1:SET_FREQ:40", self.hmi.stm32_tx_log)
        
        # 4. STOP -> verify UI and state retain 40k
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("CMD_STOP")
        self.assertEqual(self.hmi.stm_freq[1], 40)
        self.assertIn('b_freq.txt="40k"', self.hmi.nextion_tx_log)

    def test_bug03_sweep_preservation_across_manual_edit_and_cycle(self):
        """BUG-03: SWEEP ON -> manual TIME/TEMP edit -> verify GREEN -> START -> STOP -> verify GREEN."""
        self.hmi.secili_goz = 1
        self.hmi.stm_son_veri_zamani[1] = 5000
        
        # 1. Enable SWEEP
        self.hmi.komutIsle("CMD_SWEEP_ON")
        self.assertTrue(self.hmi.runtime_sweep[1])
        
        # 2. Manual TIME/TEMP edit
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("MANUAL_MODE")
        self.hmi.komutIsle("SET_TIME:30")
        self.assertTrue(self.hmi.runtime_sweep[1])
        self.assertIn("b_sweep.bco=2016", self.hmi.nextion_tx_log)
        
        # 3. START -> verify SWEEP:ON is transmitted
        self.hmi.stm32_tx_log.clear()
        self.hmi.komutIsle("CMD_START|30|50")
        self.assertTrue(self.hmi.makine_calisiyor[1])
        self.assertIn("T1:SWEEP:ON", self.hmi.stm32_tx_log)
        
        # 4. STOP -> verify SWEEP button stays GREEN (2016)
        self.hmi.nextion_tx_log.clear()
        self.hmi.komutIsle("CMD_STOP")
        self.assertTrue(self.hmi.runtime_sweep[1])
        self.assertIn("b_sweep.bco=2016", self.hmi.nextion_tx_log)

    def test_srv_save_sweep_params_synchronized_to_stm32(self):
        """Verify SRV_SAVE 3/600/4 synchronizes to STM32 immediately and START preserves them."""
        self.hmi.secili_goz = 1
        self.hmi.stm_son_veri_zamani[1] = 5000
        self.hmi.g_service_authenticated = True
        self.hmi.service_auth_time = 5000
        
        # 1. Open Page 6 and edit Span=3, Period=600, Step=4
        self.hmi.komutIsle("PAGE6_OPEN")
        self.hmi.komutIsle("SET_SWP_SPAN:3")
        self.hmi.komutIsle("SET_SWP_PER:600")
        self.hmi.komutIsle("SET_STEP_INC:4")
        
        # 2. SRV_SAVE -> must immediately synchronize to STM32
        self.hmi.stm32_tx_log.clear()
        self.hmi.komutIsle("SRV_SAVE")
        
        # Verify NVS / runtime state
        self.assertEqual(self.hmi.service_sweep[1]["span_khz"], 3)
        self.assertEqual(self.hmi.service_sweep[1]["period_ms"], 600)
        self.assertEqual(self.hmi.service_sweep[1]["step_increment"], 4)
        
        # Verify immediate RS485 transmission to STM32
        self.assertIn("T1:SET_SWP_SPAN:3", self.hmi.stm32_tx_log)
        self.assertIn("T1:SET_SWP_PER:600", self.hmi.stm32_tx_log)
        self.assertIn("T1:SET_STEP_INC:4", self.hmi.stm32_tx_log)
        
        # 3. Return to Page 0, Enable SWEEP, and START
        self.hmi.komutIsle("CMD_SWEEP_ON")
        self.hmi.stm32_tx_log.clear()
        self.hmi.komutIsle("CMD_START|10|60")
        
        # Verify START payload also transmits the exact 3/600/4 sweep parameters
        self.assertIn("T1:SET_SWP_SPAN:3", self.hmi.stm32_tx_log)
        self.assertIn("T1:SET_SWP_PER:600", self.hmi.stm32_tx_log)
        self.assertIn("T1:SET_STEP_INC:4", self.hmi.stm32_tx_log)
        self.assertIn("T1:SWEEP:ON", self.hmi.stm32_tx_log)
        self.assertIn("T1:START", self.hmi.stm32_tx_log)


if __name__ == "__main__":
    unittest.main()
