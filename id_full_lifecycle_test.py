"""
id_full_lifecycle_test.py - Full end-to-end EAGLE-PROV-v2 ID commissioning
architecture test, driven entirely over the real RS485 bus via the ESP32's
USB console (/dev/ttyACM0). Exercises: service auth, RESET_ID, DISCOVER,
ASSIGN_ID (fresh commissioning), STAGE_ID+ASSIGN_ID (ID change), RESET_ID
again, a real SWD reset to test DIP-switch fallback, then restores the
board to its original production Tank ID (1) via DISCOVER+ASSIGN_ID.

Captures the ESP32 console output continuously for the whole test and
downloads it as one timestamped .txt log.

Usage:
    python id_full_lifecycle_test.py
"""
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
import rpi_exec  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parent
LOG_DIR = REPO_ROOT / "logs"
REMOTE_LOG = "/tmp/id_lifecycle_test_log.txt"

ESP32_DEVICE = "/dev/ttyACM0"
BOARD_UID = "001400183235510230393936"  # captured earlier via GET_UID, this chip's real HW UID
ORIGINAL_TANK_ID = 1  # restore target at the end

TOTAL_CAPTURE_S = 50  # generous margin over the ~25-30s the sequence needs


def start_background_capture():
    print(f"[INFO] {ESP32_DEVICE} 115200 baud ayarlaniyor, {TOTAL_CAPTURE_S}sn arka plan capture basliyor...")
    rpi_exec.run_cmd(f"stty -F {ESP32_DEVICE} 115200 raw -echo", timeout=10)
    bg_cmd = f"timeout {TOTAL_CAPTURE_S} cat {ESP32_DEVICE} > {REMOTE_LOG} 2>/dev/null < /dev/null &"
    rpi_exec.run_cmd(bg_cmd, timeout=10)
    time.sleep(0.5)


def send(cmd: str):
    print(f"[SEND] {cmd}")
    esc = cmd.replace("'", "'\\''")
    rpi_exec.run_cmd(f"printf '%s\\n' '{esc}' > {ESP32_DEVICE}", timeout=10)


def phase(title: str):
    print(f"\n=== {title} ===")


def hw_reset_via_swd():
    print("[INFO] SWD uzerinden gercek donanim reset (openocd reset run)...")
    cmd = (
        "openocd "
        "-f interface/stlink.cfg "
        "-f target/stm32g4x.cfg "
        '-c "transport select swd; init; reset run; shutdown"'
    )
    rc, out, err = rpi_exec.run_cmd(cmd, timeout=30)
    print(out[-500:] if out else "")
    if err:
        print(err[-300:])


def fetch_log() -> Path:
    LOG_DIR.mkdir(exist_ok=True)
    ts = time.strftime("%Y%m%d_%H%M%S")
    local_path = LOG_DIR / f"id_lifecycle_test_{ts}.txt"

    client = rpi_exec.get_client()
    try:
        sftp = client.open_sftp()
        sftp.get(REMOTE_LOG, str(local_path))
        sftp.close()
    finally:
        client.close()

    print(f"\n[OK] Log indirildi -> {local_path}")
    return local_path


def main() -> int:
    try:
        start_background_capture()

        phase("1) SERVIS YETKILENDIRME (sifre: 123456)")
        for digit in "123456":
            send(f"KEY_{digit}")
            time.sleep(0.1)
        send("KEY_OK")
        time.sleep(1.0)

        phase(f"2) YENI KART SIMULASYONU: T{ORIGINAL_TANK_ID}:RESET_ID")
        send(f"T{ORIGINAL_TANK_ID}:RESET_ID")
        time.sleep(1.5)

        phase("3) DISCOVER (T0:DISCOVER) - slot gecikmesi ~650ms'ye kadar surebilir")
        send("T0:DISCOVER")
        time.sleep(2.0)

        phase(f"4) ILK ATAMA: T0:ASSIGN_ID:{ORIGINAL_TANK_ID}:{BOARD_UID}")
        send(f"T0:ASSIGN_ID:{ORIGINAL_TANK_ID}:{BOARD_UID}")
        time.sleep(1.5)

        phase("5) ID DEGISIKLIGI: STAGE_ID -> ASSIGN_ID:5")
        send(f"T{ORIGINAL_TANK_ID}:STAGE_ID:{BOARD_UID}")
        time.sleep(1.0)
        send(f"T0:ASSIGN_ID:5:{BOARD_UID}")
        time.sleep(1.5)

        phase("6) RESET_ID (ID=5 uzerinden)")
        send(f"T5:RESET_ID:{BOARD_UID}")
        time.sleep(1.5)

        phase("7) DIP SWITCH FALLBACK TESTI: gercek SWD reset")
        hw_reset_via_swd()
        print("[INFO] Reset sonrasi 5sn boyunca telemetriyi izliyoruz (DIP switch etkisi varsa STAT gorunur)...")
        time.sleep(5.0)

        phase(f"8) URETIM DURUMUNA GERI YUKLEME: DISCOVER + ASSIGN_ID:{ORIGINAL_TANK_ID}")
        # If the DIP-fallback reset left the board at some non-zero ID, RESET_ID
        # it first (harmless no-op if it's already 0/uncommissioned -- STM32 will
        # just NACK or ignore since prov_state is already UNCOMMISSIONED, that's fine).
        send(f"T0:RESET_ID")
        time.sleep(1.0)
        send("T0:DISCOVER")
        time.sleep(2.0)
        send(f"T0:ASSIGN_ID:{ORIGINAL_TANK_ID}:{BOARD_UID}")
        time.sleep(1.5)

        print("\n[INFO] Kalan capture suresini bekliyoruz...")
        time.sleep(3.0)

        fetch_log()
        return 0
    except Exception as exc:
        print(f"\nHATA: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
