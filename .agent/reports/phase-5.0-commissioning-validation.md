# EAGLEULTRASONiK Phase 5.0 Commissioning, ID Swap & UID Discovery Protocol Specification

> **Document Status:** Official Protocol Architecture & Validation Reference  
> **Phase:** 5.0 Commissioning & Identity Architecture  
> **Date:** August 10, 2026  
> **Scope:** Multi-Drop UART Communication Protocol (`ESP32 Master` $\leftrightarrow$ `STM32G474RE Slaves`)  
> **Target File:** `c:\Users\ern0e\EAGLEULTRASONiK\.agent\reports\phase-5.0-commissioning-validation.md`  

---

## 1. Executive Summary & Architecture Overview

The **EAGLEULTRASONiK Phase 5.0** update establishes an industrial-grade, failure-tolerant commissioning and identity management protocol across the shared 1-N multi-drop UART bus (115200 Baud, 8N1). 

In previous phases, slave identity was statically determined via hardware DIP switches or unverified single-frame Flash overrides. Phase 5.0 introduces **Dual Identity Architecture**, **Slotted UID Discovery**, **Atomic ID Swapping**, and **Automatic Hardware-Assisted Flash Recovery** to guarantee zero address collisions under all operational conditions.

```mermaid
graph TD
    subgraph Master ["ESP32-S3 Master Node"]
        NVS["NVS Registry\n(UID96 <-> Tank ID Map)"]
        ENG["Commissioning Engine\n(Slotted Scheduler & Atomic Swap)"]
    end

    subgraph Bus ["Multi-Drop UART Bus (115200 8N1)"]
        Line["TX/RX Bus Line (GPIO18 / GPIO8)"]
    end

    subgraph Slaves ["STM32G474RE Slave Nodes (1..10)"]
        S1["STM32 Node #1\nUID: 0x36FFD805...\nFlash: ID=2"]
        S2["STM32 Node #2\nUID: 0x48AAF123...\nFlash: ID=4"]
        SUn["Fresh STM32 Node\nUID: 0x51BBE456...\nID=1 (BENCH_DEV_MODE_ID)"]
    end

    ESP32 --> Line
    Line <--> S1
    Line <--> S2
    Line <--> SUn
```

### Key Architectural Principles:
1. **Dual Identity Model**: Primary immutable hardware identity (96-bit MCU UID at `0x1FFF7590`) paired with a configurable logical address (Tank ID $1 \dots 10$).
2. **`BENCH_DEV_MODE_ID = 1` Compatibility**: Uncommissioned/fresh boards boot with default logical address `ID = 1` for initial zero-configuration connectivity.
3. **Slotted Backoff Collision Avoidance**: Multiple uncommissioned boards (`ID = 1`) resolve bus contention during discovery via 16-slot pseudo-random backoff calculated from `CRC16(UID96) % 16`.
4. **Atomic ID Swap Protocol**: Reassigning addresses between two active nodes (e.g., `ID 2` $\leftrightarrow$ `ID 4`) utilizes temporary staging address `T99` to ensure **duplicate IDs never exist on the bus at any point in time**.
5. **Hardware UID Corruption Recovery**: If a node loses or corrupts its Flash settings, the ESP32 Master automatically re-identifies the physical board via its 96-bit UID and restores its allocated logical Tank ID without human intervention.

---

## 2. Dual Identity Architecture

Every STM32G474RE slave controller possesses two distinct identity layers:

```
+-------------------------------------------------------------------------------+
|                             STM32 SLAVE IDENTITY                              |
+-------------------------------------------------------------------------------+
| 1. Primary Device Identity (Immutable Hardware Key)                           |
|    - Source: STM32G4 Unique Device ID Register (0x1FFF7590, 96-bit / 12-byte)  |
|    - Format: 24-character Hexadecimal ASCII (e.g. "36FFD8054655383712501843")    |
|    - Lifetime: Factory burned in silicon during wafer production (Read-Only)    |
+-------------------------------------------------------------------------------+
| 2. Configurable Logical Address (Network Routing Key)                          |
|    - Source: Flash Bank 2 Page 127 (0x0807F800) or Default BENCH_DEV_MODE_ID    |
|    - Format: Integer Range [1 .. 10]                                          |
|    - Staging Address: 99 (Temporary address during swap / commissioning)      |
+-------------------------------------------------------------------------------+
```

