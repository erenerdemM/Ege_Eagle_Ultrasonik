"""
live_monitor.py - STM32 ST-Link VCP seri cikisini CANLI olarak terminale akitir.
Durdurmak icin Ctrl+C.

Kullanim:
    python live_monitor.py                    # ST-Link'i otomatik bulur
    python live_monitor.py --device /dev/ttyACM1
"""
from pathlib import Path
import argparse
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
import rpi_exec  # noqa: E402


def detect_stlink_device() -> str:
    """/dev/serial/by-id/ icinde 'STMicroelectronics' gecen linki bulup
    gercek /dev/ttyACMx yolunu dondurur (ESP32'nin CH340/CP210x cipiyle
    karismamasi icin)."""
    rc, out, err = rpi_exec.run_cmd(
        "ls -la /dev/serial/by-id/ 2>/dev/null", timeout=10
    )
    for line in out.splitlines():
        if "STMicroelectronics" in line and "->" in line:
            target = line.split("->")[-1].strip()
            # target something like ../../ttyACM1
            dev_name = target.split("/")[-1]
            return f"/dev/{dev_name}"
    raise RuntimeError(
        "ST-Link VCP bulunamadi (/dev/serial/by-id/ icinde 'STMicroelectronics' "
        "gecen bir link yok). --device ile elle belirt."
    )


def stream_live(device: str) -> None:
    print(f"[INFO] Canli izleme basliyor: {device} (115200 baud). "
          f"Durdurmak icin Ctrl+C.\n")

    client = rpi_exec.get_client()
    try:
        # stty ayarini once tek seferlik komutla yapiyoruz
        client.exec_command(f"stty -F {device} 115200 raw -echo")
        time.sleep(0.3)

        # Sonsuz 'cat' komutunu pty uzerinden acip canli okuyoruz
        channel = client.get_transport().open_session()
        channel.get_pty()
        channel.exec_command(f"cat {device}")

        while True:
            if channel.recv_ready():
                data = channel.recv(4096).decode("utf-8", errors="replace")
                sys.stdout.write(data)
                sys.stdout.flush()
            if channel.exit_status_ready():
                break
            time.sleep(0.05)
    except KeyboardInterrupt:
        print("\n[INFO] Durduruldu.")
    finally:
        client.close()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--device", type=str, default=None,
                         help="Seri port yolu (orn. /dev/ttyACM1). "
                              "Verilmezse ST-Link otomatik bulunur.")
    args = parser.parse_args()

    try:
        device = args.device or detect_stlink_device()
        stream_live(device)
        return 0
    except Exception as exc:
        print(f"\nHATA: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
