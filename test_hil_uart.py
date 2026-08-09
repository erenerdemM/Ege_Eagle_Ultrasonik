"""
Hardware-in-the-Loop (HIL) UART test suite for the EAGLEULTRASONiK washing
machine controller (ESP32 master <-> STM32 slave, 115200 baud, protocol per
Manifesto_V3.md section 5).

PHYSICAL WIRING CONSTRAINT: the host PC has no access to the shared internal
ESP32<->STM32 bus and no access to the Nextion HMI. The only two links
available are two independent USB-serial channels:

  * ESP32_PORT (COM10) - the ESP32's USB debug console (Serial). Test commands
    are written here; the ESP32 firmware (ekran_kontrol.ino) detects bus-style
    "T<id>:..." frames and forwards them verbatim to the STM32 over its
    internal Serial1 link, logging "[PC->STM] ..." / "[STM->ESP] STAT,..." back
    on this same port so it can also be used as a secondary telemetry source.

  * STM32_PORT (COM11) - the STM32's ST-Link Virtual COM Port (LPUART1). The
    STM32 firmware (esp32_uart.c) now mirrors every STAT,... telegram it sends
    to the ESP32 directly onto this port, so it is read here as the ground
    truth for the slave's own state.

Because the PC can no longer impersonate the STM32 slave on the bus (tests
04/05 in the previous revision fabricated STAT/fault frames directly on the
bus), pure fault-injection and bus-silence/watchdog tests are not physically
realizable from software alone anymore. They have been replaced with tests
that are honest about what this wiring can verify: the STOP-as-fault-ack path
using real hardware, and a dual-channel consistency check that cross-verifies
the ESP32's forwarded telemetry log against the STM32's own COM11 report.

Run from the workspace root:  python test_hil_uart.py
"""

import re
import sys
import time
import logging
import unittest

import serial

# =============================================================================
# CONFIGURATION - adjust COM ports to match the physical bench wiring.
# =============================================================================
ESP32_PORT = "COM10"   # ESP32 USB debug console (Serial) - test commands go in here
STM32_PORT = "COM11"   # STM32 ST-Link Virtual COM Port (LPUART1) - telemetry ground truth

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

    def close(self):
        self.serial.close()
        logging.info("[%s] closed", self.name)