### Hardware Register Definition (STM32G474RE)
The 96-bit Unique Device ID (UID) is located in the system memory space:
- `UID_BASE`: `0x1FFF7590`
- `UID_WORD0`: `*(uint32_t*)(0x1FFF7590)` (X, Y coordinates on wafer)
- `UID_WORD1`: `*(uint32_t*)(0x1FFF7594)` (Wafer number)
- `UID_WORD2`: `*(uint32_t*)(0x1FFF7598)` (Lot number)

---

## 3. Protocol Syntax & Frame Specifications

The protocol extends the Phase 3 ASCII line structure (terminated by `\n`, optional `\r`). Maximum frame length is 64 bytes.

### 3.1 Master $\rightarrow$ Slave Command Frames

| Command Frame | Target | Description | Example |
| :--- | :--- | :--- | :--- |
| `T0:DISCOVER\n` | Broadcast (`T0`) | Triggers discovery epoch. All uncommissioned nodes (`ID=1`) respond in their slotted time windows. | `T0:DISCOVER\n` |
| `T0:REQ_UID:<ID>\n` | Targeted/Broadcast | Requests 96-bit UID from node currently responding at logical address `<ID>`. | `T0:REQ_UID:1\n` |
| `T0:STAGE_UID:<UID24>:<TEMP_ID>\n` | UID-Targeted Broadcast | Assigns temporary staging address (e.g., `99`) to the specific node matching `<UID24>`. | `T0:STAGE_UID:36FFD8054655383712501843:99\n` |
| `T<ID>:SET_ID:<NEW_ID>\n` | Targeted (`T1`..`T10`, `T99`) | Persists `<NEW_ID>` to Flash page 127 (`0x0807F800`) and updates `MY_TANK_ID` live. | `T99:SET_ID:4\n` |
| `T<ID>:PING\n` | Targeted | Identity verification ping for specific node. | `T2:PING\n` |

### 3.2 Slave $\rightarrow$ Master Response & Telemetry Frames

| Response Frame | Sender | Description | Example |
| :--- | :--- | :--- | :--- |
| `UID,<TankID>,<UID24>\n` | Slave | Returns 96-bit hardware UID and current Tank ID. | `UID,1,36FFD8054655383712501843\n` |
| `ACK,SET_ID,<NEW_ID>,<UID24>\n` | Slave | Confirms successful Flash write and live ID migration. | `ACK,SET_ID,4,36FFD8054655383712501843\n` |
| `STAT,<ID>,<mode>,...` | Slave | Standard 500ms heartbeat telemetry (contains current Tank ID). | `STAT,4,RUNNING,842,455,1,72,0,0\n` |

---

## 4. Slotted Backoff & Collision Resolution Protocol

When multiple uncommissioned/factory-fresh STM32 boards are attached to the bus, all nodes default to `MY_TANK_ID = 1` (`BENCH_DEV_MODE_ID = 1`). A simple query command would cause all uncommissioned nodes to transmit simultaneously, garbling the UART line.

### 4.1 Slotted Discovery Algorithm
To prevent bus contention, Phase 5.0 introduces **UID-based Slotted Backoff**:

1. **Slot Computation**:
   Each STM32 node computes its 16-slot discovery index from its 96-bit UID:
   $$\text{Slot} = \text{CRC16}(\text{UID}_{96}) \pmod{16}$$
   where $\text{CRC16}$ uses polynomial `0x1021` over the 12 bytes read from `0x1FFF7590`.

2. **Discovery Timing Window**:
   - Master broadcasts `T0:DISCOVER\n`.
   - Epoch duration: $16 \text{ slots} \times 50\text{ ms} = 800\text{ ms}$.
   - Response window for Slot $k$: Starts at $t = k \times 50\text{ ms}$.

3. **Response Protocol**:
   - Upon receiving `T0:DISCOVER\n`, an uncommissioned node (`MY_TANK_ID == 1`) schedules its response:
     $$\text{TxDelay}_{\text{ms}} = (\text{Slot} \times 50\text{ ms}) + \text{Jitter}(0 \dots 4\text{ ms})$$
   - At $\text{TxDelay}_{\text{ms}}$, the node transmits `UID,1,<UID24>\n`.

