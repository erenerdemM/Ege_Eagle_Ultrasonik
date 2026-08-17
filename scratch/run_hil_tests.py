import sys, os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import rpi_exec

cmd = "cd /home/eren/EAGLEULTRASONiK && python3 -m pytest test_hil_uart.py -v"
rc, out, err = rpi_exec.run_cmd(cmd, timeout=300)
print("=== STDOUT ===")
print(out)
if err:
    print("=== STDERR ===")
    print(err)
print(f"=== EXIT CODE: {rc} ===")
