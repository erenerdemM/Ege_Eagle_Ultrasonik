import sys, os, time
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import rpi_exec

cmd = """python3 - << 'EOF'
import serial, time, re

esp32 = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
stm32 = serial.Serial('/dev/ttyACM1', 115200, timeout=1)

time.sleep(0.1)

# Unlock service menu
for digit in "123456":
    esp32.write(f"KEY_{digit}\\n".encode('ascii'))
    time.sleep(0.02)
esp32.write(b"KEY_OK\\n")
time.sleep(0.2)

# Determine active ID
stm32.reset_input_buffer()
active_id = 1
t0 = time.time()
while time.time() - t0 < 2.0:
    line = stm32.readline().decode('ascii', errors='replace').strip()
    m = re.match(r"STAT,(\d+),", line)
    if m:
        active_id = int(m.group(1))
        break

print(f"Active Tank ID before restoration: {active_id}")

# If active_id != 1, restore to Tank 1
if active_id != 1:
    esp32.write(f"T{active_id}:GET_UID\\n".encode('ascii'))
    t0 = time.time()
    uid = None
    while time.time() - t0 < 2.0:
        line = stm32.readline().decode('ascii', errors='replace').strip()
        m = re.search(r"UID24:([0-9A-Fa-f]{24})", line)
        if m:
            uid = m.group(1)
            break
    if uid:
        print(f"UID found: {uid}. Staging and reassigning to Tank 1...")
        esp32.write(f"T{active_id}:STAGE_ID:{uid}\\n".encode('ascii'))
        time.sleep(0.1)
        esp32.write(f"T0:ASSIGN_ID:1:{uid}\\n".encode('ascii'))
        time.sleep(0.2)
        active_id = 1

# Send baseline restoration commands
cmds = [
    f"T{active_id}:STOP",
    f"T{active_id}:SET_TIME:15",
    f"T{active_id}:SET_TEMP:50",
    f"T{active_id}:SET_POWER:100",
    f"T{active_id}:SET_FREQ:28",
    f"T{active_id}:SWEEP:OFF",
    f"T{active_id}:SET_SWP_SPAN:2",
    f"T{active_id}:SET_SWP_PER:400",
    f"T{active_id}:SET_STEP_INC:4",
    f"T{active_id}:CLEAR_FAULT",
]

for c in cmds:
    esp32.write((c + "\\n").encode('ascii'))
    time.sleep(0.05)

# Read back telemetry
time.sleep(0.5)
stm32.reset_input_buffer()
lines = []
t0 = time.time()
while time.time() - t0 < 2.0 and len(lines) < 5:
    line = stm32.readline().decode('ascii', errors='replace').strip()
    if line.startswith("STAT"):
        lines.append(line)

print("Restoration telemetry readback:")
for l in lines:
    print(l)

esp32.close()
stm32.close()
EOF
"""

rc, out, err = rpi_exec.run_cmd(cmd)
print("OUT:")
print(out)
if err:
    print("ERR:", err)