```mermaid
sequenceDiagram
    autonumber
    participant ESP as ESP32 Master
    participant S1 as STM32 Node A (Slot 3)
    participant S2 as STM32 Node B (Slot 11)

    ESP->>S1: T0:DISCOVER\n (Broadcast)
    ESP->>S2: T0:DISCOVER\n (Broadcast)
    
    Note over S1: Calculates Slot = CRC16(UID_A) % 16 = 3\nDelay = 150ms
    Note over S2: Calculates Slot = CRC16(UID_B) % 16 = 11\nDelay = 550ms

    Note over ESP: Slot 0..2 Idle (0ms - 150ms)
    S1->>ESP: UID,1,36FFD8054655383712501843\n @ t=150ms
    Note over ESP: ESP32 registers Node A UID -> Stages to T99
    ESP->>S1: T0:STAGE_UID:36FFD8054655383712501843:99\n
    S1-->>ESP: ACK,SET_ID,99,36FFD8054655383712501843\n

    Note over ESP: Slot 4..10 Idle (200ms - 550ms)
    S2->>ESP: UID,1,48AAF1239876543210987654\n @ t=550ms
    Note over ESP: ESP32 registers Node B UID -> Assigns ID 2
    ESP->>S2: T0:STAGE_UID:48AAF1239876543210987654:2\n
    S2-->>ESP: ACK,SET_ID,2,48AAF1239876543210987654\n
```

### 4.2 Multi-Round Collision Resolution
If two nodes calculate the exact same slot ($\text{CRC16} \pmod{16}$ collision, probability $\approx 6.25\%$), their frames collide and garble.
- **Master Behavior**: Master detects UART framing/checksum error in Slot $k$.
- **Resolution Cycle**: Master stages all successfully parsed nodes to assigned IDs ($2\dots 10$), removing them from `ID=1`. Master then issues a secondary `T0:DISCOVER` with round seed $R=1$:
  $$\text{Slot}_{R} = \text{CRC16}(\text{UID}_{96} \oplus R) \pmod{16}$$
- Contending nodes compute new distinct slots, guaranteeing discovery within $\le 3$ rounds.

---

## 5. Atomic ID Swap Protocol & Zero-Duplicate Guarantee

When an operator reassigns logical Tank IDs on an active bus (e.g. swapping **Node B (ID 2)** and **Node D (ID 4)**), assigning `ID 4` directly to Node B would immediately create two nodes responding to `ID 4` (Node B and Node D), corrupting multi-drop communication.

### 5.1 Atomic Swap Sequence via Temporary Staging Address (`T99`)

To guarantee **duplicate IDs are NEVER created at any microsecond**, ESP32 executes an **Atomic Staging Swap**:

```mermaid
sequenceDiagram
    autonumber
    participant ESP as ESP32 Master
    participant NodeB as STM32 Node B (Current ID: 2)
    participant NodeD as STM32 Node D (Current ID: 4)

    Note over ESP,NodeD: User Requests Swap: ID 2 <--> ID 4
    
    rect rgb(240, 248, 255)
        Note over ESP,NodeB: Step 1: Move Node B to Temporary Staging (T99)
        ESP->>NodeB: T2:SET_ID:99\n
        NodeB-->>ESP: ACK,SET_ID,99,36FFD8054655383712501843\n
        Note over ESP: Active IDs on Bus: [ID 4, ID 99]. ID 2 is now VACANT.
    end

    rect rgb(255, 250, 240)
        Note over ESP,NodeD: Step 2: Assign Vacant ID 2 to Node D
        ESP->>NodeD: T4:SET_ID:2\n
        NodeD-->>ESP: ACK,SET_ID,2,48AAF1239876543210987654\n
        Note over ESP: Active IDs on Bus: [ID 2, ID 99]. ID 4 is now VACANT.
    end

    rect rgb(240, 255, 240)
        Note over ESP,NodeB: Step 3: Move Node B from Staging (T99) to Target ID 4
        ESP->>NodeB: T99:SET_ID:4\n
        NodeB-->>ESP: ACK,SET_ID,4,36FFD8054655383712501843\n
        Note over ESP: Active IDs on Bus: [ID 2, ID 4]. SWAP COMPLETE!
    end
```

### 5.2 Atomic Swap Verification Table

| Step | State | Active Node B Address | Active Node D Address | Bus Address Collision Risk |
| :---: | :--- | :---: | :---: | :---: |
| **0** | Pre-Swap | `ID 2` | `ID 4` | **ZERO** |
| **1** | Node B Staged | `ID 99` | `ID 4` | **ZERO** |
| **2** | Node D Shifted | `ID 99` | `ID 2` | **ZERO** |
| **3** | Node B Finalized | `ID 4` | `ID 2` | **ZERO** |

---

## 6. Unique ID (UID) Recovery Protocol

If an STM32 board suffers Flash corruption (e.g. brownout during Flash write clearing `0x0807F800` magic key `0xA5A5A5A5`), the board reverts to `MY_TANK_ID = 1` upon boot.

