"""List Pi serial devices with their USB vendor/model info, to tell ST-Link
VCP apart from the ESP32's USB-serial chip."""
import rpi_exec

cmd = (
    "ls -la /dev/serial/by-id/ 2>/dev/null; "
    "echo ---; "
    "for d in /dev/ttyACM* /dev/ttyUSB*; do "
    "echo \"$d:\"; "
    "udevadm info -q property -n \"$d\" 2>/dev/null | grep -E 'ID_VENDOR|ID_MODEL|ID_SERIAL'; "
    "done"
)

rc, out, err = rpi_exec.run_cmd(cmd, timeout=15)
print("=== STDOUT ===")
print(out)
if err:
    print("=== STDERR ===")
    print(err)
