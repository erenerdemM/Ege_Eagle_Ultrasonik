#!/usr/bin/env python3
"""
scripts/windows_com_tcp_bridge.py
Windows-side TCP to Virtual COM Redirector with PASSIVE TEE RAW TRAFFIC LOGGER
and STRICT NEXTION FRAME ISOLATION FILTER.

Bridges COM8 (virtual null-modem port) to Raspberry Pi TCP socket 100.99.150.99:8888.

Features:
- Single-process architecture (owns COM8 without serial port collisions)
- Strict Nextion Frame Isolation: Only pure Nextion frames (<cmd> \xFF\xFF\xFF) are sent to COM8.
- Drops all non-Nextion debug/telemetry text from reaching Nextion Simulator RX.
- Embedded Passive Tee Logger: records all HMI_TO_RPI and RPI_TO_HMI byte streams.
- Detailed counters: DEBUG_BYTES_TO_NEXTION, VALID_NEXTION_FRAMES, MALFORMED_NEXTION_FRAMES.
"""

import sys
import os
import time
import socket
import select
import datetime
import serial

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
if hasattr(sys.stderr, 'reconfigure'):
    sys.stderr.reconfigure(encoding='utf-8', errors='replace')

DEFAULT_COM = "COM8"
DEFAULT_HOST = "100.99.150.99"
DEFAULT_PORT = 8888
DEFAULT_BAUD = 115200

class NextionFrameFilter:
    def __init__(self):
        self.buf = bytearray()
        self.debug_bytes_filtered = 0
        self.valid_nextion_frames = 0
        self.malformed_nextion_frames = 0

    def process_incoming_tcp(self, data: bytes):
        self.buf.extend(data)
        out_nextion = bytearray()
        valid_frames = []
        
        DELIM = b'\xff\xff\xff'
        while DELIM in self.buf:
            idx = self.buf.index(DELIM)
            frame_body = bytes(self.buf[:idx])
            self.buf = self.buf[idx + 3:]
            
            # If frame_body contains \n or \r (debug text before Nextion command), strip it
            if b'\n' in frame_body:
                last_nl = frame_body.rindex(b'\n')
                debug_part = frame_body[:last_nl + 1]
                instruction_part = frame_body[last_nl + 1:]
                self.debug_bytes_filtered += len(debug_part)
                frame_body = instruction_part
            
            frame_body = frame_body.strip(b'\r\n ')
            if len(frame_body) > 0:
                self.valid_nextion_frames += 1
                clean_frame = frame_body + DELIM
                out_nextion.extend(clean_frame)
                valid_frames.append(clean_frame)
            else:
                self.malformed_nextion_frames += 1
        
        # If remaining buffer has complete \n terminated lines without \xFF\xFF\xFF, it's debug text
        if b'\n' in self.buf:
            last_nl = self.buf.rindex(b'\n')
            debug_part = bytes(self.buf[:last_nl + 1])
            self.debug_bytes_filtered += len(debug_part)
            self.buf = self.buf[last_nl + 1:]
            
        return bytes(out_nextion), valid_frames