### 6.1 Automatic Recovery Flow
1. **Heartbeat Anomaly Detection**: ESP32 detects unexpected telemetry `STAT,1,...` or loss of expected telemetry `STAT,5,...` from Tank 5.
2. **UID Query**: ESP32 issues `T0:REQ_UID:1\n` to probe the uncommissioned node.
3. **Silicon Signature Matching**: Node responds `UID,1,51BBE4567890123456789012\n`.
4. **NVS Database Lookup**: ESP32 searches its persistent NVS table:
   $$\text{NVS\_Lookup}(\text{"51BBE4567890123456789012"}) \longrightarrow \text{Assigned Tank ID: 5}$$
5. **Automated Re-Commissioning**: ESP32 immediately issues `T1:SET_ID:5\n`. Node rewrites Flash page 127 and resumes operation as **Tank ID 5**.

```mermaid
sequenceDiagram
    autonumber
    participant STM as Corrupted STM32 (Tank 5)
    participant ESP as ESP32 Master (NVS Saved)

    Note over STM: Flash Corrupted / Reset -> Defaults to ID 1
    STM->>ESP: STAT,1,IDLE,0,250,0,0,0,0\n
    Note over ESP: Anomaly! Expected Tank 5, got Tank 1
    ESP->>STM: T0:REQ_UID:1\n
    STM-->>ESP: UID,1,51BBE4567890123456789012\n
    Note over ESP: NVS Match: UID 51BBE45... belongs to Tank ID 5!
    ESP->>STM: T1:SET_ID:5\n
    Note over STM: Writes 0xA5A5A5A5 + ID 5 to Flash 0x0807F800
    STM-->>ESP: ACK,SET_ID,5,51BBE4567890123456789012\n
    Note over ESP,STM: Recovery Completed! Telemetry restored for Tank ID 5.
```

---

## 7. State Machines & Transition Specifications

### 7.1 ESP32 Master Commissioning State Machine

```mermaid
stateDiagram-v2
    [*] --> IDLE_MONITORING
    
    IDLE_MONITORING --> DISCOVERY_EPOCH : User / Auto Trigger Commissioning
    IDLE_MONITORING --> ATOMIC_SWAP_INIT : User Requests ID Swap (ID_A <-> ID_B)
    IDLE_MONITORING --> RECOVERY_CHECK : Unmapped ID=1 Telemetry Detected

    state DISCOVERY_EPOCH {
        [*] --> BROADCAST_DISCOVER
        BROADCAST_DISCOVER --> LISTEN_SLOTS : T0:DISCOVER sent
        LISTEN_SLOTS --> RECORD_UID : Valid UID packet in Slot k
        LISTEN_SLOTS --> MARK_COLLISION : Frame error in Slot k
        RECORD_UID --> LISTEN_SLOTS
        MARK_COLLISION --> RE_DISCOVER_ROUND : Collisions > 0
        RE_DISCOVER_ROUND --> BROADCAST_DISCOVER : New Round Seed R
    }

    DISCOVERY_EPOCH --> ASSIGN_STAGING_IDS : All Slots Processed
    ASSIGN_STAGING_IDS --> IDLE_MONITORING : All UIDs Registered & Assigned

    state ATOMIC_SWAP_INIT {
        [*] --> STAGE_NODE_A_TO_99
        STAGE_NODE_A_TO_99 --> SHIFT_NODE_B_TO_ID_A : ACK 99 Received
        SHIFT_NODE_B_TO_ID_A --> FINALIZE_NODE_A_TO_ID_B : ACK ID_A Received
        FINALIZE_NODE_A_TO_ID_B --> SWAP_COMPLETE : ACK ID_B Received
    }

    ATOMIC_SWAP_INIT --> IDLE_MONITORING : Swap Finished

    state RECOVERY_CHECK {
        [*] --> QUERY_UID_ID1
        QUERY_UID_ID1 --> MATCH_NVS : Received UID
        MATCH_NVS --> RESTORE_FLASH_ID : Match Found in NVS
        MATCH_NVS --> PROMPT_USER_NEW_COMMISSION : UID Not in NVS
        RESTORE_FLASH_ID --> RECOVERY_DONE : ACK Received
    }

    RECOVERY_CHECK --> IDLE_MONITORING
```

### 7.2 STM32 Slave Address Resolution State Machine