# =============================================================================
# PROTOCOL HELPERS
# =============================================================================
class TelemetryFrame:
    """Parses one STAT,<id>,<mode>,<rem_sec>,<temp_x10>,<relay>,<power>,<fault> line."""

    PATTERN = re.compile(
        r"STAT,(\d+),(IDLE|RUNNING|FAULT),(\d+),(-?\d+),(\d+),(\d+),(\d+)$"
    )

    def __init__(self, tank_id, mode, remaining_sec, temp_c, relay, power_pct, fault_flags):
        self.tank_id = tank_id
        self.mode = mode
        self.remaining_sec = remaining_sec
        self.temp_c = temp_c
        self.relay = relay
        self.power_pct = power_pct
        self.fault_flags = fault_flags

    @classmethod
    def parse(cls, line):
        # Returns a TelemetryFrame or None; search() (not match()) tolerates the
        # "[STM->ESP] " log prefix the ESP32 prepends when forwarding on COM10.
        if not line:
            return None
        m = cls.PATTERN.search(line)
        if not m:
            return None
        tank_id, mode, rem_sec, temp_x10, relay, power_pct, fault_flags = m.groups()
        return cls(
            int(tank_id), mode, int(rem_sec), int(temp_x10) / 10.0,
            int(relay), int(power_pct), int(fault_flags),
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
    def start(tank_id):
        return "T{}:START".format(tank_id)

    @staticmethod
    def stop(tank_id):
        return "T{}:STOP".format(tank_id)


# =============================================================================
# HIL_DEEP_DEBUG WHITE-BOX FRAME PARSERS
# =============================================================================
class TriacDebugFrame:
    """Parses one 'DEBUG_STM: ADC=<raw>, DELAY=<us>, RELAY=<0/1>' line (main.c HIL_DeepDebug_Print)."""

    PATTERN = re.compile(r"DEBUG_STM:\s*ADC=(\d+),\s*DELAY=(\d+),\s*RELAY=([01])")

    def __init__(self, adc_raw, delay_us, relay):
        self.adc_raw = adc_raw
        self.delay_us = delay_us
        self.relay = relay

    @classmethod
    def parse(cls, line_or_match):
        # Accepts either a raw line or an already-matched re.Match (from _wait_for_match)
        m = line_or_match if isinstance(line_or_match, re.Match) else (
            cls.PATTERN.search(line_or_match) if line_or_match else None
        )
        if not m:
            return None
        return cls(int(m.group(1)), int(m.group(2)), int(m.group(3)))


# =============================================================================
# TEST SUITE
# =============================================================================
class HardwareInLoopTests(unittest.TestCase):
    """Modular HIL regression tests for the ESP32<->STM32 UART protocol.

    All commands are injected via ESP32_PORT (COM10); telemetry is verified
    against STM32_PORT (COM11), the STM32's own ground-truth report, with
    ESP32_PORT's forwarded "[STM->ESP] STAT,..." log used as a secondary,
    cross-checked source.
    """

    active_tank_id = 1  # updated once test_01 renames the slave

    @classmethod
    def setUpClass(cls):
        try:
            cls.esp32 = UARTBus("ESP32", ESP32_PORT, ESP32_BAUD)
            cls.stm32 = UARTBus("STM32", STM32_PORT, STM32_BAUD)
        except serial.SerialException as exc:
            # Log the raw pyserial error (port name/PermissionError/FileNotFoundError) before
            # converting to a skip, since a bare SkipTest reason is otherwise never surfaced.
            logging.error("COM port open failed: %s", exc)
            print("SETUP FAILED: {}".format(exc))
            raise unittest.SkipTest(
                "HIL bench not connected ({}). Check cabling/COM port mapping.".format(exc)
            )

    @classmethod
    def tearDownClass(cls):
        for link in (cls.esp32, cls.stm32):
            link.close()

    # --- shared helpers ---------------------------------------------------
    def _send(self, command):
        # Injects a "T<id>:..." bus command via the ESP32's USB debug port (COM10);
        # ekran_kontrol.ino forwards it verbatim to the STM32 over Serial1.
        self.esp32.write_line(command)

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
        # Generic HIL_DEEP_DEBUG line poller: returns the first re.Match of `pattern` on `bus`
        # satisfying `predicate(match)`, or None on timeout. Used for DEBUG_STM/DEBUG_ESP32 lines.
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
        # Broadcasts T0:SET_ID:2 via COM10 and confirms the slave now reports TankID=2 on COM11
        new_id = 2
        self._send(ProtocolCommands.set_id_broadcast(new_id))
        frame = self._wait_for_stat(self.stm32, new_id, timeout=3.0)
        self.assertIsNotNone(frame, "No STAT,{},... telemetry seen on COM11 after T0:SET_ID:{}".format(new_id, new_id))
        type(self).active_tank_id = new_id  # subsequent tests target the renamed slave

    # --- 2. Parameter transmission ------------------------------------------
    def test_02_parameter_transmission(self):
        # Sends SET_TIME/SET_TEMP/SET_POWER and confirms the slave applies them once running
        tid = type(self).active_tank_id
        self._send(ProtocolCommands.set_time(tid, 15))
        self._send(ProtocolCommands.set_temp(tid, 60))
        self._send(ProtocolCommands.set_power(tid, 50))
        self._send(ProtocolCommands.start(tid))

        # remaining_sec is loaded to setpoint_time_minutes*60 the instant RUNNING begins
        running = self._wait_for_stat(self.stm32, tid, timeout=2.0, predicate=lambda f: f.mode == "RUNNING")
        self.assertIsNotNone(running, "Slave never reported RUNNING after START")
        self.assertGreaterEqual(running.remaining_sec, 895, "SET_TIME:15 not reflected in remaining_sec")

        # actual power ramps toward the setpoint via soft-start; poll until it settles near 50%
        settled = self._wait_for_stat(self.stm32, tid, timeout=6.0, predicate=lambda f: abs(f.power_pct - 50) <= 5)
        self.assertIsNotNone(settled, "SET_POWER:50 never reached by the soft-start ramp")

        self._send(ProtocolCommands.stop(tid))  # leave the slave idle for later tests

    # --- 3. Operation cycle ---------------------------------------------------
    def test_03_operation_cycle(self):
        # START must flip telemetry mode to RUNNING; STOP must flip it back to IDLE
        tid = type(self).active_tank_id
        self._send(ProtocolCommands.start(tid))
        running = self._wait_for_stat(self.stm32, tid, timeout=2.0, predicate=lambda f: f.mode == "RUNNING")
        self.assertIsNotNone(running, "Mode did not switch to RUNNING after START")

        self._send(ProtocolCommands.stop(tid))
        idle = self._wait_for_stat(self.stm32, tid, timeout=2.0, predicate=lambda f: f.mode == "IDLE")
        self.assertIsNotNone(idle, "Mode did not switch to IDLE after STOP")

    # --- 4. STOP-as-fault-ack (real hardware) ----------------------------------
    def test_04_stop_clears_fault(self):
        # NOTE: without bus access the PC cannot fabricate a FAULT STAT frame anymore;
        # this instead confirms real hardware's STOP path always yields fault_flags==0
        # (esp32_uart.c: "STOP also acts as a manual fault acknowledge/reset").
        tid = type(self).active_tank_id
        self._send(ProtocolCommands.stop(tid))
        idle = self._wait_for_stat(
            self.stm32, tid, timeout=2.0,
            predicate=lambda f: f.mode == "IDLE" and f.fault_flags == 0,
        )
        self.assertIsNotNone(idle, "STOP did not settle the slave into a fault-free IDLE state")

    # --- 5. Dual-channel telemetry consistency ---------------------------------
    def test_05_dual_channel_consistency(self):
        # Cross-verifies the ESP32's forwarded "[STM->ESP] STAT,..." log (COM10) against
        # the STM32's own ground-truth report (COM11) to confirm the forwarding path is intact.
        tid = type(self).active_tank_id
        self._send(ProtocolCommands.set_power(tid, 40))

        stm32_frame = self._wait_for_stat(self.stm32, tid, timeout=3.0)
        self.assertIsNotNone(stm32_frame, "No STAT,{},... seen on COM11 (STM32 ground truth)".format(tid))

        esp32_frame = self._wait_for_stat(self.esp32, tid, timeout=3.0)
        self.assertIsNotNone(esp32_frame, "No forwarded STAT,{},... seen on COM10 (ESP32 log)".format(tid))

        self.assertEqual(stm32_frame.tank_id, esp32_frame.tank_id, "Tank ID mismatch between COM11 and COM10")
        self.assertEqual(stm32_frame.mode, esp32_frame.mode, "Mode mismatch between COM11 and COM10")

    # --- 6. Triac phase-angle math (white-box) -------------------------------
    def test_06_triac_math(self):
        # Confirms PowerPctToDelayUs() in ultrasonic_pwm.c computes the mathematically correct
        # firing delay for a known power setpoint: delay = MAX - (MAX-MIN)*power/100
        tid = type(self).active_tank_id
        power_pct = 50
        expected_delay_us = TRIAC_MAX_DELAY_US - ((TRIAC_MAX_DELAY_US - TRIAC_MIN_DELAY_US) * power_pct // 100)

        self._send(ProtocolCommands.set_power(tid, power_pct))
        self._send(ProtocolCommands.start(tid))

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

    # --- 7. PT100 ADC-to-temperature correlation (white-box) -----------------
    def test_07_pt100_adc(self):
        # Cross-checks the raw ADC2 count (DEBUG_STM, COM11) against the temperature the STAT
        # telegram reports, using the same linear calibration as pt100_adc.c
        tid = type(self).active_tank_id

        m = self._wait_for_match(self.stm32, TriacDebugFrame.PATTERN, timeout=3.0)
        self.assertIsNotNone(m, "No DEBUG_STM ADC=... line seen on COM11")
        debug_frame = TriacDebugFrame.parse(m)

        stat_frame = self._wait_for_stat(self.stm32, tid, timeout=3.0)
        self.assertIsNotNone(stat_frame, "No STAT,{},... telemetry seen on COM11".format(tid))

        expected_temp_c = (debug_frame.adc_raw * PT100_CAL_SLOPE) + PT100_CAL_OFFSET
        self.assertAlmostEqual(
            stat_frame.temp_c, expected_temp_c, delta=1.0,
            msg="Reported temp_c ({}) does not correlate with raw ADC={} (expected ~{:.2f}C)".format(
                stat_frame.temp_c, debug_frame.adc_raw, expected_temp_c),
        )

    # --- 8. ESP32 internals: NVS persistence + HMI payload parsing -----------
    def test_08_esp32_internals(self):
        # Drives P_SAVE via the ESP32 USB debug console (routed through komutIsle exactly like a
        # real Nextion payload since it doesn't match the "T<id>:" bus prefix) and verifies both
        # the parsed HMI fields and the resulting NVS write-back, plus the 3000ms WDT state line.
        prog = 3
        sure, sicaklik = 22, 65

        self._send("EDIT_P{}".format(prog))
        time.sleep(0.1)  # emulate human delay between Nextion screen change and save tap
        self._send("P_SAVE|{}|{}".format(sure, sicaklik))

        parse_re = re.compile(r"DEBUG_ESP32: HMI_PARSE cmd=P_SAVE prog=(\d+) sure=(\d+) sicaklik=(\d+)")
        parsed = self._wait_for_match(
            self.esp32, parse_re, timeout=3.0,
            predicate=lambda m: int(m.group(1)) == prog,
        )
        self.assertIsNotNone(parsed, "No DEBUG_ESP32 HMI_PARSE line seen for P_SAVE on COM10")
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

        # Watchdog: confirm the 3000ms WDT state line reports this tank as currently connected
        tid = type(self).active_tank_id
        wdt_re = re.compile(r"DEBUG_ESP32: WDT tank=(\d+) connected=([01]) age_ms=(\d+)")
        wdt = self._wait_for_match(
            self.esp32, wdt_re, timeout=4.0,
            predicate=lambda m: int(m.group(1)) == tid,
        )
        self.assertIsNotNone(wdt, "No DEBUG_ESP32 WDT line seen for tank {}".format(tid))
        self.assertEqual(wdt.group(2), "1", "Watchdog reports tank {} as disconnected".format(tid))


# =============================================================================
# CONSOLE-ONLY PASS/FAIL REPORTER
# =============================================================================
class ConsoleSummaryResult(unittest.TestResult):
    """Suppresses all default unittest output; prints only 'test_name: PASSED/FAILED'."""

    @staticmethod
    def _label(test):
        # setUpClass-level errors pass an _ErrorHolder (no _testMethodName); fall back to id()
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