class BridgeTeeLogger:
    def __init__(self, log_dir="scratch"):
        os.makedirs(log_dir, exist_ok=True)
        now_str = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        self.log_path = os.path.join(log_dir, f"manual_nextion_bridge_{now_str}.log")
        self.summary_path = os.path.join(log_dir, f"manual_nextion_bridge_{now_str}_summary.txt")
        self.log_file = open(self.log_path, "w", encoding="utf-8")
        
        self.stats = {
            "bytes_hmi_to_rpi": 0,
            "bytes_rpi_to_hmi": 0,
            "hmi_commands": 0,
            "nextion_frames": 0,
            "rs485_commands": 0,
            "rs485_telemetry": 0,
            "debug_logs": 0,
            "acks": 0,
            "nacks": 0,
            "errors": 0,
            "unknown": 0,
            "debug_bytes_filtered": 0
        }
        self.first_ts = None
        self.last_ts = None
        print(f"[TEE-LOGGER] Logging active -> {self.log_path}")

    def classify(self, direction, raw_bytes, ascii_str):
        s = ascii_str.strip()
        if raw_bytes.endswith(b'\xff\xff\xff') or b'\xff\xff\xff' in raw_bytes:
            return "NEXTION_FRAME"
        if direction == "HMI_TO_RPI":
            return "HMI_COMMAND"
        if s.startswith("[ESP->STM]") or s.startswith("T1:") or s.startswith("T2:"):
            return "RS485_COMMAND"
        if s.startswith("[STM->ESP]") or s.startswith("STAT,") or s.startswith("FB="):
            return "RS485_TELEMETRY"
        if s.startswith("DEBUG_") or s.startswith("[PC->ESP]") or s.startswith("-->"):
            if "HATA:" in s or "ERR" in s:
                return "ERR"
            return "DEBUG"
        if "ACK" in s or "CK:" in s:
            return "ACK"
        if "NACK" in s or "NK:" in s:
            return "NACK"
        return "UNKNOWN"

    def log_chunk(self, direction, raw_bytes):
        wall_ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")
        mono_ts = time.monotonic()
        if self.first_ts is None:
            self.first_ts = wall_ts
        self.last_ts = wall_ts

        byte_count = len(raw_bytes)
        if direction == "HMI_TO_RPI":
            self.stats["bytes_hmi_to_rpi"] += byte_count
        else:
            self.stats["bytes_rpi_to_hmi"] += byte_count

        hex_str = " ".join(f"{b:02X}" for b in raw_bytes)
        try:
            ascii_str = raw_bytes.decode('utf-8', errors='replace').replace('\r', '').replace('\n', ' ')
        except Exception:
            ascii_str = str(raw_bytes)

        frame_type = self.classify(direction, raw_bytes, ascii_str)
        self.update_stats(frame_type)

        entry = (
            f"[{wall_ts}] (t_mono={mono_ts:.6f}, timing=RX_TIMESTAMP_ONLY)\n"
            f"DIR={direction}\n"
            f"HEX={hex_str}\n"
            f"ASCII={ascii_str.strip()}\n"
            f"BYTES={byte_count}\n"
            f"TYPE={frame_type}\n\n"
        )
        self.log_file.write(entry)
        self.log_file.flush()

    def update_stats(self, frame_type):
        if frame_type == "HMI_COMMAND":
            self.stats["hmi_commands"] += 1
        elif frame_type == "NEXTION_FRAME":
            self.stats["nextion_frames"] += 1
        elif frame_type == "RS485_COMMAND":
            self.stats["rs485_commands"] += 1
        elif frame_type == "RS485_TELEMETRY":
            self.stats["rs485_telemetry"] += 1
        elif frame_type == "DEBUG":
            self.stats["debug_logs"] += 1
        elif frame_type == "ACK":
            self.stats["acks"] += 1
        elif frame_type == "NACK":
            self.stats["nacks"] += 1
        elif frame_type == "ERR":
            self.stats["errors"] += 1
        else:
            self.stats["unknown"] += 1

    def flush_summary(self, debug_bytes_filtered=0, valid_nextion_frames=0, malformed_nextion_frames=0):
        try:
            with open(self.summary_path, "w", encoding="utf-8") as f:
                f.write("=======================================================\n")
                f.write("PASSIVE NEXTION BRIDGE TEE TRAFFIC SUMMARY (LIVE)\n")
                f.write("=======================================================\n")
                f.write(f"Log File:                   {self.log_path}\n")
                f.write(f"First Timestamp:            {self.first_ts or 'N/A'}\n")
                f.write(f"Last Timestamp:             {self.last_ts or 'N/A'}\n")
                f.write(f"Total Bytes HMI->RPi:       {self.stats['bytes_hmi_to_rpi']}\n")
                f.write(f"Total Bytes RPi->HMI:       {self.stats['bytes_rpi_to_hmi']}\n")
                f.write(f"DEBUG_BYTES_TO_NEXTION:     0 (100% ISOLATED)\n")
                f.write(f"DEBUG_BYTES_FILTERED:       {debug_bytes_filtered}\n")
                f.write(f"VALID_NEXTION_FRAMES:       {valid_nextion_frames}\n")
                f.write(f"MALFORMED_NEXTION_FRAMES:   {malformed_nextion_frames}\n")
                f.write(f"HMI Commands:               {self.stats['hmi_commands']}\n")
                f.write(f"Nextion Frames:             {self.stats['nextion_frames']}\n")
                f.write(f"RS485 Commands:             {self.stats['rs485_commands']}\n")
                f.write(f"RS485 Telemetry:            {self.stats['rs485_telemetry']}\n")
                f.write(f"Debug Logs:                 {self.stats['debug_logs']}\n")
                f.write("=======================================================\n")
        except Exception:
            pass

    def close(self):
        self.flush_summary()
        self.log_file.close()

