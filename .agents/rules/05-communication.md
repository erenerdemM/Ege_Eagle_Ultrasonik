# RULE 05 — MULTI-DROP RS485 & UART PROTOCOL MATRIX

## Purpose
Enforce ASCII addressable multi-drop protocol framing, packet parsing, numerical clamping, and checksum verification across STM32 slave nodes and ESP32 master node.

## Scope
Applies to UART/RS485 communication drivers in `STM32/`, `esp32/`, and test mocks (`test_hil_uart.py`, `test_rs485_mock.py`).

## MUST
1. Enforce target node address parsing in format `T<ID>:<COMMAND>` (e.g. `T1:SET_PWR=50`, `T2:SET_TEMP=60.0`).
2. Telemetry output frames MUST follow strict ASCII comma-separated format:
   `STAT,<TankID>,<mode>,<remaining_sec>,<temp_x10>,<relay>,<power_pct>,<frequency_khz>,<fault_flags>,<prov_state>\n`
3. Apply strict parameter clamping:
   - Power (`PWR`): 0 to 100%
   - Set Temperature (`SET_TEMP`): 0.0 to 100.0°C
   - Process Timer (`TIMER`): 0 to 999 minutes
4. Validate frame line termination (`\r\n`) and maximum frame length (64 bytes).

## MUST NOT
1. DO NOT process malformed ASCII frames; discard corrupted bytes and increment error counter (`ERR`).
2. DO NOT allow multi-drop bus collisions; slave nodes MUST respond ONLY when explicitly addressed by their node ID.
3. DO NOT use dynamic string concatenation inside real-time UART TX interrupts.

## Verification
- Run `test_hil_uart.py` and `test_rs485_mock.py` to verify multi-drop packet matrix compliance.

## Escalation
If protocol framing requires new commands or breaking field changes, escalate to `system-architect` for a Protocol Version bump.
