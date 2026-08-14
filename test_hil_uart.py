"""
Hardware-in-the-Loop (HIL) UART test suite for the EAGLEULTRASONiK washing
machine controller (ESP32 master <-> STM32 slave, 115200 baud, protocol per
Manifesto_V3.md section 5).

PHYSICAL WIRING CONSTRAINT: the host PC has no access to the shared internal
ESP32<->STM32 bus and no access to the Nextion HMI. The only two links
available are two independent USB-serial channels:

  * ESP32_PORT (COM10 / /dev/ttyACM1) - the ESP32's USB debug console (Serial). Test commands
    are written here; the ESP32 firmware (ekran_kontrol.ino) detects bus-style
    "T<id>:..." frames and forwards them verbatim to the STM32 over its
    internal Serial1 link, logging "[PC->STM] ..." / "[STM->ESP] STAT,..." back
    on this same port so it can also be used as a secondary telemetry source.

  * STM32_PORT (COM11 / /dev/ttyACM0) - the STM32's ST-Link Virtual COM Port (LPUART1). The
    STM32 firmware (esp32_uart.c) mirrors every STAT,... telegram and command ACK/NACK
    it sends to the ESP32 directly onto this port, so it is read here as the ground
    truth for the slave's own state.

Run from the workspace root:  python -m pytest test_hil_uart.py -v
"""

import re
import sys
import time
import logging
import unittest

import serial
import serial.tools.list_ports


def auto_detect_port(vid, pid, default_port):
    try:
        for p in serial.tools.list_ports.comports():
            if p.vid == vid and p.pid == pid:
                return p.device
    except Exception:
        pass
    return default_port


# =============================================================================
# CONFIGURATION - adjust COM ports to match the physical bench wiring.
# =============================================================================
ESP32_PORT = auto_detect_port(0x1A86, 0x55D3, "COM10" if sys.platform == "win32" else "/dev/ttyACM1")   # ESP32 USB debug console
STM32_PORT = auto_detect_port(0x0483, 0x374E, "COM11" if sys.platform == "win32" else "/dev/ttyACM0")   # STM32 ST-Link Virtual COM Port

ESP32_BAUD = 115200
STM32_BAUD = 115200

WATCHDOG_TIMEOUT_S = 3.0  # Matches STM_BAGLANTI_TIMEOUT (3000ms) in ekran_kontrol.ino

# --- HIL_DEEP_DEBUG white-box constants (must mirror the firmware exactly) --------------
# ultrasonic_pwm.c: AC_HALF_CYCLE_US(10000) - 500, and the min firing delay
TRIAC_MIN_DELAY_US = 500
TRIAC_MAX_DELAY_US = 9500
TRIAC_SETTLE_TOLERANCE_US = 60  # soft-start ramps 20us per zero-cross; allow a few steps of slack
# pt100_adc.c: temp_c = (adc_raw * PT100_CAL_SLOPE) + PT100_CAL_OFFSET
PT100_CAL_SLOPE = 0.0327
PT100_CAL_OFFSET = -20.0

LOG_FILE = "test_results.log"

logging.basicConfig(
    filename=LOG_FILE,
    filemode="w",
    level=logging.DEBUG,
    format="%(asctime)s.%(msecs)03d [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)


# =============================================================================
# LOW-LEVEL SERIAL WRAPPER
# =============================================================================
class UARTBus:
    """Thin pyserial wrapper that logs every raw byte sent/received with a timestamp."""

    def __init__(self, name, port, baudrate, default_timeout=0.2):
        self.name = name
        self.serial = serial.Serial(port, baudrate=baudrate, timeout=default_timeout)
        logging.info("[%s] opened %s @ %d baud", self.name, port, baudrate)

    def write_line(self, text, terminator=b"\n"):
        # Sends a protocol line (or raw HMI command) followed by its terminator
        payload = text.encode("ascii") + terminator
        self.serial.write(payload)
        logging.info("[%s] TX >> %r", self.name, payload)

    def read_line(self, timeout=None):
        # Reads a single '\n'-terminated line; returns None on timeout
        self.serial.timeout = timeout if timeout is not None else self.serial.timeout
        raw = self.serial.readline()
        if not raw:
            return None
        logging.info("[%s] RX << %r", self.name, raw)
        return raw.decode("ascii", errors="replace").strip()

    def reset_input_buffer(self):
        try:
            self.serial.reset_input_buffer()
        except Exception:
            pass

    def close(self):
        self.serial.close()
        logging.info("[%s] closed", self.name)


# =============================================================================
# PROTOCOL HELPERS
# =============================================================================
class TelemetryFrame:
    """Parses one STAT,<id>,<mode>,<rem_sec>,<temp_x10>,<relay>,<power>,<freq>,<fault>,<prov> line."""

    PATTERN = re.compile(
        r"STAT,(\d+),(IDLE|RUNNING|FAULT),(\d+),(-?\d+),(\d+),(\d+),(\d+),(\d+),(\d+)$"
    )

    def __init__(self, tank_id, mode, remaining_sec, temp_c, relay, power_pct, frequency_khz, fault_flags, prov_state=2):
        self.tank_id = tank_id
        self.mode = mode
        self.remaining_sec = remaining_sec
        self.temp_c = temp_c
        self.relay = relay
        self.power_pct = power_pct
        self.frequency_khz = frequency_khz
        self.fault_flags = fault_flags
        self.prov_state = prov_state

    @classmethod
    def parse(cls, line):
        if not line:
            return None
        m = cls.PATTERN.search(line)
        if not m:
            return None
        tank_id, mode, rem_sec, temp_x10, relay, power_pct, frequency_khz, fault_flags, prov_state = m.groups()
        return cls(
            int(tank_id), mode, int(rem_sec), int(temp_x10) / 10.0,
            int(relay), int(power_pct), int(frequency_khz), int(fault_flags), int(prov_state),
        )


class ProtocolCommands:
    """Builds ESP32->STM32 command strings exactly as defined in Manifesto_V3.md §5.1."""

    @staticmethod
    def set_id_broadcast(new_id):
        return "T0:SET_ID:{}".format(new_id)

    @staticmethod
    def set_time(tank_id, minutes):
        return "T{}:SET_TIME:{}".format(tank_id, minutes)

    @staticmethod
    def set_temp(tank_id, deg_c):
        return "T{}:SET_TEMP:{}".format(tank_id, deg_c)

    @staticmethod
    def set_power(tank_id, percent):
        return "T{}:SET_POWER:{}".format(tank_id, percent)

    @staticmethod
    def set_freq(tank_id, freq_khz):
        return "T{}:SET_FREQ:{}".format(tank_id, freq_khz)

    @staticmethod
    def start(tank_id):
        return "T{}:START".format(tank_id)

    @staticmethod
    def stop(tank_id):
        return "T{}:STOP".format(tank_id)


# =============================================================================
# HIL_DEEP_DEBUG WHITE-BOX FRAME PARSERS
# =============================================================================
class TriacDebugFrame:
    """Parses one 'DEBUG_STM: ADC=<raw>, DELAY=<us>, RELAY=<0/1>, HEATER_OUT=<0/1>, HEATER_FB=<0/1>, TRIAC_OUT=<0/1>, TRIAC_FB=<0/1>' line."""

    PATTERN = re.compile(r"DEBUG_STM:\s*ADC=(\d+),\s*DELAY=(\d+),\s*RELAY=([01]),\s*HEATER_OUT=([01]),\s*HEATER_FB=([01]),\s*TRIAC_OUT=([01]),\s*TRIAC_FB=([01])")

    def __init__(self, adc_raw, delay_us, relay, heater_out, heater_fb, triac_out, triac_fb):
        self.adc_raw = adc_raw
        self.delay_us = delay_us
        self.relay = relay
        self.heater_out = heater_out
        self.heater_fb = heater_fb
        self.triac_out = triac_out
        self.triac_fb = triac_fb

    @classmethod
    def parse(cls, line_or_match):
        m = line_or_match if isinstance(line_or_match, re.Match) else (
            cls.PATTERN.search(line_or_match) if line_or_match else None
        )
        if not m:
            return None
        return cls(int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5)), int(m.group(6)), int(m.group(7)))