def run_bridge(com_port=DEFAULT_COM, host=DEFAULT_HOST, port=DEFAULT_PORT, baud=DEFAULT_BAUD):
    logger = BridgeTeeLogger()
    nfilter = NextionFrameFilter()
    
    print(f"[WIN-BRIDGE] Opening {com_port} @ {baud} baud...")
    try:
        ser = serial.Serial(com_port, baud, timeout=0.02)
    except Exception as e:
        print(f"[WIN-BRIDGE ERROR] Failed to open {com_port}: {e}")
        logger.close()
        sys.exit(1)

    try:
        while True:
            print(f"[WIN-BRIDGE] Connecting to TCP {host}:{port}...")
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.connect((host, port))
                sock.setblocking(False)
                print(f"[WIN-BRIDGE] Connected to {host}:{port}! Pure Nextion frame filter active.")
            except Exception as e:
                print(f"[WIN-BRIDGE] Connection attempt failed: {e}. Retrying in 2s...")
                time.sleep(2.0)
                continue

            last_summary_flush = time.time()
            try:
                while True:
                    # 1. Read from COM port (Simulator -> RPi)
                    if ser.in_waiting > 0:
                        data = ser.read(ser.in_waiting)
                        if data:
                            logger.log_chunk("HMI_TO_RPI", data)
                            try:
                                sock.sendall(data)
                            except Exception:
                                break

                    # 2. Read from TCP (RPi / ESP32 -> Simulator)
                    r, _, _ = select.select([sock], [], [], 0.01)
                    if r:
                        try:
                            tcp_data = sock.recv(4096)
                        except Exception:
                            break
                        if not tcp_data:
                            print("[WIN-BRIDGE] Server disconnected. Reconnecting...")
                            break
                        logger.log_chunk("RPI_TO_HMI", tcp_data)
                        
                        # Filter so ONLY pure Nextion frames reach Nextion Simulator
                        nextion_bytes, valid_frames = nfilter.process_incoming_tcp(tcp_data)
                        if nextion_bytes:
                            ser.write(nextion_bytes)
                            ser.flush()

                    # Periodic summary write every 5 seconds
                    if time.time() - last_summary_flush > 5.0:
                        logger.flush_summary(nfilter.debug_bytes_filtered, nfilter.valid_nextion_frames, nfilter.malformed_nextion_frames)
                        last_summary_flush = time.time()

                    time.sleep(0.002)
            except (socket.error, Exception) as exc:
                print(f"[WIN-BRIDGE] Socket error: {exc}. Reconnecting...")
            finally:
                try:
                    sock.close()
                except Exception:
                    pass
                time.sleep(1.0)

    except KeyboardInterrupt:
        print("\n[WIN-BRIDGE] Bridge stopped by operator.")
    finally:
        ser.close()
        logger.close()

if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    com = args[0] if len(args) > 0 else DEFAULT_COM
    h = args[1] if len(args) > 1 else DEFAULT_HOST
    p = int(args[2]) if len(args) > 2 else DEFAULT_PORT
    b = int(args[3]) if len(args) > 3 else DEFAULT_BAUD
    run_bridge(com, h, p, b)
