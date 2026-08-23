#!/usr/bin/env python3
"""
scripts/rpi_hmi_bridge.py - Non-blocking TCP Socket to Serial Port Bridge for Nextion HMI Simulator
Bridges Nextion Editor Simulator on Windows PC to the ESP32 physical HMI UART or USB-Serial port on Raspberry Pi.

Usage:
    python3 rpi_hmi_bridge.py [SERIAL_DEVICE] [BAUD_RATE] [TCP_PORT]
    e.g.
    python3 rpi_hmi_bridge.py /dev/ttyUSB0 9600 8888
"""
import sys
import socket
import select
import time
import serial

DEFAULT_DEVICE = "/dev/ttyUSB0"
DEFAULT_BAUD = 9600
DEFAULT_PORT = 8888

def main():
    device = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_DEVICE
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_BAUD
    port = int(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_PORT

    print(f"[BRIDGE] Opening serial port {device} @ {baud} baud...")
    try:
        ser = serial.Serial()
        ser.port = device
        ser.baudrate = baud
        ser.dtr = False
        ser.rts = False
        ser.timeout = 0.02
        ser.open()
        ser.dtr = False
        ser.rts = False
    except Exception as e:
        print(f"[BRIDGE ERROR] Failed to open serial port {device}: {e}")
        print("[BRIDGE ERROR] Check if USB-UART dongle is connected or if permissions are required (dialout group / sudo).")
        sys.exit(1)

    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_sock.bind(("0.0.0.0", port))
    server_sock.listen(5)
    print(f"[BRIDGE] TCP Server listening on 0.0.0.0:{port}. Waiting for Windows Nextion Simulator...")

    try:
        while True:
            client_sock, client_addr = server_sock.accept()
            print(f"[BRIDGE] Client connected from {client_addr}")
            client_sock.setblocking(False)

            try:
                while True:
                    # Serial -> TCP Socket (ESP32 Nextion output to Windows Simulator)
                    if ser.in_waiting > 0:
                        data = ser.read(ser.in_waiting)
                        if data:
                            try:
                                client_sock.sendall(data)
                            except BlockingIOError:
                                time.sleep(0.005)
                                client_sock.sendall(data)

                    # TCP Socket -> Serial (Windows Simulator touch events to ESP32)
                    r, _, _ = select.select([client_sock], [], [], 0.01)
                    if r:
                        tcp_data = client_sock.recv(4096)
                        if not tcp_data:
                            print("[BRIDGE] Client disconnected.")
                            break
                        ser.write(tcp_data)
                        ser.flush()
            except (ConnectionResetError, BrokenPipeError) as exc:
                print(f"[BRIDGE] Connection reset: {exc}")
            except Exception as exc:
                print(f"[BRIDGE] Connection error: {exc}")
            finally:
                client_sock.close()
    except KeyboardInterrupt:
        print("\n[BRIDGE] Shutting down bridge.")
    finally:
        ser.close()
        server_sock.close()

if __name__ == "__main__":
    main()
