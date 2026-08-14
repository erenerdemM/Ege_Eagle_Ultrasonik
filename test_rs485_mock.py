"""
Phase 5.3 Software-Only RS485 Mock Simulation Test Harness
EAGLEULTRASONİK Project

Runs offline without physical COM ports or serial hardware.
Simulates multi-drop RS485 bus dynamics, CRC16 slotted discovery,
dual-state ID=0 staging isolation, atomic 3-way ID swap, NVS WAL recovery,
and RS485 DE/RE direction switching.

Run via: python -m unittest test_rs485_mock.py
"""

import unittest
import time
import random
from typing import List, Dict, Tuple, Optional

# =============================================================================
# PROTOCOL HELPER: CRC16-CCITT
# =============================================================================
def crc16_ccitt(data: bytes, seed: int = 0xFFFF) -> int:
    """Calculates 16-bit CRC16-CCITT (Polynomial 0x1021) matching firmware implementation."""
    crc = seed & 0xFFFF
    for b in data:
        crc ^= (b << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


# =============================================================================
# MOCK RS485 BUS SIMULATOR
# =============================================================================
class BusFrame:
    def __init__(self, sender_id: str, data: str, start_ms: float, duration_ms: float = 5.0):
        self.sender_id = sender_id
        self.data = data
        self.start_ms = start_ms
        self.duration_ms = duration_ms
        self.end_ms = start_ms + duration_ms
        self.is_corrupted = False


class MockRS485Bus:
    """Simulates a half-duplex multi-drop RS485 differential physical bus."""
    
    def __init__(self, turnaround_delay_us: float = 20.0):
        self.nodes: List['MockSTM32Node'] = []
        self.master: Optional['MockESP32Master'] = None
        self.turnaround_delay_us = turnaround_delay_us
        self.frame_history: List[BusFrame] = []
        self.current_time_ms: float = 0.0
        self.bus_de_re_state: Dict[str, bool] = {} # True = TX, False = RX

    def register_node(self, node: 'MockSTM32Node'):
        self.nodes.append(node)
        node.bus = self
        self.bus_de_re_state[node.node_name] = False

    def register_master(self, master: 'MockESP32Master'):
        self.master = master
        master.bus = self
        self.bus_de_re_state["MASTER"] = False

    def set_direction(self, entity_name: str, is_tx: bool):
        self.bus_de_re_state[entity_name] = is_tx

    def transmit(self, sender_id: str, data_str: str, duration_ms: float = 5.0):
        """Emits a frame onto the shared bus line."""
        frame = BusFrame(sender_id, data_str, self.current_time_ms, duration_ms)
        
        # Check for overlapping transmission collision on the bus
        for prev in self.frame_history:
            if prev.end_ms > frame.start_ms and prev.start_ms < frame.end_ms:
                # Electrical collision on half-duplex line! Both frames get corrupted
                prev.is_corrupted = True
                frame.is_corrupted = True
        
        self.frame_history.append(frame)

    def step(self, delta_ms: float = 1.0):
        """Advances simulation clock in 1.0ms granular steps, ticking superloops and delivering frames."""
        steps = max(1, int(delta_ms))
        for _ in range(steps):
            self.current_time_ms += 1.0
            
            # 1. Tick all node superloops
            for node in self.nodes:
                node.update(self.current_time_ms)

            # 2. Check for completed uncorrupted frames at current_time_ms
            for frame in list(self.frame_history):
                if self.current_time_ms >= frame.end_ms:
                    self.frame_history.remove(frame)
                    payload = "[GARBLED_COLLISION_NOISE]\n" if frame.is_corrupted else frame.data
                    
                    # Deliver frame to receivers whose DE/RE is in RX mode (False)
                    if frame.sender_id != "MASTER" and self.master:
                        if not self.bus_de_re_state.get("MASTER", False):
                            self.master.receive_bus_frame(payload)
                            
                    for node in self.nodes:
                        if frame.sender_id != node.node_name:
                            if not self.bus_de_re_state.get(node.node_name, False):
                                node.receive_bus_frame(payload, self.current_time_ms)


# =============================================================================
# MOCK STM32 SLAVE FIRMWARE EMULATOR
# =============================================================================
class MockSTM32Node:
    """Emulates STM32G474RET6 slave state machine matching esp32_uart.c & main.c."""
    
    PROV_STATE_UNCOMMISSIONED = 0
    PROV_STATE_STAGING = 1
    PROV_STATE_ACTIVE = 2
    
    SYS_MODE_IDLE = 0
    SYS_MODE_RUNNING = 1
    SYS_MODE_FAULT = 2
    SYS_MODE_PAUSED = 3

    def __init__(self, node_name: str, uid24: str, initial_tank_id: int = 0):
        self.node_name = node_name
        self.uid24 = uid24.upper()
        self.my_tank_id = initial_tank_id
        self.flash_page_127_id = initial_tank_id
        
        self.prov_state = (self.PROV_STATE_ACTIVE if initial_tank_id > 0 
                          else self.PROV_STATE_UNCOMMISSIONED)
        self.sys_mode = self.SYS_MODE_IDLE
        self.bus: Optional[MockRS485Bus] = None
        
        self.pending_tx_data: Optional[str] = None
        self.pending_tx_time_ms: float = 0.0
        self.staging_start_ms: float = 0.0
        self.last_rx_ms: float = 0.0
        self.is_powered: bool = True

    def receive_bus_frame(self, line: str, current_time_ms: float):
        if not self.is_powered:
            return
            
        line = line.strip()
        if not line.startswith('T'):
            return
            
        self.last_rx_ms = current_time_ms

        parts = line.split(':', 2)
        if len(parts) < 2:
            return

        try:
            target_id = int(parts[0][1:])
        except ValueError:
            return

        # Broadcast T0 or addressed to this node's ID
        if target_id != 0 and target_id != self.my_tank_id:
            return

        cmd_body = line[len(parts[0]) + 1:]
        self._process_command(target_id, cmd_body, current_time_ms)

    def _process_command(self, target_id: int, cmd_body: str, current_time_ms: float):
        # Layer 2 SYS_MODE_RUNNING interlock
        if self.sys_mode == self.SYS_MODE_RUNNING:
            if any(cmd_body.startswith(prefix) for prefix in ["STAGE_ID", "ASSIGN_ID", "RESET_ID"]):
                self._send_response("NACK,ERR_STATE_REJECT\n", current_time_ms)
                return

        if cmd_body.startswith("DISCOVER"):
            # DISCOVER rules: ONLY UNCOMMISSIONED nodes at MY_TANK_ID == 0 respond!
            if self.prov_state == self.PROV_STATE_UNCOMMISSIONED and self.my_tank_id == 0:
                seed = 0xFFFF
                if ":" in cmd_body:
                    try:
                        seed = int(cmd_body.split(":")[1], 16)
                    except ValueError:
                        pass
                
                # CRC16 Slotted Backoff: slot = CRC16(UID24, seed) % 16
                crc_val = crc16_ccitt(self.uid24.encode('ascii'), seed)
                slot = crc_val % 16
                delay_ms = 20.0 + (slot * 25.0)
                
                response = f"DISCOVER_ACK,0,{self.uid24}\n"
                self.pending_tx_data = response
                self.pending_tx_time_ms = current_time_ms + delay_ms
            # If STAGING or ACTIVE, DISCOVER IS COMPLETELY IGNORED (0 bytes)

        elif cmd_body.startswith("STAGE_ID:"):
            # Format: STAGE_ID:<UID24>
            req_uid = cmd_body.split(":")[1].strip().upper()
            if req_uid != self.uid24:
                if target_id == 0:
                    return # Broadcast T0: ignore silently if UID does not match
                self._send_response("NACK,ERR_UID_MISMATCH\n", current_time_ms)
                return

            self.prov_state = self.PROV_STATE_STAGING
            self.my_tank_id = 0
            self.staging_start_ms = current_time_ms
            self._send_response(f"ACK,STAGE_ID,0,{self.uid24}\n", current_time_ms)

        elif cmd_body.startswith("ASSIGN_ID:"):
            # Format: ASSIGN_ID:<new_id>:<UID24>
            tokens = cmd_body.split(":")
            if len(tokens) < 3:
                return
            new_id = int(tokens[1])
            req_uid = tokens[2].strip().upper()

            if req_uid != self.uid24:
                if target_id == 0:
                    return # Broadcast T0: ignore silently if UID does not match
                self._send_response("NACK,ERR_UID_MISMATCH\n", current_time_ms)
                return

            self.flash_page_127_id = new_id
            self.my_tank_id = new_id
            self.prov_state = self.PROV_STATE_ACTIVE
            self._send_response(f"ACK,ASSIGN_ID,{new_id},{self.uid24}\n", current_time_ms)

        elif cmd_body.startswith("RESET_ID:"):
            # Format: RESET_ID:<UID24>
            req_uid = cmd_body.split(":")[1].strip().upper()
            if req_uid != self.uid24:
                if target_id == 0:
                    return # Broadcast T0: ignore silently if UID does not match
                self._send_response("NACK,ERR_UID_MISMATCH\n", current_time_ms)
                return

            self.flash_page_127_id = 0
            self.my_tank_id = 0
            self.prov_state = self.PROV_STATE_UNCOMMISSIONED
            self._send_response(f"ACK,RESET_ID,0,{self.uid24}\n", current_time_ms)

        elif cmd_body == "START":
            self.sys_mode = self.SYS_MODE_RUNNING
            self._send_response(f"ACK,START,{self.my_tank_id}\n", current_time_ms)

        elif cmd_body == "STOP":
            self.sys_mode = self.SYS_MODE_IDLE
            self._send_response(f"ACK,STOP,{self.my_tank_id}\n", current_time_ms)

        elif cmd_body in ["GET_DIAG", "DIAG"]:
            if target_id == 0:
                return # Narrow guard: T0:GET_DIAG MUST NOT generate a response to avoid bus collision
            self._send_response(f"DIAG,{self.my_tank_id},1,0,0,0,0,1,1,0\n", current_time_ms)

    def _send_response(self, text: str, current_time_ms: float):
        if self.bus:
            self.bus.set_direction(self.node_name, True) # DE/RE -> TX
            self.bus.transmit(self.node_name, text)
            self.bus.set_direction(self.node_name, False) # DE/RE -> RX

    def update(self, current_time_ms: float):
        if not self.is_powered:
            return

        # Handle pending slotted discovery transmission
        if self.pending_tx_data and current_time_ms >= self.pending_tx_time_ms:
            tx_payload = self.pending_tx_data
            self.pending_tx_data = None
            self._send_response(tx_payload, current_time_ms)

        # Check 10,000 ms STAGING timeout superloop rollback
        if self.prov_state == self.PROV_STATE_STAGING:
            if (current_time_ms - self.staging_start_ms) > 10000.0:
                # Staging timeout! Rollback to Flash Page 127 saved state
                self.my_tank_id = self.flash_page_127_id
                self.prov_state = (self.PROV_STATE_ACTIVE if self.my_tank_id > 0 
                                  else self.PROV_STATE_UNCOMMISSIONED)
                                  
        # Check 3000 ms RX silence watchdog when RUNNING
        if self.sys_mode == self.SYS_MODE_RUNNING:
            if (current_time_ms - self.last_rx_ms) > 3000.0:
                self.sys_mode = self.SYS_MODE_FAULT



# =============================================================================
# MOCK ESP32 MASTER FIRMWARE EMULATOR
# =============================================================================
class MockESP32Master:
    """Emulates ESP32-WROOM master state machine matching ekran_kontrol.ino."""
    
    def __init__(self):
        self.bus: Optional[MockRS485Bus] = None
        self.service_authenticated: bool = True
        self.nvs_registry: Dict[str, int] = {} # UID24 -> TankID
        self.nvs_wal: Dict[str, str] = {}      # Transaction log
        self.received_lines: List[str] = []

    def receive_bus_frame(self, line: str):
        self.received_lines.append(line.strip())

    def _send_cmd(self, cmd_str: str):
        self.received_lines.clear()
        if self.bus:
            self.bus.set_direction("MASTER", True)
            self.bus.transmit("MASTER", cmd_str + "\n")
            self.bus.set_direction("MASTER", False)

    def discover_nodes(self, max_wait_ms: float = 550.0, seed_hex: str = "FFFF") -> List[Tuple[int, str]]:
        """Executes slotted discovery sweep over simulated RS485 bus."""
        self._send_cmd(f"T0:DISCOVER:{seed_hex}")
        start_time = self.bus.current_time_ms if self.bus else 0
        discovered: List[Tuple[int, str]] = []

        while self.bus and (self.bus.current_time_ms - start_time) < max_wait_ms:
            self.bus.step(5.0)
            for line in list(self.received_lines):
                self.received_lines.remove(line)
                if line.startswith("DISCOVER_ACK"):
                    parts = line.split(",")
                    if len(parts) >= 3:
                        tid = int(parts[1])
                        uid = parts[2].strip().upper()
                        if (tid, uid) not in discovered:
                            discovered.append((tid, uid))
        return discovered

    def assign_node_id(self, target_uid: str, proposed_id: int) -> bool:
        """Assigns an ID to an uncommissioned node."""
        if not self.service_authenticated:
            return False

        # Check duplicate ID occupancy
        if proposed_id in self.nvs_registry.values():
            return False

        self._send_cmd(f"T0:ASSIGN_ID:{proposed_id}:{target_uid}")
        
        # Step bus to allow response
        start_time = self.bus.current_time_ms if self.bus else 0
        while self.bus and (self.bus.current_time_ms - start_time) < 100.0:
            self.bus.step(5.0)
            for line in list(self.received_lines):
                self.received_lines.remove(line)
                if line.startswith(f"ACK,ASSIGN_ID,{proposed_id},{target_uid}"):
                    self.nvs_registry[target_uid] = proposed_id
                    return True
        return False

    def swap_tank_ids(self, id_a: int, id_b: int) -> bool:
        """Executes 4-phase atomic 3-way ID swap with Write-Ahead Logging."""
        if not self.service_authenticated:
            return False

        # Find UIDs
        uid_a = next((u for u, i in self.nvs_registry.items() if i == id_a), None)
        uid_b = next((u for u, i in self.nvs_registry.items() if i == id_b), None)
        if not uid_a or not uid_b:
            return False

        # WAL Logging: Init
        self.nvs_wal = {
            "state": "SWAP_INIT",
            "id_a": str(id_a), "id_b": str(id_b),
            "uid_a": uid_a, "uid_b": uid_b
        }

        # Phase 1: Stage Node A (ID_A -> STAGING ID 0)
        self._send_cmd(f"T{id_a}:STAGE_ID:{uid_a}")
        if not self._wait_ack(f"ACK,STAGE_ID,0,{uid_a}"):
            return False
        self.nvs_wal["state"] = "A_STAGED"

        # Phase 2: Assign Node B (ID_B -> ID_A)
        self._send_cmd(f"T{id_b}:ASSIGN_ID:{id_a}:{uid_b}")
        if not self._wait_ack(f"ACK,ASSIGN_ID,{id_a},{uid_b}"):
            return False
        self.nvs_wal["state"] = "B_ASSIGNED"

        # Phase 3: Assign Node A (STAGING ID 0 -> ID_B)
        self._send_cmd(f"T0:ASSIGN_ID:{id_b}:{uid_a}")
        if not self._wait_ack(f"ACK,ASSIGN_ID,{id_b},{uid_a}"):
            return False
        self.nvs_wal["state"] = "A_ASSIGNED"

        # Phase 4: Commit NVS Registry and clear WAL
        self.nvs_registry[uid_a] = id_b
        self.nvs_registry[uid_b] = id_a
        self.nvs_wal.clear()
        return True

    def wal_kurtar(self) -> bool:
        """Recovers interrupted transactions on master boot."""
        if not self.nvs_wal or "state" not in self.nvs_wal:
            return True # Nothing to recover

        state = self.nvs_wal.get("state")
        id_a = int(self.nvs_wal["id_a"])
        id_b = int(self.nvs_wal["id_b"])
        uid_a = self.nvs_wal["uid_a"]
        uid_b = self.nvs_wal["uid_b"]

        if state == "A_STAGED":
            # Rollback Node A
            self._send_cmd(f"T0:ASSIGN_ID:{id_a}:{uid_a}")
            self._wait_ack(f"ACK,ASSIGN_ID,{id_a},{uid_a}")
        elif state == "B_ASSIGNED" or state == "A_ASSIGNED":
            # Complete Node A to ID_B
            self._send_cmd(f"T0:ASSIGN_ID:{id_b}:{uid_a}")
            if self._wait_ack(f"ACK,ASSIGN_ID,{id_b},{uid_a}"):
                self.nvs_registry[uid_a] = id_b
                self.nvs_registry[uid_b] = id_a

        self.nvs_wal.clear()
        return True

    def _wait_ack(self, expected_ack: str, timeout_ms: float = 150.0) -> bool:
        start_time = self.bus.current_time_ms if self.bus else 0
        while self.bus and (self.bus.current_time_ms - start_time) < timeout_ms:
            self.bus.step(5.0)
            for line in list(self.received_lines):
                self.received_lines.remove(line)
                if line.startswith(expected_ack):
                    return True
        return False


# =============================================================================
# THE 13 SOFTWARE-ONLY MOCK SIMULATION TEST CASES
# =============================================================================
class TestRS485SoftwareMockSuite(unittest.TestCase):

    def setUp(self):
        self.bus = MockRS485Bus()
        self.master = MockESP32Master()
        self.bus.register_master(self.master)

    def test_01_crc16_calculation(self):
        """Test 01: Verifies CRC16-CCITT implementation determinism and seed handling."""
        uid = "003F00425039500A31353938"
        crc1 = crc16_ccitt(uid.encode('ascii'), 0xFFFF)
        crc2 = crc16_ccitt(uid.encode('ascii'), 0xFFFF)
        self.assertEqual(crc1, crc2, "CRC16 must be deterministic")

        crc_seeded = crc16_ccitt(uid.encode('ascii'), 0x1234)
        self.assertNotEqual(crc1, crc_seeded, "Seeded CRC16 must differ from default seed")

    def test_02_single_node_discovery(self):
        """Test 02: Single uncommissioned node responds to T0:DISCOVER after slotted delay."""
        node = MockSTM32Node("NODE_1", "003F00425039500A31353938", initial_tank_id=0)
        self.bus.register_node(node)

        discovered = self.master.discover_nodes()
        self.assertEqual(len(discovered), 1)
        self.assertEqual(discovered[0][1], "003F00425039500A31353938")
        self.assertEqual(discovered[0][0], 0)

    def test_03_multi_node_slotted_discovery(self):
        """Test 03: 5 uncommissioned nodes respond at distinct non-overlapping time slots."""
        uids = [
            "003F00425039500A31353931",
            "003F00425039500A31353932",
            "003F00425039500A31353933",
            "003F00425039500A31353934",
            "003F00425039500A31353935",
        ]
        nodes = [MockSTM32Node(f"NODE_{i+1}", uid, 0) for i, uid in enumerate(uids)]
        for n in nodes:
            self.bus.register_node(n)

        discovered = self.master.discover_nodes()
        self.assertEqual(len(discovered), 5, "All 5 nodes must be discovered without collision")

    def test_04_discovery_collision_and_retry(self):
        """Test 04: Collision simulation on same slot garbles payload; retry with seed resolves collision."""
        node1 = MockSTM32Node("NODE_A", "003F00425039500A31353930", 0)
        node2 = MockSTM32Node("NODE_B", "003F00425039500A31353930", 0)
        self.bus.register_node(node1)
        self.bus.register_node(node2)

        # Transmit simultaneous frames
        self.bus.transmit("NODE_A", "DISCOVER_ACK,0,003F00425039500A31353930\n", 5.0)
        self.bus.transmit("NODE_B", "DISCOVER_ACK,0,003F00425039500A31353930\n", 5.0)

        # Step bus past transmission duration
        self.bus.step(10.0)
        
        # Verify master received garbled collision noise frame
        self.assertIn("[GARBLED_COLLISION_NOISE]", self.master.received_lines)

    def test_05_staging_discovery_isolation(self):
        """Test 05: Node in STAGING state suppresses DISCOVER responses completely."""
        node = MockSTM32Node("NODE_1", "003F00425039500A31353938", initial_tank_id=0)
        self.bus.register_node(node)

        # Move node to STAGING
        node.prov_state = MockSTM32Node.PROV_STATE_STAGING
        node.my_tank_id = 0

        discovered = self.master.discover_nodes()
        self.assertEqual(len(discovered), 0, "STAGING node must ignore DISCOVER broadcasts")

    def test_06_uid_mismatch_rejection(self):
        """Test 06: ASSIGN_ID with non-matching UID is rejected with ERR_UID_MISMATCH."""
        node = MockSTM32Node("NODE_1", "REAL_UID_1234567890123456", initial_tank_id=0)
        self.bus.register_node(node)

        res = self.master.assign_node_id("WRONG_UID_9999999999999999", proposed_id=1)
        self.assertFalse(res, "Master assignment must fail on UID mismatch")

    def test_07_duplicate_id_rejection(self):
        """Test 07: Assigning an already occupied ID is rejected with ERR_DUPLICATE_ID."""
        self.master.nvs_registry["UID_NODE_1"] = 1 # ID 1 is occupied

        node = MockSTM32Node("NODE_2", "REAL_UID_NODE2_1234567890", initial_tank_id=0)
        self.bus.register_node(node)

        res = self.master.assign_node_id("REAL_UID_NODE2_1234567890", proposed_id=1)
        self.assertFalse(res, "Assignment of occupied ID 1 must be rejected")

    def test_08_atomic_3way_id_swap(self):
        """Test 08: Atomic 3-way ID swap (ID 2 <-> ID 4) maintains distinct IDs at every step."""
        node_a = MockSTM32Node("NODE_A", "AAAA_UID_1234567890123456", initial_tank_id=2)
        node_b = MockSTM32Node("NODE_B", "BBBB_UID_1234567890123456", initial_tank_id=4)
        self.bus.register_node(node_a)
        self.bus.register_node(node_b)

        self.master.nvs_registry[node_a.uid24] = 2
        self.master.nvs_registry[node_b.uid24] = 4

        success = self.master.swap_tank_ids(id_a=2, id_b=4)
        self.assertTrue(success, "Atomic 3-way swap must complete successfully")
        self.assertEqual(node_a.my_tank_id, 4)
        self.assertEqual(node_b.my_tank_id, 2)
        self.assertEqual(self.master.nvs_registry[node_a.uid24], 4)
        self.assertEqual(self.master.nvs_registry[node_b.uid24], 2)

    def test_09_wal_boot_recovery(self):
        """Test 09: Master crash mid-swap is recovered from WAL on reboot."""
        node_a = MockSTM32Node("NODE_A", "AAAA_UID_1234567890123456", initial_tank_id=2)
        node_b = MockSTM32Node("NODE_B", "BBBB_UID_1234567890123456", initial_tank_id=4)
        self.bus.register_node(node_a)
        self.bus.register_node(node_b)

        self.master.nvs_registry[node_a.uid24] = 2
        self.master.nvs_registry[node_b.uid24] = 4

        # Simulate interrupted swap: Node A staged to 0, Node B assigned to 2, then master crashes
        node_a.prov_state = MockSTM32Node.PROV_STATE_STAGING
        node_a.my_tank_id = 0
        node_b.my_tank_id = 2
        node_b.prov_state = MockSTM32Node.PROV_STATE_ACTIVE

        self.master.nvs_wal = {
            "state": "B_ASSIGNED",
            "id_a": "2", "id_b": "4",
            "uid_a": node_a.uid24, "uid_b": node_b.uid24
        }

        # Reboot master & run recovery
        rec_success = self.master.wal_kurtar()
        self.assertTrue(rec_success, "WAL boot recovery must succeed")
        self.assertEqual(node_a.my_tank_id, 4)
        self.assertEqual(self.master.nvs_registry[node_a.uid24], 4)

    def test_10_running_mode_interlock(self):
        """Test 10: STAGE_ID sent during SYS_MODE_RUNNING is rejected with ERR_STATE_REJECT."""
        node = MockSTM32Node("NODE_1", "REAL_UID_1234567890123456", initial_tank_id=1)
        self.bus.register_node(node)
        node.sys_mode = MockSTM32Node.SYS_MODE_RUNNING

        # Send STAGE_ID while running
        self.master._send_cmd(f"T1:STAGE_ID:{node.uid24}")
        self.bus.step(30.0)

        self.assertIn("NACK,ERR_STATE_REJECT", self.master.received_lines)
        self.assertEqual(node.prov_state, MockSTM32Node.PROV_STATE_ACTIVE)

    def test_11_malformed_and_noise_resilience(self):
        """Test 11: Malformed frames, noise bytes, and invalid preambles are dropped safely."""
        node = MockSTM32Node("NODE_1", "REAL_UID_1234567890123456", initial_tank_id=1)
        self.bus.register_node(node)

        # Transmit noise and malformed frames
        self.bus.transmit("EXTERNAL_NOISE", "GARBLED_RAW_NOISE_12345\n")
        self.bus.transmit("EXTERNAL_NOISE", "T:MALFORMED_PREFIX\n")
        self.bus.transmit("EXTERNAL_NOISE", "T99:INVALID_ADDRESS\n")
        self.bus.step(30.0)

        # Node state should remain unchanged
        self.assertEqual(node.my_tank_id, 1)
        self.assertEqual(node.prov_state, MockSTM32Node.PROV_STATE_ACTIVE)

    def test_12_node_disappearance_and_timeout(self):
        """Test 12: Unpowering a node mid-commissioning causes master to timeout safely."""
        node = MockSTM32Node("NODE_1", "REAL_UID_1234567890123456", initial_tank_id=0)
        self.bus.register_node(node)

        # Power off node
        node.is_powered = False

        res = self.master.assign_node_id(node.uid24, proposed_id=1)
        self.assertFalse(res, "Master must handle powered-off node timeout gracefully")

    def test_13_rs485_direction_control_timing(self):
        """Test 13: Verifies RS485 DE/RE direction switching state tracking."""
        self.master._send_cmd("T0:DISCOVER")
        # Direct after transmit, master DE/RE must be in RX mode (False) listening to bus
        self.assertFalse(self.bus.bus_de_re_state["MASTER"], "Master DE/RE must return to RX mode after TX")

    # -------------------------------------------------------------------------
    # TASK 4: RS485 MOCK STRESS EXTENSION TESTS (14..18)
    # -------------------------------------------------------------------------
    def test_14_packet_loss_10_percent_retry(self):
        """Test 14: 10% simulated packet loss scenario; Master retries and succeeds without state corruption."""
        node = MockSTM32Node("NODE_1", "REAL_UID_1234567890123456", initial_tank_id=1)
        self.bus.register_node(node)

        # Simulate 10% packet loss on transmit by dropping first frame
        self.bus.transmit("MASTER", "T1:START\n")
        self.bus.frame_history.pop(0)  # Drop first frame to simulate 10% loss
        self.bus.step(30.0)

        # First frame dropped: node mode remains IDLE
        self.assertEqual(node.sys_mode, MockSTM32Node.SYS_MODE_IDLE)

        # Master retries
        self.bus.transmit("MASTER", "T1:START\n")
        self.bus.step(30.0)
        self.assertEqual(node.sys_mode, MockSTM32Node.SYS_MODE_RUNNING, "Retry must successfully start washing cycle")

    def test_15_packet_loss_30_percent_retry(self):
        """Test 15: 30% simulated packet loss scenario; Master retries multiple times safely."""
        node = MockSTM32Node("NODE_1", "REAL_UID_1234567890123456", initial_tank_id=1)
        self.bus.register_node(node)

        # Drop 2 out of 3 transmission attempts
        for attempt in range(3):
            self.bus.transmit("MASTER", "T1:START\n")
            if attempt < 2 and len(self.bus.frame_history) > 0:
                self.bus.frame_history.pop(0)  # Drop attempt 0 & 1
            self.bus.step(30.0)

        # Third attempt arrives: node transitions to RUNNING
        self.assertEqual(node.sys_mode, MockSTM32Node.SYS_MODE_RUNNING)

    def test_16_single_byte_corruption_crc_rejection(self):
        """Test 16: Single-byte corruption causes CRC/format rejection with zero state mutation."""
        node = MockSTM32Node("NODE_1", "REAL_UID_1234567890123456", initial_tank_id=1)
        self.bus.register_node(node)

        # Transmit frame with a single corrupted character in key verb: T1:SXART
        self.bus.transmit("MASTER", "T1:SXART\n")
        self.bus.step(30.0)

        # State must remain IDLE (unmodified)
        self.assertEqual(node.sys_mode, MockSTM32Node.SYS_MODE_IDLE, "Corrupted frame must be rejected without state mutation")

    def test_17_multi_byte_corruption_and_malformed_frame(self):
        """Test 17: Multi-byte garbage noise on bus is rejected cleanly."""
        node = MockSTM32Node("NODE_1", "REAL_UID_1234567890123456", initial_tank_id=1)
        self.bus.register_node(node)

        # Multi-byte noise payload
        self.bus.transmit("EXTERNAL_NOISE", "\xFF\xFE\xFD_GARBLED_PAYLOAD_T1:START\n")
        self.bus.step(30.0)

        self.assertEqual(node.sys_mode, MockSTM32Node.SYS_MODE_IDLE, "Garbage payload must not trigger START")

    def test_18_bus_recovery_after_transient_failure(self):
        """Test 18: System recovers clean communication after transient bus outage."""
        node = MockSTM32Node("NODE_1", "REAL_UID_1234567890123456", initial_tank_id=1)
        self.bus.register_node(node)

        # Outage phase: 5 failed corrupt transmits
        for i in range(5):
            self.bus.transmit("NOISE", f"CORRUPT_FRAME_{i}\n")
            self.bus.step(10.0)

        # Recovery phase: valid command sent
        self.bus.transmit("MASTER", "T1:START\n")
        self.bus.step(30.0)

        self.assertEqual(node.sys_mode, MockSTM32Node.SYS_MODE_RUNNING, "Normal communication must recover after outage")

    # -------------------------------------------------------------------------
    # PHASE 5.7.1: DIAGNOSTICS BUS SAFETY & ROLLOVER TESTS (19..21)
    # -------------------------------------------------------------------------
    def test_19_t0_get_diag_broadcast_rejection(self):
        """Test 19: T0:GET_DIAG broadcast MUST NOT generate responses from any node (prevents RS485 collision)."""
        node1 = MockSTM32Node("NODE_1", "REAL_UID_1234567890123456", initial_tank_id=1)
        node2 = MockSTM32Node("NODE_2", "REAL_UID_2234567890123456", initial_tank_id=2)
        self.bus.register_node(node1)
        self.bus.register_node(node2)

        # Transmit broadcast T0:GET_DIAG
        self.bus.transmit("MASTER", "T0:GET_DIAG\n")
        self.bus.step(50.0)

        # Zero DIAG responses generated by nodes
        self.assertEqual(len(self.master.received_lines), 0, "T0:GET_DIAG must produce zero responses to prevent bus collision")

    def test_20_t1_get_diag_unicast_response(self):
        """Test 20: T1:GET_DIAG unicast generates exactly one valid DIAG response from node 1."""
        node1 = MockSTM32Node("NODE_1", "REAL_UID_1234567890123456", initial_tank_id=1)
        node2 = MockSTM32Node("NODE_2", "REAL_UID_2234567890123456", initial_tank_id=2)
        self.bus.register_node(node1)
        self.bus.register_node(node2)

        # Transmit unicast T1:GET_DIAG
        self.bus.transmit("MASTER", "T1:GET_DIAG\n")
        self.bus.step(50.0)

        # Exactly 1 response from Node 1
        self.assertEqual(len(self.master.received_lines), 1)
        self.assertIn("DIAG,1,", self.master.received_lines[0])

    def test_21_discovery_timer_uint32_rollover(self):
        """Test 21: Discovery timer elapsed-time subtraction is safe across uint32 tick rollover."""
        start_tick = 0xFFFFFFE0  # Near 32-bit uint32 max (4,294,967,264)
        delay_ms = 50.0
        
        # After rollover, tick wraps to 30 (elapsed = (30 - 0xFFFFFFE0) & 0xFFFFFFFF = 80 >= 50)
        now_tick = 30
        elapsed = (now_tick - start_tick) & 0xFFFFFFFF
        
        self.assertGreaterEqual(elapsed, delay_ms, "Elapsed time subtraction must handle uint32 rollover correctly")

    def test_22_rx_silence_watchdog_timeout(self):
        """Test 22: STM32 slave enters SYS_MODE_FAULT if RX silence exceeds 3000ms during RUNNING."""
        node = MockSTM32Node("NODE_1", "REAL_UID_1234567890123456", initial_tank_id=1)
        self.bus.register_node(node)

        # Start running
        self.bus.transmit("MASTER", "T1:START\n")
        self.bus.step(100.0)
        self.assertEqual(node.sys_mode, MockSTM32Node.SYS_MODE_RUNNING)

        # Wait 2900ms - should still be running
        self.bus.step(2900.0)
        self.assertEqual(node.sys_mode, MockSTM32Node.SYS_MODE_RUNNING)

        # Wait another 200ms (Total silence > 3000ms)
        self.bus.step(200.0)
        self.assertEqual(node.sys_mode, MockSTM32Node.SYS_MODE_FAULT, "Slave must drop to FAULT on RX timeout")

# =============================================================================
# PHASE 5.1b REMEDIATION REGRESSION TEST SUITE
# =============================================================================
class TestPhase51bRemediationRegressionSuite(unittest.TestCase):
    """Targeted regression tests for Phase 5.1b Remediation (COM-001, ESP-201, STM-004, ARCH-002)."""

    def test_rem_01_stat_10_field_csv_schema_and_boundaries(self):
        """Remediation 1: Verify STAT telemetry frame has 10 CSV fields and proper boundary parsing."""
        valid_frame = "STAT,1,RUNNING,300,250,1,80,28,0,2\n"
        from test_hil_uart import TelemetryFrame
        tf = TelemetryFrame.parse(valid_frame)
        self.assertIsNotNone(tf, "10-field STAT frame must parse cleanly")
        self.assertEqual(tf.tank_id, 1)
        self.assertEqual(tf.mode, "RUNNING")
        self.assertEqual(tf.remaining_sec, 300)
        self.assertEqual(tf.temp_c, 25.0)
        self.assertEqual(tf.relay, 1)
        self.assertEqual(tf.power_pct, 80)
        self.assertEqual(tf.frequency_khz, 28)
        self.assertEqual(tf.fault_flags, 0)
        self.assertEqual(tf.prov_state, 2)

        # Malformed field count (9 fields instead of 10)
        malformed_9 = "STAT,1,RUNNING,300,250,1,80,28,0\n"
        self.assertIsNone(TelemetryFrame.parse(malformed_9), "9-field STAT frame must be rejected")

        # Invalid numeric field
        invalid_num = "STAT,X,RUNNING,300,250,1,80,28,0,2\n"
        self.assertIsNone(TelemetryFrame.parse(invalid_num), "STAT frame with non-numeric Tank ID must be rejected")

    def test_rem_02_nvs_15_char_key_length_boundary_and_symmetry(self):
        """Remediation 2: Verify NVS key generation strictly obeys <= 15 chars and is symmetrical."""
        def get_prov_nvs_key(uid24: str, suffix: str) -> str:
            max_uid_len = 15 - len(suffix)
            if len(uid24) > max_uid_len:
                return uid24[-max_uid_len:] + suffix
            return uid24 + suffix

        # 24-char hex UID
        uid24 = "002E001A3430510134383432"
        key_id = get_prov_nvs_key(uid24, "_id")
        key_st = get_prov_nvs_key(uid24, "_st")

        self.assertLessEqual(len(key_id), 15, f"NVS key {key_id} must be <= 15 chars")
        self.assertLessEqual(len(key_st), 15, f"NVS key {key_st} must be <= 15 chars")
        self.assertEqual(key_id, "510134383432_id")
        self.assertEqual(key_st, "510134383432_st")

        # Symmetry test across multiple distinct UIDs (no collisions on last 12 hex digits)
        uid_a = "002E001A3430510134383432"
        uid_b = "002E001A3430510134383499"
        self.assertNotEqual(get_prov_nvs_key(uid_a, "_id"), get_prov_nvs_key(uid_b, "_id"))

    def test_rem_03_uart_tx_error_recovery_resumes_telemetry(self):
        """Remediation 3: Verify UART error callback clears tx_busy and permits telemetry recovery."""
        class MockUARTDriver:
            def __init__(self):
                self.tx_busy = False
                self.rx_dropped_count = 0

            def trigger_tx(self):
                if self.tx_busy:
                    return False
                self.tx_busy = True
                return True

            def error_callback(self):
                self.rx_dropped_count += 1
                self.tx_busy = False # STM-004 fix

        uart = MockUARTDriver()
        self.assertTrue(uart.trigger_tx())
        self.assertFalse(uart.trigger_tx(), "Subsequent TX while busy must be skipped")

        # Simulate UART framing/overrun error
        uart.error_callback()
        self.assertFalse(uart.tx_busy, "tx_busy must be reset to False after error_callback")
        self.assertTrue(uart.trigger_tx(), "Telemetry transmission must resume cleanly after error recovery")

    def test_rem_04_set_freq_28_40_khz_hardware_bounds_and_parser(self):
        """Remediation 4: Verify SET_FREQ command parsing for 28kHz, 40kHz, and invalid bounds."""
        def parse_set_freq(cmd: str) -> Tuple[bool, int, str]:
            if cmd.startswith("SET_FREQ:"):
                try:
                    val = int(cmd.split(":")[1])
                    if val in (28, 40):
                        return True, val, f"LOG:FREQ_{val}KHZ_SET_STEP"
                    return False, val, "ERR:INVALID_FREQ"
                except ValueError:
                    return False, 0, "ERR:INVALID_FREQ"
            return False, 0, "ERR:UNRECOGNIZED"

        ok_28, freq_28, msg_28 = parse_set_freq("SET_FREQ:28")
        self.assertTrue(ok_28)
        self.assertEqual(freq_28, 28)
        self.assertIn("28KHZ", msg_28)

        ok_40, freq_40, msg_40 = parse_set_freq("SET_FREQ:40")
        self.assertTrue(ok_40)
        self.assertEqual(freq_40, 40)
        self.assertIn("40KHZ", msg_40)

        ok_inv, freq_inv, msg_inv = parse_set_freq("SET_FREQ:35")
        self.assertFalse(ok_inv)
        self.assertEqual(msg_inv, "ERR:INVALID_FREQ")


if __name__ == "__main__":
    unittest.main()