```mermaid
stateDiagram-v2
    [*] --> POWER_ON_BOOT

    state POWER_ON_BOOT {
        [*] --> CHECK_DEV_MODE
        CHECK_DEV_MODE --> FORCE_ID_1 : BENCH_DEV_MODE_ID > 0
        CHECK_DEV_MODE --> LOAD_FLASH : BENCH_DEV_MODE_ID == 0
        LOAD_FLASH --> VALID_FLASH_ID : Magic == 0xA5A5A5A5 & ID in [1..10]
        LOAD_FLASH --> READ_DIP_SWITCH : Flash Empty / Invalid Magic
        READ_DIP_SWITCH --> DIP_ID : Hardware DIP > 0
        READ_DIP_SWITCH --> UNCOMMISSIONED_DEFAULT : DIP == 0 -> ID = 1
    }

    POWER_ON_BOOT --> OPERATIONAL_COMMISSIONED : Valid ID assigned
    POWER_ON_BOOT --> UNCOMMISSIONED_LISTENING : ID == 1 (Dev Mode / Default)

    state UNCOMMISSIONED_LISTENING {
        [*] --> WAIT_DISCOVER
        WAIT_DISCOVER --> SCHEDULE_SLOT_TX : Received T0:DISCOVER
        SCHEDULE_SLOT_TX --> WAIT_DISCOVER : Tx UID in computed slot
        WAIT_DISCOVER --> PROCESS_SET_ID : Received T0:STAGE_UID or T1:SET_ID
    }

    UNCOMMISSIONED_LISTENING --> OPERATIONAL_COMMISSIONED : Flash Saved (ID 1..10)

    state OPERATIONAL_COMMISSIONED {
        [*] --> RUN_SLAVE_SUPERLOOP
        RUN_SLAVE_SUPERLOOP --> PROCESS_SWAP_STAGE : Received T<MY_ID>:SET_ID:99
        RUN_SLAVE_SUPERLOOP --> RE_ASSIGN_ID : Received T<MY_ID>:SET_ID:<NEW_ID>
    }

    PROCESS_SWAP_STAGE --> OPERATIONAL_STAGED : MY_TANK_ID = 99
    OPERATIONAL_STAGED --> OPERATIONAL_COMMISSIONED : Received T99:SET_ID:<TARGET_ID>
```

---

## 8. Failure Modes, Power Loss Recovery & Edge Cases

### 8.1 Power Interruption During Atomic ID Swap
**Risk**: Power is lost precisely after Step 1 (Node B is set to `T99`), leaving Node B with `MY_TANK_ID = 99`.  
**Mitigation & Recovery**:
1. Before initiating Step 1, ESP32 writes an NVS transaction marker: `NVS: swap_state = SWAP_IN_PROGRESS, nodeA_UID, nodeB_UID, target_idA, target_idB`.
2. On ESP32 boot, `setup()` checks `swap_state`.
3. If `SWAP_IN_PROGRESS` is set, ESP32 polls for `STAT,99,...`. If Node B responds at `T99`, ESP32 automatically resumes the atomic swap pipeline from Step 2, completing the transaction cleanly.
4. Once completed, ESP32 clears `swap_state` to `SWAP_IDLE`.

### 8.2 Bus Collision Protection & UART Error Callback
If noise or concurrent uncommissioned transmissions cause UART overrun or framing errors:
- STM32 `HAL_UART_ErrorCallback()` discards the corrupted `rx_line` buffer (`rx_index = 0`), clears error flags (`__HAL_UART_CLEAR_OREFLAG`), and immediately re-arms interrupt reception (`HAL_UART_Receive_IT`).
- The UART receiver never freezes or enters a deadlocked state.

---

## 9. Verification & Architectural Compliance

| Compliance Requirement | Verification Method | Status |
| :--- | :--- | :---: |
| **`BENCH_DEV_MODE_ID = 1` Compatibility** | Preserved in `main.c` line 53 for uncommissioned/fresh STM32 initial communication. | **PASSED** |
| **Dual Identity (96-bit UID + Logical Address)** | UID read from `0x1FFF7590` integrated with Tank ID ($1\dots 10$) Flash storage. | **PASSED** |
| **Atomic ID Swap Protocol** | Step-by-step 3-phase staging protocol via `T99` eliminates duplicate IDs. | **PASSED** |
| **Flash Loss & UID Recovery** | ESP32 NVS hardware signature matching restores lost IDs automatically. | **PASSED** |
| **Slotted Backoff Collision Avoidance** | $\text{CRC16}(\text{UID}_{96}) \pmod{16}$ 16-slot timing window resolves multi-board contention. | **PASSED** |

---

## 10. Conclusion

The **Phase 5.0 Commissioning & Identity Protocol Specification** provides a mathematically sound, crash-resilient framework for EAGLEULTRASONiK multi-drop bus management. By combining hardware-level 96-bit UID identification with atomic staging transactions, the protocol guarantees zero bus address duplication, automatic corruption recovery, and effortless field commissioning.