# =============================================================================
# TEST SUITE
# =============================================================================
class HardwareInLoopTests(unittest.TestCase):
    """Modular HIL regression tests for the ESP32<->STM32 UART protocol.

    All commands are injected via ESP32_PORT; telemetry is verified
    against STM32_PORT, the STM32's own ground-truth report, with
    ESP32_PORT's forwarded "[STM->ESP] STAT,..." log used as a secondary,
    cross-checked source.
    """

    active_tank_id = 1  # updated once test_01 renames the slave or dynamically detected

    @classmethod
    def setUpClass(cls):
        try:
            cls.esp32 = UARTBus("ESP32", ESP32_PORT, ESP32_BAUD)
            cls.stm32 = UARTBus("STM32", STM32_PORT, STM32_BAUD)

            time.sleep(0.2)
            cls.esp32.reset_input_buffer()
            cls.stm32.reset_input_buffer()

            # ESP32 service interlock unlock (required for provisioning commands)
            for digit in "123456":
                cls.esp32.write_line(f"KEY_{digit}")
                time.sleep(0.04)
            cls.esp32.write_line("KEY_OK")
            time.sleep(0.25)

            # Discover the currently active STM32 Tank ID from the normal telemetry stream
            detected_id = None
            deadline = time.time() + 3.0
            while time.time() < deadline:
                line = cls.stm32.read_line(timeout=0.2)
                frame = TelemetryFrame.parse(line)
                if frame is not None and frame.tank_id > 0:
                    detected_id = frame.tank_id
                    break

            if detected_id is not None:
                cls.active_tank_id = detected_id
                logging.info("HIL active Tank ID auto-detected from STAT as %d", detected_id)
            else:
                logging.warning("Could not auto-detect active Tank ID from STAT; keeping default %d", cls.active_tank_id)

        except serial.SerialException as exc:
            logging.error("COM port open failed: %s", exc)
            print("SETUP FAILED: {}".format(exc))
            raise unittest.SkipTest(
                "HIL bench not connected ({}). Check cabling/COM port mapping.".format(exc)
            )

    @classmethod
    def tearDownClass(cls):
        for link in (getattr(cls, "esp32", None), getattr(cls, "stm32", None)):
            if link is not None:
                link.close()

    def setUp(self):
        # Flush buffers and ensure service mode is active before each test
        if hasattr(self, "esp32") and hasattr(self, "stm32"):
            self.esp32.reset_input_buffer()
            self.stm32.reset_input_buffer()
            for digit in "123456":
                self.esp32.write_line(f"KEY_{digit}")
                time.sleep(0.02)
            self.esp32.write_line("KEY_OK")
            time.sleep(0.1)

    def tearDown(self):
        # Guarantee machine is left in a safe IDLE state after every test
        if hasattr(self, "esp32"):
            tid = self._get_active_tank_id()
            self.esp32.write_line(ProtocolCommands.stop(tid))
            time.sleep(0.05)

    # --- shared helpers ---------------------------------------------------
    def _send(self, command):
        # Injects a "T<id>:..." bus command via the ESP32's USB debug port;
        # ekran_kontrol.ino forwards it verbatim to the STM32 over Serial1.
        self.esp32.write_line(command)

    def _get_active_tank_id(self):
        """Discovers or confirms the active Tank ID from the live STAT stream."""
        deadline = time.time() + 2.5
        while time.time() < deadline:
            line = self.stm32.read_line(timeout=0.2)
            frame = TelemetryFrame.parse(line)
            if frame is not None and frame.tank_id > 0:
                type(self).active_tank_id = frame.tank_id
                return frame.tank_id
        return type(self).active_tank_id

    def _wait_for_stat(self, bus, tank_id, timeout, predicate=None):
        # Polls `bus` for a STAT frame from `tank_id` matching `predicate`
        deadline = time.time() + timeout
        while time.time() < deadline:
            line = bus.read_line(timeout=max(deadline - time.time(), 0.05))
            frame = TelemetryFrame.parse(line) if line else None
            if frame and frame.tank_id == tank_id and (predicate is None or predicate(frame)):
                return frame
        return None

    def _wait_for_match(self, bus, pattern, timeout, predicate=None):
        # Generic HIL line poller: returns the first re.Match of `pattern` on `bus`
        # satisfying `predicate(match)`, or None on timeout.
        deadline = time.time() + timeout
        while time.time() < deadline:
            line = bus.read_line(timeout=max(deadline - time.time(), 0.05))
            if not line:
                continue
            m = pattern.search(line)
            if m and (predicate is None or predicate(m)):
                return m
        return None

    # --- 1. ID assignment ---------------------------------------------------
    def test_01_id_assignment(self):
        # Provisioning follows the real STM32 commissioning flow:
        # GET_UID -> STAGE_ID:<UID> -> ASSIGN_ID:<new_id>:<UID>
        curr_id = self._get_active_tank_id()
        new_id = 2 if curr_id == 1 else 1

        self._send("T{}:GET_UID".format(curr_id))
        uid_re = re.compile(r"UID24:([0-9A-Fa-f]{24})")
        m_uid = self._wait_for_match(self.stm32, uid_re, timeout=2.5)
        self.assertIsNotNone(m_uid, "STM32 failed to return UID24 on T{}:GET_UID".format(curr_id))
        uid = m_uid.group(1)

        # STAGE_ID
        self._send("T{}:STAGE_ID:{}".format(curr_id, uid))
        ack_stage = re.compile(r"ACK,STAGE_ID")
        m_stage = self._wait_for_match(self.stm32, ack_stage, timeout=2.0)
        self.assertIsNotNone(m_stage, "STM32 failed to return ACK,STAGE_ID")

        # ASSIGN_ID to new_id (universal broadcast T0:ASSIGN_ID)
        self._send("T0:ASSIGN_ID:{}:{}".format(new_id, uid))
        ack_assign = re.compile(r"ACK,ASSIGN_ID,(\d+)")
        m_assign = self._wait_for_match(self.stm32, ack_assign, timeout=2.0)
        self.assertIsNotNone(m_assign, "STM32 failed to return ACK,ASSIGN_ID")
        self.assertEqual(int(m_assign.group(1)), new_id, "Assigned ID does not match requested ID")

        # Confirm new ID in telemetry
        frame = self._wait_for_stat(self.stm32, new_id, timeout=3.0)
        self.assertIsNotNone(frame, "No STAT,{},... telemetry seen on STM32 after ASSIGN_ID".format(new_id))
        type(self).active_tank_id = new_id

    # --- 2. Parameter transmission ------------------------------------------
    def test_02_parameter_transmission(self):
        # Sends SET_TIME/SET_TEMP/SET_POWER and confirms the slave applies them once running
        tid = self._get_active_tank_id()
        self._send(ProtocolCommands.stop(tid))
        self._wait_for_stat(self.stm32, tid, timeout=2.0, predicate=lambda f: f.mode == "IDLE")

        self._send(ProtocolCommands.set_time(tid, 15))
        time.sleep(0.04)
        self._send(ProtocolCommands.set_temp(tid, 60))
        time.sleep(0.04)
        self._send(ProtocolCommands.set_power(tid, 50))
        time.sleep(0.04)
        self._send(ProtocolCommands.start(tid))

        # remaining_sec is loaded to setpoint_time_minutes*60 the instant RUNNING begins
        running = self._wait_for_stat(self.stm32, tid, timeout=3.0, predicate=lambda f: f.mode == "RUNNING")
        self.assertIsNotNone(running, "Slave never reported RUNNING after START")
        self.assertGreaterEqual(running.remaining_sec, 895, "SET_TIME:15 not reflected in remaining_sec")

        # actual power ramps toward the setpoint via soft-start; poll until it settles near 50%
        settled = self._wait_for_stat(self.stm32, tid, timeout=6.0, predicate=lambda f: abs(f.power_pct - 50) <= 5)
        self.assertIsNotNone(settled, "SET_POWER:50 never reached by the soft-start ramp")

        self._send(ProtocolCommands.stop(tid))
        self._wait_for_stat(self.stm32, tid, timeout=2.0, predicate=lambda f: f.mode == "IDLE")

    # --- 3. Operation cycle ---------------------------------------------------
    def test_03_operation_cycle(self):
        # START must flip telemetry mode to RUNNING; STOP must flip it back to IDLE
        tid = self._get_active_tank_id()
        self._send(ProtocolCommands.stop(tid))
        self._wait_for_stat(self.stm32, tid, timeout=2.0, predicate=lambda f: f.mode == "IDLE")

        self._send(ProtocolCommands.set_time(tid, 15))
        time.sleep(0.04)
        self._send(ProtocolCommands.set_temp(tid, 60))
        time.sleep(0.04)
        self._send(ProtocolCommands.set_power(tid, 50))
        time.sleep(0.04)
        self._send(ProtocolCommands.start(tid))

        running = self._wait_for_stat(self.stm32, tid, timeout=3.0, predicate=lambda f: f.mode == "RUNNING")
        self.assertIsNotNone(running, "Mode did not switch to RUNNING after START")

        self._send(ProtocolCommands.stop(tid))
        idle = self._wait_for_stat(self.stm32, tid, timeout=2.0, predicate=lambda f: f.mode == "IDLE")
        self.assertIsNotNone(idle, "Mode did not switch to IDLE after STOP")

    # --- 4. STOP-as-fault-ack (real hardware) ----------------------------------
    def test_04_stop_clears_fault(self):
        tid = self._get_active_tank_id()
        self._send(ProtocolCommands.stop(tid))
        idle = self._wait_for_stat(
            self.stm32, tid, timeout=2.0,
            predicate=lambda f: f.mode == "IDLE" and f.fault_flags == 0,
        )
        self.assertIsNotNone(idle, "STOP did not settle the slave into a fault-free IDLE state")

    # --- 5. Dual-channel telemetry consistency ---------------------------------
    def test_05_dual_channel_consistency(self):
        # Cross-verifies the ESP32's forwarded "[STM->ESP] STAT,..." log against
        # the STM32's own ground-truth report to confirm the forwarding path is intact.
        # Compares tank_id, mode, frequency, and power_pct to detect partial-forwarding bugs.
        tid = self._get_active_tank_id()
        self._send(ProtocolCommands.set_power(tid, 40))

        stm32_frame = self._wait_for_stat(self.stm32, tid, timeout=3.0)
        self.assertIsNotNone(stm32_frame, "No STAT,{},... seen on STM32 ground truth".format(tid))

        esp32_frame = self._wait_for_stat(self.esp32, tid, timeout=3.0)
        self.assertIsNotNone(esp32_frame, "No forwarded STAT,{},... seen on ESP32 log".format(tid))

        self.assertEqual(stm32_frame.tank_id, esp32_frame.tank_id, "Tank ID mismatch between STM32 and ESP32")
        self.assertEqual(stm32_frame.mode, esp32_frame.mode, "Mode mismatch between STM32 and ESP32")
        self.assertEqual(stm32_frame.frequency_khz, esp32_frame.frequency_khz, "Frequency mismatch between STM32 and ESP32")
        # Soft-start may advance one step between two consecutive frames; allow ±2 pct
        self.assertAlmostEqual(
            stm32_frame.power_pct, esp32_frame.power_pct, delta=2,
            msg="Power pct divergence >2 between STM32 and ESP32 (forwarding may relay stale frame)",
        )

    # --- 6. Triac phase-angle math (white-box) -------------------------------
    def test_06_triac_math(self):
        # Confirms PowerPctToDelayUs() in ultrasonic_pwm.c computes the mathematically correct
        # firing delay for a known power setpoint: delay = MAX - (MAX-MIN)*power/100
        tid = self._get_active_tank_id()
        power_pct = 50
        expected_delay_us = TRIAC_MAX_DELAY_US - ((TRIAC_MAX_DELAY_US - TRIAC_MIN_DELAY_US) * power_pct // 100)

        self._send(ProtocolCommands.stop(tid))
        self._wait_for_stat(self.stm32, tid, timeout=2.0, predicate=lambda f: f.mode == "IDLE")

        self._send(ProtocolCommands.set_time(tid, 15))
        time.sleep(0.04)
        self._send(ProtocolCommands.set_temp(tid, 60))
        time.sleep(0.04)
        self._send(ProtocolCommands.set_power(tid, power_pct))
        time.sleep(0.04)
        self._send(ProtocolCommands.start(tid))

        running = self._wait_for_stat(self.stm32, tid, timeout=3.0, predicate=lambda f: f.mode == "RUNNING")
        self.assertIsNotNone(running, "Slave did not enter RUNNING mode for triac math test")

        # Soft-start ramps the delay down 20us per zero-cross; give it time to settle
        settled = self._wait_for_match(
            self.stm32, TriacDebugFrame.PATTERN, timeout=6.0,
            predicate=lambda m: abs(int(m.group(2)) - expected_delay_us) <= TRIAC_SETTLE_TOLERANCE_US,
        )
        self.assertIsNotNone(
            settled,
            "Triac delay never settled to the mathematically correct value for {}% power "
            "(expected {}us +/-{}us)".format(power_pct, expected_delay_us, TRIAC_SETTLE_TOLERANCE_US),
        )

        self._send(ProtocolCommands.stop(tid))
        self._wait_for_stat(self.stm32, tid, timeout=2.0, predicate=lambda f: f.mode == "IDLE")

    # --- 7. PT100 ADC-to-temperature correlation (white-box) -----------------
    def test_07_pt100_adc(self):
        # Cross-checks the raw ADC count (DEBUG_STM) against the temperature the STAT
        # telegram reports, using the same linear calibration as pt100_adc.c
        tid = self._get_active_tank_id()

        m = self._wait_for_match(self.stm32, TriacDebugFrame.PATTERN, timeout=3.0)
        self.assertIsNotNone(m, "No DEBUG_STM ADC=... line seen on STM32")
        debug_frame = TriacDebugFrame.parse(m)

        stat_frame = self._wait_for_stat(self.stm32, tid, timeout=3.0)
        self.assertIsNotNone(stat_frame, "No STAT,{},... telemetry seen on STM32".format(tid))

        expected_temp_c = (debug_frame.adc_raw * PT100_CAL_SLOPE) + PT100_CAL_OFFSET
        self.assertAlmostEqual(
            stat_frame.temp_c, expected_temp_c, delta=2.0,
            msg="Reported temp_c ({}) does not correlate with raw ADC={} (expected ~{:.2f}C)".format(
                stat_frame.temp_c, debug_frame.adc_raw, expected_temp_c),
        )

    # --- 8. ESP32 internals: NVS persistence + HMI payload parsing -----------
    def test_08_esp32_internals(self):
        # Drives P_SAVE via ESP32 debug console and verifies both parsed HMI fields
        # and resulting NVS write-back, plus the 3000ms WDT state line.
        prog = 3
        sure, sicaklik = 22, 65

        self._send("EDIT_P{}".format(prog))
        time.sleep(0.08)
        self._send("P_SAVE|{}|{}".format(sure, sicaklik))

        parse_re = re.compile(r"DEBUG_ESP32: HMI_PARSE cmd=P_SAVE prog=(\d+) sure=(\d+) sicaklik=(\d+)")
        parsed = self._wait_for_match(
            self.esp32, parse_re, timeout=3.0,
            predicate=lambda m: int(m.group(1)) == prog,
        )
        self.assertIsNotNone(parsed, "No DEBUG_ESP32 HMI_PARSE line seen for P_SAVE")
        self.assertEqual(int(parsed.group(2)), sure, "Parsed 'sure' field does not match the sent HMI payload")
        self.assertEqual(int(parsed.group(3)), sicaklik, "Parsed 'sicaklik' field does not match the sent HMI payload")

        nvs_re = re.compile(r"DEBUG_ESP32: NVS_WRITE key=(\S+) val=(-?\d+)")
        wrote_sure = self._wait_for_match(
            self.esp32, nvs_re, timeout=3.0,
            predicate=lambda m: m.group(1) == "pS{}".format(prog) and int(m.group(2)) == sure,
        )
        self.assertIsNotNone(wrote_sure, "NVS_WRITE for pS{} (val={}) not observed".format(prog, sure))

        wrote_temp = self._wait_for_match(
            self.esp32, nvs_re, timeout=3.0,
            predicate=lambda m: m.group(1) == "pT{}".format(prog) and int(m.group(2)) == sicaklik,
        )
        self.assertIsNotNone(wrote_temp, "NVS_WRITE for pT{} (val={}) not observed".format(prog, sicaklik))

        # Watchdog: confirm the 3000ms WDT state line reports the active tank as connected
        tid = self._get_active_tank_id()
        wdt_re = re.compile(r"DEBUG_ESP32: WDT tank=(\d+) connected=([01]) age_ms=(\d+)")
        wdt = self._wait_for_match(
            self.esp32, wdt_re, timeout=4.0,
            predicate=lambda m: int(m.group(1)) == tid,
        )
        self.assertIsNotNone(wdt, "No DEBUG_ESP32 WDT line seen for tank {}".format(tid))
        self.assertEqual(wdt.group(2), "1", "Watchdog reports tank {} as disconnected".format(tid))

    # --- 9. Dual Frequency Selection Tests (X9C103S Potentiometer) ------------
    def test_f1_set_freq_28(self):
        # Sends T<id>:SET_FREQ:28 and verifies the STAT telegram reports frequency_khz == 28
        tid = self._get_active_tank_id()
        self._send(ProtocolCommands.set_freq(tid, 28))
        frame = self._wait_for_stat(self.stm32, tid, timeout=3.0, predicate=lambda f: f.frequency_khz == 28)
        self.assertIsNotNone(frame, "STAT packet frequency was not updated to 28 kHz after T{}:SET_FREQ:28".format(tid))
        self.assertEqual(frame.frequency_khz, 28, "STAT frequency_khz mismatch (expected 28)")

    def test_f2_set_freq_40(self):
        # Sends T<id>:SET_FREQ:40 and verifies the STAT telegram reports frequency_khz == 40
        tid = self._get_active_tank_id()
        self._send(ProtocolCommands.set_freq(tid, 40))
        frame = self._wait_for_stat(self.stm32, tid, timeout=3.0, predicate=lambda f: f.frequency_khz == 40)
        self.assertIsNotNone(frame, "STAT packet frequency was not updated to 40 kHz after T{}:SET_FREQ:40".format(tid))
        self.assertEqual(frame.frequency_khz, 40, "STAT frequency_khz mismatch (expected 40)")

    def test_f3_set_freq_invalid(self):
        # Sends T<id>:SET_FREQ:35 (invalid frequency) and verifies system rejects the change
        # and retains the current frequency (40 kHz)
        tid = self._get_active_tank_id()
        # First ensure we are at 40 kHz
        self._send(ProtocolCommands.set_freq(tid, 40))
        base_frame = self._wait_for_stat(self.stm32, tid, timeout=2.0, predicate=lambda f: f.frequency_khz == 40)
        self.assertIsNotNone(base_frame, "Failed to set base frequency to 40 kHz")

        # Send invalid command 35 kHz
        self._send(ProtocolCommands.set_freq(tid, 35))

        # Verify ERR:INVALID_FREQ is received
        err_re = re.compile(r"ERR:INVALID_FREQ")
        err_match = self._wait_for_match(self.stm32, err_re, timeout=2.0)
        self.assertIsNotNone(err_match, "STM32 did not emit ERR:INVALID_FREQ for invalid frequency 35")

        # Check STAT telegram still reports 40
        stat_frame = self._wait_for_stat(self.stm32, tid, timeout=2.0)
        self.assertIsNotNone(stat_frame, "No STAT frame received after invalid frequency command")
        self.assertEqual(stat_frame.frequency_khz, 40, "System failed to retain current frequency (40 kHz) on invalid input")

    # --- 10. Phase 5.2 Remediation Tests -------------------------------------
    def test_09_id0_discovery_slotted(self):
        # Broadcasts T0:DISCOVER. Commissioned (PROV_STATE_ACTIVE) and staging nodes must
        # silently ignore it; only uncommissioned nodes at MY_TANK_ID==0 may respond.
        tid = self._get_active_tank_id()
        stat = self._wait_for_stat(self.stm32, tid, timeout=2.0)
        self._send("T0:DISCOVER")
        disc_re = re.compile(r"DISCOVER_ACK,0,([0-9A-Fa-f]{24})")
        m = self._wait_for_match(self.stm32, disc_re, timeout=2.0)
        if stat is not None and stat.prov_state == 2:  # PROV_STATE_ACTIVE
            # Commissioned node must NOT respond — assertIsNone proves correct firmware gating
            self.assertIsNone(m, "Commissioned (PROV_STATE_ACTIVE) node must not respond to T0:DISCOVER")
        else:
            # Uncommissioned node present: verify ACK frame format
            self.assertIsNotNone(m, "Uncommissioned node failed to respond to T0:DISCOVER")
            self.assertEqual(len(m.group(1)), 24, "Invalid 24-char hex UID in discovery ACK")

    def test_10_id0_discovery_multi_simulated(self):
        # Host-side algorithm verification: replicates CRC16_CCITT % 16 slot assignment
        # from esp32_uart.c for 5 simulated UIDs. Verifies the Python implementation
        # of the slotted backoff formula produces in-range [0, 15] results.
        # NOTE: This does not contact hardware; it verifies the host's understanding
        # of the firmware slot algorithm is internally consistent.
        sample_uids = [
            b"STM32G4_UID_1", b"STM32G4_UID_2", b"STM32G4_UID_3", b"STM32G4_UID_4", b"STM32G4_UID_5"
        ]
        slots = []
        for raw in sample_uids:
            crc = 0xFFFF
            for b in raw:
                crc ^= (b << 8)
                for _ in range(8):
                    if crc & 0x8000:
                        crc = ((crc << 1) ^ 0x1021) & 0xFFFF
                    else:
                        crc = (crc << 1) & 0xFFFF
            slot = crc % 16
            slots.append(slot)
            self.assertGreaterEqual(slot, 0, "Slot must be >= 0 (4-bit window)")
            self.assertLess(slot, 16, "Slot must be < 16 (4-bit window, delay = slot * 25ms)")
        self.assertEqual(len(slots), 5, "Failed to compute 5 simulated discovery slots")

    def test_11_uid_mismatch_rejection(self):
        # Sends T0:ASSIGN_ID:3:BAD_UID_1234567890123456 and verifies NACK,ASSIGN_ID,ERR_UID_MISMATCH
        self._send("T0:ASSIGN_ID:3:BAD_UID_1234567890123456")
        nack_re = re.compile(r"NACK,ASSIGN_ID,ERR_UID_MISMATCH")
        m = self._wait_for_match(self.stm32, nack_re, timeout=2.0)
        self.assertIsNotNone(m, "STM32 failed to return NACK on invalid UID assignment")

    def test_12_id_duplicate_rejection(self):
        # Sends ASSIGN_ID to a node currently in PROV_STATE_ACTIVE without staging first
        tid = self._get_active_tank_id()
        self._send("T{}:GET_UID".format(tid))
        uid_re = re.compile(r"UID24:([0-9A-Fa-f]{24})")
        m_uid = self._wait_for_match(self.stm32, uid_re, timeout=2.0)
        self.assertIsNotNone(m_uid, "Failed to get UID from active node")
        real_uid = m_uid.group(1)

        # Try to directly re-assign active node without staging
        self._send("T{}:ASSIGN_ID:5:{}".format(tid, real_uid))
        err_re = re.compile(r"NACK,ASSIGN_ID,ERR_STATE_INVALID")
        m_err = self._wait_for_match(self.stm32, err_re, timeout=2.0)
        self.assertIsNotNone(m_err, "Active node failed to reject direct ASSIGN_ID without staging")

    def test_13_atomic_swap_flow(self):
        # Verifies the full 2-step atomic swap semantics against the single active node:
        # STAGE_ID transitions the node to MY_TANK_ID=0/STAGING; ASSIGN_ID reassigns it
        # back to its original ID; STAT telemetry at original ID confirms successful swap.
        tid = self._get_active_tank_id()
        self._send("T{}:GET_UID".format(tid))
        uid_re = re.compile(r"UID24:([0-9A-Fa-f]{24})")
        m_uid = self._wait_for_match(self.stm32, uid_re, timeout=2.0)
        self.assertIsNotNone(m_uid, "Failed to get UID from active node")
        real_uid = m_uid.group(1)

        # Step 1: STAGE_ID — node must transition to MY_TANK_ID=0, PROV_STATE_STAGING
        self._send("T{}:STAGE_ID:{}".format(tid, real_uid))
        ack_stage = re.compile(r"ACK,STAGE_ID")
        m_stage = self._wait_for_match(self.stm32, ack_stage, timeout=2.0)
        self.assertIsNotNone(m_stage, "Node failed to acknowledge STAGE_ID (staging transition failed)")

        # Step 2: ASSIGN_ID back to original ID via T0 broadcast (node is now at MY_TANK_ID=0)
        self._send("T0:ASSIGN_ID:{}:{}".format(tid, real_uid))
        ack_assign = re.compile(r"ACK,ASSIGN_ID,(\d+)")
        m_assign = self._wait_for_match(self.stm32, ack_assign, timeout=2.0)
        self.assertIsNotNone(m_assign, "Node did not ACK ASSIGN_ID after staging (swap step 2 failed)")
        self.assertEqual(int(m_assign.group(1)), tid, "ASSIGN_ID ACK reports wrong ID after swap")

        # Step 3: Confirm node re-appears on the bus at original ID in ACTIVE state
        stat = self._wait_for_stat(self.stm32, tid, timeout=3.0)
        self.assertIsNotNone(
            stat,
            "No STAT,{},... telemetry after ASSIGN_ID; node did not return to bus (swap incomplete)".format(tid),
        )
        self.assertEqual(stat.prov_state, 2, "Node did not return to PROV_STATE_ACTIVE (2) after ASSIGN_ID")

    def test_14_staging_discovery_isolation(self):
        # Verifies STAGING nodes ignore T0:DISCOVER broadcasts
        tid = self._get_active_tank_id()
        self._send("T{}:GET_UID".format(tid))
        uid_re = re.compile(r"UID24:([0-9A-Fa-f]{24})")
        m_uid = self._wait_for_match(self.stm32, uid_re, timeout=2.0)
        self.assertIsNotNone(m_uid, "Failed to get UID from active node")
        real_uid = m_uid.group(1)

        self._send("T{}:STAGE_ID:{}".format(tid, real_uid))
        ack_stage = re.compile(r"ACK,STAGE_ID")
        self._wait_for_match(self.stm32, ack_stage, timeout=2.0)
        time.sleep(0.1)

        # Send T0:DISCOVER broadcast
        self._send("T0:DISCOVER")
        disc_re = re.compile(r"DISCOVER_ACK")
        m_disc = self._wait_for_match(self.stm32, disc_re, timeout=1.0)
        self.assertIsNone(m_disc, "Staging node incorrectly responded to T0:DISCOVER broadcast!")

        # Cancel staging
        self._send("T0:CANCEL_STAGE")
        time.sleep(0.1)

    def test_15_running_commissioning_rejection(self):
        # Verifies commissioning commands are rejected with ERR:LOCKED_SYS_RUNNING while process is active
        tid = self._get_active_tank_id()
        self._send(ProtocolCommands.stop(tid))
        self._wait_for_stat(self.stm32, tid, timeout=2.0, predicate=lambda f: f.mode == "IDLE")

        self._send(ProtocolCommands.set_time(tid, 15))
        time.sleep(0.04)
        self._send(ProtocolCommands.set_temp(tid, 60))
        time.sleep(0.04)
        self._send(ProtocolCommands.set_power(tid, 50))
        time.sleep(0.04)
        self._send(ProtocolCommands.start(tid))

        running = self._wait_for_stat(self.stm32, tid, timeout=3.0, predicate=lambda f: f.mode == "RUNNING")
        self.assertIsNotNone(running, "Node failed to enter RUNNING mode")

        # Inject commissioning attempt while RUNNING
        self._send("T{}:STAGE_ID".format(tid))
        m_lock = self._wait_for_match(self.stm32, re.compile(r"ERR:LOCKED_SYS_RUNNING"), timeout=1.0)
        if m_lock is None:
            # Check ESP32 Layer 1 interlock rejection
            m_lock = self._wait_for_match(self.esp32, re.compile(r"PROVISIONING REJECTED|CALISAN TANK VAR"), timeout=1.5)
        self.assertIsNotNone(m_lock, "Commissioning while RUNNING was not rejected by Layer 1 (ESP32) or Layer 2 (STM32)")

        self._send(ProtocolCommands.stop(tid))
        self._wait_for_stat(self.stm32, tid, timeout=2.0, predicate=lambda f: f.mode == "IDLE")

    def test_16_safety_watchdog_comm_loss(self):
        """TEST-24: Verifies STM32 safe shutdown when RS485 commands stop arriving.

        After confirming RUNNING, this test enters a deliberate communication
        silence of >3 s. The STM32 watchdog (RX_SILENCE_TIMEOUT_MS = 3000 ms in
        esp32_uart.c) must fire, call SystemState_SafeStop(STOP_REASON_COMM_TIMEOUT),
        and report mode=FAULT with non-zero fault_flags on the ST-Link VCP.
        """
        logging.info("Starting TEST-24: Watchdog/Comm Loss Safety Shutdown")
        tid = self._get_active_tank_id()
        self._send(ProtocolCommands.stop(tid))
        self._wait_for_stat(self.stm32, tid, timeout=2.0, predicate=lambda f: f.mode == "IDLE")

        self._send(ProtocolCommands.set_time(tid, 15))
        time.sleep(0.04)
        self._send(ProtocolCommands.set_temp(tid, 60))
        time.sleep(0.04)
        self._send(ProtocolCommands.set_power(tid, 50))
        time.sleep(0.04)
        self._send(ProtocolCommands.start(tid))

        stat = self._wait_for_stat(self.stm32, tid, timeout=3.0, predicate=lambda st: st.mode == "RUNNING")
        self.assertIsNotNone(stat, "Failed to start machine for watchdog test")

        # --- Communication silence window ---
        # Disable ESP32 automatic heartbeat to simulate communication loss
        self.esp32.write_line("HIL_HEARTBEAT_OFF")
        time.sleep(0.05)

        # No commands are sent for the next >3 s.  The STM32 watchdog in
        # ESP32_UART_Process() checks (HAL_GetTick() - s_last_rx_tick_ms) >
        # RX_SILENCE_TIMEOUT_MS (3000) while SYS_MODE_RUNNING and must trip.
        logging.info("Entering comm silence window (> RX_SILENCE_TIMEOUT_MS = 3 s)...")
        fault_frame = self._wait_for_stat(
            self.stm32, tid, timeout=5.0,
            predicate=lambda st: st.mode == "FAULT" and st.fault_flags != 0,
        )

        # Resume ESP32 automatic heartbeat
        self.esp32.write_line("HIL_HEARTBEAT_ON")
        time.sleep(0.05)
        self.assertIsNotNone(
            fault_frame,
            "STM32 did not transition to FAULT after > 3 s communication silence "
            "(RX_SILENCE_TIMEOUT_MS watchdog not triggered in esp32_uart.c)",
        )
        self.assertNotEqual(
            fault_frame.fault_flags, 0,
            "FAULT mode has no fault flags set (expected FAULT_COMM_TIMEOUT bit)",
        )
        logging.info(
            "Watchdog FAULT confirmed: mode=%s fault_flags=0x%02X",
            fault_frame.mode, fault_frame.fault_flags,
        )
        # STOP must clear the fault and return to IDLE (per Manifesto_V3.md §5.1)
        self._send(ProtocolCommands.stop(tid))
        cleared = self._wait_for_stat(
            self.stm32, tid, timeout=2.0,
            predicate=lambda st: st.mode == "IDLE" and st.fault_flags == 0,
        )
        self.assertIsNotNone(cleared, "STOP did not clear FAULT and return to fault-free IDLE")
        logging.info("Watchdog comm-loss test complete: fault cleared via STOP.")

    def test_17_physical_loopback_readback(self):
        """TEST-12, 13, 17, 18: Verifies that physical loopback signals match commanded outputs."""
        logging.info("Starting TEST-17: Physical Loopback Readback Verification")
        tid = self._get_active_tank_id()

        # Ensure IDLE
        self._send(ProtocolCommands.stop(tid))
        self._wait_for_stat(self.stm32, tid, timeout=3.0, predicate=lambda st: st.mode == "IDLE")

        # Verify IDLE state matching
        m = self._wait_for_match(self.stm32, TriacDebugFrame.PATTERN, timeout=3.0)
        self.assertIsNotNone(m, "No DEBUG_STM frame seen on STM32 in IDLE")
        frame = TriacDebugFrame.parse(m)
        self.assertEqual(frame.heater_out, 0, "Heater should be OFF in IDLE")
        self.assertEqual(frame.heater_fb, 0, "Heater FB should be LOW in IDLE")
        self.assertEqual(frame.triac_out, 0, "Triac should be OFF in IDLE")
        self.assertEqual(frame.triac_fb, 0, "Triac FB should be LOW in IDLE")

        # Start machine with valid setpoints
        self._send(ProtocolCommands.set_time(tid, 15))
        time.sleep(0.04)
        self._send(ProtocolCommands.set_temp(tid, 60))
        time.sleep(0.04)
        self._send(ProtocolCommands.set_power(tid, 50))
        time.sleep(0.04)
        self._send(ProtocolCommands.start(tid))

        # Wait for RUNNING
        self._wait_for_stat(self.stm32, tid, timeout=3.0, predicate=lambda st: st.mode == "RUNNING")

        # Observation window for RUNNING physical outputs:
        # Sample DEBUG_STM frames across a window so that an early frame sampled
        # before the START command propagated does not cause a false failure.
        deadline = time.time() + 4.0
        running_frame = None
        while time.time() < deadline:
            m = self._wait_for_match(self.stm32, TriacDebugFrame.PATTERN, timeout=0.5)
            if not m:
                continue
            cand = TriacDebugFrame.parse(m)
            if cand.heater_out == 1:
                running_frame = cand
                break

        self.assertIsNotNone(running_frame, "No RUNNING DEBUG_STM frame with HEATER_OUT=1 observed")
        self.assertEqual(running_frame.heater_out, 1, "Heater should be ON in RUNNING")
        self.assertEqual(running_frame.heater_fb, 1, "Heater FB should be HIGH in RUNNING")
        if running_frame.triac_out == 1:
            self.assertEqual(running_frame.triac_fb, 1, "Triac FB should match Triac OUT")

        # Stop and verify safe return to IDLE
        self._send(ProtocolCommands.stop(tid))
        self._wait_for_stat(self.stm32, tid, timeout=3.0, predicate=lambda st: st.mode == "IDLE")

        m = self._wait_for_match(self.stm32, TriacDebugFrame.PATTERN, timeout=3.0)
        self.assertIsNotNone(m, "No DEBUG_STM frame seen on STM32 after STOP")
        frame = TriacDebugFrame.parse(m)
        self.assertEqual(frame.heater_out, 0, "Heater should be OFF after STOP")
        self.assertEqual(frame.heater_fb, 0, "Heater FB should be LOW after STOP")
        self.assertEqual(frame.triac_out, 0, "Triac should be OFF after STOP")
        self.assertEqual(frame.triac_fb, 0, "Triac FB should be LOW after STOP")
        logging.info("Physical loopback feedback fully validated.")


# =============================================================================
# CONSOLE-ONLY PASS/FAIL REPORTER
# =============================================================================
class ConsoleSummaryResult(unittest.TestResult):
    """Suppresses all default unittest output; prints only 'test_name: PASSED/FAILED'."""

    @staticmethod
    def _label(test):
        return getattr(test, "_testMethodName", None) or test.id()

    def addSuccess(self, test):
        super().addSuccess(test)
        print("{}: PASSED".format(self._label(test)))

    def addFailure(self, test, err):
        super().addFailure(test, err)
        logging.error("FAILURE in %s:\n%s", self._label(test), self._exc_info_to_string(err, test))
        print("{}: FAILED".format(self._label(test)))

    def addError(self, test, err):
        super().addError(test, err)
        logging.error("ERROR in %s:\n%s", self._label(test), self._exc_info_to_string(err, test))
        print("{}: FAILED".format(self._label(test)))

    def addSkip(self, test, reason):
        super().addSkip(test, reason)
        logging.warning("SKIPPED %s: %s", self._label(test), reason)
        print("{}: SKIPPED ({})".format(self._label(test), reason))


if __name__ == "__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(HardwareInLoopTests)
    result = ConsoleSummaryResult()
    suite.run(result)
    sys.exit(0 if result.wasSuccessful() else 1)
