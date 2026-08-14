# EAGLEULTRASONiK Phase 5.2 — Adversarial Stress-Test Review Report
## Device Commissioning & Protocol Integrity Design Challenge

> **Document Status:** OFFICIAL ADVERSARIAL AUDIT REPORT & SPECIFICATION  
> **Role:** Adversarial Reviewer for Safety-Critical Embedded Systems  
> **Target Subsystem:** Multi-Drop RS485 Protocol (`ESP32-S3 Master` $\leftrightarrow$ `STM32G474RE Slaves (1..10)`)  
> **Target File:** `c:\Users\ern0e\EAGLEULTRASONiK\.agent\reports\phase-5.2-adversarial-review-report.md`  
> **Date:** August 10, 2026  

---

## 1. Executive Summary & Audit Mandate

As the **Adversarial Reviewer** for EAGLEULTRASONiK, the primary objective is to challenge every safety assumption, edge case, hardware race condition, memory corruption risk, and protocol timing vulnerability in the **Phase 5.2 Device Commissioning & Protocol Integrity Architecture**.

Industrial ultrasonic generator arrays operate in high-power, electrically noisy (EMI/ESD), multi-node environments. Mis-assigned node IDs, bus collisions, un-staged identity swaps, or corrupt Flash memory writes can lead to catastrophic failures—such as loss of emergency stop control (`STOP` command contention), thermal runaways in transducers, or physical driver destruction on the RS485 differential bus.

This document presents a rigorous stress-test evaluation of **10 Critical Failure Vectors**, providing detailed step-by-step failure scenarios, quantitative risk ratings, and mandatory mitigation contracts for software and protocol implementation.

---

## 2. Risk Evaluation Matrix

Risk ratings are assigned based on **Severity** (impact on safety, bus integrity, and system stability) and **Likelihood** (probability of occurrence during manufacturing, commissioning, or field operation).

| Vector ID | Failure Vector Description | Severity | Likelihood | Risk Rating |
| :---: | :--- | :---: | :---: | :---: |
| **V-01** | Duplicate ID Creation During Assignment or Swap | CRITICAL | HIGH | 🔴 **CRITICAL** |
| **V-02** | Simultaneous Discovery Response (5 Nodes Slot Collision) | HIGH | HIGH | 🟠 **HIGH** |
| **V-03** | Interrupted ID Assignment Mid-Write (Power Cut during Flash Page 127 Erase/Program) | CRITICAL | MEDIUM | 🔴 **CRITICAL** |
| **V-04** | Flash Readback Mismatch or Corrupted Doubleword | HIGH | MEDIUM | 🟠 **HIGH** |
| **V-05** | Stale or Replayed `SET_ID` Frame Execution | HIGH | MEDIUM | 🟠 **HIGH** |
| **V-06** | 96-bit UID Mismatch in Unicast Frame Parsing | CRITICAL | MEDIUM | 🔴 **CRITICAL** |
| **V-07** | ESP32 Master Crash/Reset During 3-Way Atomic Swap | HIGH | MEDIUM | 🟠 **HIGH** |
| **V-08** | STM32 Slave Crash/Reset During `T99` Staging | HIGH | MEDIUM | 🟠 **HIGH** |
| **V-09** | Re-commissioning Node to Invalid ID ($<1$, $>10$, or ID $0$) | CRITICAL | LOW | 🟠 **HIGH** |
| **V-10** | Multi-Drop Bus Saturation with 10 Nodes During Discovery | MEDIUM | HIGH | 🟡 **MEDIUM** |

---

## 3. Deep-Dive Stress-Test Review: 10 Critical Failure Vectors

---

### Vector 1: Duplicate ID Creation During Assignment or Swap

#### 1.1 Exact Failure Scenario
1. **Direct Assignment Vector:** An operator or Master script attempts to assign `ID = 4` to a newly discovered node on the bus. However, `ID = 4` is already assigned to active **Node D**.
2. **Line Contention:** Once the new node adopts `MY_TANK_ID = 4`, both the new node and Node D listen on `T4:`. During periodic status telemetry transmission (500 ms heartbeat) or upon receiving `T4:GET_STATUS`, both nodes simultaneously pull their RS485 Driver Enable (`DE`) pins HIGH.
3. **Electrical & Protocol Destruction:** Node A transmitting logic `0` ($V_A > V_B$) while Node D transmits logic `1` ($V_A < V_B$) creates line voltage contention. The differential voltage drops into the undefined threshold region ($|V_{OD}| < 200\text{ mV}$), drawing excessive supply current and distorting UART bits.
4. **Safety Failure:** The ESP32 receiver detects garbled UART framing errors. If a safety-critical command (`T4:STOP` or `T0:STOP_ALL`) is issued, line contention prevents clean frame parsing, leaving active ultrasonic transducers running uncontrolled.

#### 1.2 Risk Rating
* **Severity:** **CRITICAL** (Loss of emergency stop control, bus communication collapse, potential RS485 transceiver thermal stress).
* **Likelihood:** **HIGH** (Occurs whenever direct un-staged reassignment or duplicate DIP switch setting is executed).
* **Overall Risk Level:** 🔴 **CRITICAL**

#### 1.3 Mandatory Mitigation Contracts
* **CONTRACT-V01-A (3-Way Atomic Staging):** Direct assignment to an active ID is strictly prohibited. ID reassignment MUST follow the 3-phase staging sequence: `Node_A(ID_x) -> T99`, `Node_B(ID_y) -> ID_x`, `Node_A(T99) -> ID_y`.
* **CONTRACT-V01-B (Master Bus Reservation Table):** The ESP32 Master MUST maintain an in-memory and NVS Active ID Map. Before sending `SET_ID:N`, the Master engine MUST assert that `ID N` is marked `VACANT`.
* **CONTRACT-V01-C (Unicast UID Binding):** Every `SET_ID` frame MUST contain the 24-character hexadecimal hardware UID (`T99:SET_ID:<TARGET_ID>:<UID24>`). Slave nodes MUST perform exact bitwise matching against `0x1FFF7590` before accepting the new ID.
* **CONTRACT-V01-D (Post-Assignment Ping Probe):** Master MUST issue `T<NEW_ID>:PING:<UID24>` and receive `ACK,PING` before finalizing NVS database updates.

---

### Vector 2: Simultaneous Discovery Response from 5 Uncommissioned Nodes (Slot Collision)

#### 2.1 Exact Failure Scenario
1. 5 uncommissioned nodes (`MY_TANK_ID = 1`) are attached to the RS485 bus. The Master broadcasts `T0:DISCOVER`.
2. Each node calculates its response slot: $\text{Slot} = \text{CRC16}(\text{UID96}) \pmod{16}$.
3. By the Birthday Paradox, the probability of at least one slot collision among 5 nodes in 16 slots is:
   $$P(\text{Collision}) = 1 - \frac{16 \times 15 \times 14 \times 13 \times 12}{16^5} = 1 - 0.5074 = 49.26\%$$
4. If **Node 2** and **Node 4** both evaluate to Slot 5, at $t = 5 \times 40\text{ ms} = 200\text{ ms}$, both nodes simultaneously assert `DE` and transmit `T0:UID_ANNOUNCE...`.
5. Transmissions overlap on the bus, causing UART framing/checksum errors on the ESP32 receiver. If the protocol lacks multi-round resolution, both nodes remain undiscovered and commissioning halts.

#### 2.2 Risk Rating
* **Severity:** **HIGH** (Commissioning pipeline lockup, undetected connected nodes).
* **Likelihood:** **HIGH** ($\sim 49.3\%$ probability for 5 nodes; $>98\%$ for 10 nodes).
* **Overall Risk Level:** 🟠 **HIGH**

#### 2.3 Mandatory Mitigation Contracts
* **CONTRACT-V02-A (Seeded Multi-Round Re-Discovery):** If Master detects UART errors during Slot $k$, it increments discovery epoch seed $R$ ($R=1, 2, 3$). Slaves compute:
  $$\text{Slot}_{R} = \text{CRC16}(\text{UID96} \oplus R) \pmod{16}$$
  Nodes already claimed in state `DEVICE_SELECTED` or `COMMISSIONED` IGNORE subsequent `T0:DISCOVER` frames.
* **CONTRACT-V02-B (Per-Node Hardware Jitter):** Slave slot delay MUST incorporate microsecond hardware jitter derived from the lower bits of the UID register:
  $$T_{\text{delay}} = (\text{Slot} \times 40\text{ ms}) + (\text{UID}[0] \pmod{15}\text{ ms})$$
* **CONTRACT-V02-C (Binary Prefix Partitioning Fallback):** If collisions persist after 3 discovery rounds, Master issues `T0:DISCOVER_MASK:<HEX_PREFIX>`, querying only nodes whose UID begins with `<HEX_PREFIX>`, deterministically bisecting the collision domain.

---

### Vector 3: Interrupted ID Assignment Mid-Write (Power Cut During Flash Page 127 Erase/Program)

#### 3.1 Exact Failure Scenario
1. STM32 receives valid unicast `T99:SET_ID:3:<UID24>`.
2. Slave unlocks Flash, issues `HAL_FLASHEx_Erase()` for Bank 2 Page 127 (`0x0807F800`). Page erase requires $\approx 20\text{ ms}$.
3. During page erase or 64-bit doubleword programming, system 24V power drops or the supply cord is disconnected.
4. Flash Page 127 is left partially erased or partially written:
   * *Case A:* Entire page erased to `0xFFFFFFFF`.
   * *Case B:* Doubleword partial program where Magic `0xA5A5A5A5` is garbled or ID word contains random bit states (`0x000000FF`).
5. On next power-on boot, if `TankId_Load()` lacks strict doubleword verification, node may boot with an out-of-range ID, corrupt `MY_TANK_ID`, or crash during address matching.

#### 3.2 Risk Rating
* **Severity:** **CRITICAL** (Node boots into unrecoverable/illegal state, unexpected bus behavior).
* **Likelihood:** **MEDIUM** (Industrial power glitches, intentional power cycling during setup).
* **Overall Risk Level:** 🔴 **CRITICAL**

#### 3.3 Mandatory Mitigation Contracts
* **CONTRACT-V03-A (Atomic 64-Bit Payload & Magic Invariant):** Payload stored at `0x0807F800` MUST strictly match the 64-bit layout:
  * Address `0x0807F800`: Magic Key `0xA5A5A5A5` (uint32_t)
  * Address `0x0807F804`: Configured Tank ID $N \in [1 \dots 10]$ (uint32_t)
* **CONTRACT-V03-B (Boot Invariant Validation in `TankId_Load`):**
  ```c
  uint8_t TankId_Load(void) {
      uint32_t magic     = *(volatile uint32_t *)(TANK_ID_FLASH_ADDR);
      uint32_t stored_id = *(volatile uint32_t *)(TANK_ID_FLASH_ADDR + 4U);
      if (magic == 0xA5A5A5A5UL && stored_id >= 1U && stored_id <= 10U) {
          return (uint8_t)stored_id;
      }
      return 0U; // Invalid or erased -> Fall back to UNCOMMISSIONED (ID = 1 / DIP)
  }
  ```
* **CONTRACT-V03-C (Power-On Hardware Recovery Announcement):** If `TankId_Load()` evaluates to `0U` on boot, node enters `UNCOMMISSIONED` state and sends `T0:UID_ANNOUNCE:<UID24>:RECOVERY`. ESP32 Master automatically matches UID against NVS and re-commissions the node without human intervention.

---

### Vector 4: Flash Readback Mismatch or Corrupted Doubleword

#### 4.1 Exact Failure Scenario
1. STM32 executes `TankId_SaveOverride(new_id)`. Page erase succeeds, but `HAL_FLASH_Program()` returns an error (e.g. Flash controller lock, write protection bit, degraded Flash cell endurance, or voltage dip).
2. In current un-mitigated code ([`main.c:149`](file:///c:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L149)), `MY_TANK_ID` is assigned `new_id` live in RAM **without checking `HAL_FLASH_Program` return status and without reading back Flash memory**.
3. Slave sends positive acknowledgment `ACK,SET_ID` to Master and operates as `MY_TANK_ID = new_id` in RAM.
4. Upon reboot, `TankId_Load()` fails because Flash memory was never correctly written. Node reverts to default `ID = 1` or DIP switch ID.
5. **Split-Brain Failure:** System functions correctly until reboot, after which node silently changes identity, breaking Master NVS topology mapping.

#### 4.2 Risk Rating
* **Severity:** **HIGH** (Silent identity drift across power cycles, RAM vs Flash state divergence).
* **Likelihood:** **MEDIUM** (Hardware degradation, Flash write errors under thermal/voltage stress).
* **Overall Risk Level:** 🟠 **HIGH**

#### 4.3 Mandatory Mitigation Contracts
* **CONTRACT-V04-A (Mandatory Readback API `TankId_SaveAndVerifyOverride`):** Replacing void-returning functions, Flash programming MUST implement strict readback verification:
  ```c
  int TankId_SaveAndVerifyOverride(uint8_t new_id) {
      if (new_id < 1U || new_id > 10U) return -1;
      HAL_FLASH_Unlock();
      __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
      
      FLASH_EraseInitTypeDef erase_init = {
          .TypeErase = FLASH_TYPEERASE_PAGES,
          .Banks     = TANK_ID_FLASH_BANK,
          .Page      = TANK_ID_FLASH_PAGE,
          .NbPages   = 1U
      };
      uint32_t page_error = 0U;
      if (HAL_FLASHEx_Erase(&erase_init, &page_error) != HAL_OK) {
          HAL_FLASH_Lock();
          return -2; // Erase Failed
      }
      
      uint64_t data = ((uint64_t)new_id << 32) | 0xA5A5A5A5UL;
      if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, TANK_ID_FLASH_ADDR, data) != HAL_OK) {
          HAL_FLASH_Lock();
          return -3; // Program Failed
      }
      HAL_FLASH_Lock();
      
      /* Readback Verification */
      uint32_t read_magic   = *(volatile uint32_t *)(TANK_ID_FLASH_ADDR);
      uint32_t read_tank_id = *(volatile uint32_t *)(TANK_ID_FLASH_ADDR + 4U);
      if (read_magic != 0xA5A5A5A5UL || read_tank_id != (uint32_t)new_id) {
          return -4; // Readback Mismatch Error
      }
      
      MY_TANK_ID = new_id; // Atomic RAM update ONLY after verified readback
      return 0; // Success
  }
  ```
* **CONTRACT-V04-B (RAM Update Protection):** `MY_TANK_ID` MUST NOT be updated in RAM if Flash write or readback verification returns any non-zero error code.
* **CONTRACT-V04-C (Error Telegram Transmission):** Upon readback mismatch, slave MUST transmit `T99:ERR_SET_ID:<UID24>:ERR_FLASH_VERIFY` and revert state to `UNCOMMISSIONED`.

---

### Vector 5: Stale or Replayed SET_ID Frame Execution

#### 5.1 Exact Failure Scenario
1. An RS485 hardware buffer, UART bridge queue, or rogue script re-transmits a stale `T99:SET_ID:3:<UID24>` frame from a previous commissioning session.
2. Target node is currently operating in `NORMAL_OPERATION` as **Tank ID 5**.
3. If node's command parser evaluates `SET_ID` frames regardless of its operational state:
   * Flash Page 127 is overwritten with `ID = 3`.
   * `MY_TANK_ID` is updated to 3 in RAM.
4. Node 5 silently transitions to ID 3 while another active node is already operating as ID 3, creating an immediate, undetected address collision on `T3:`.

#### 5.2 Risk Rating
* **Severity:** **HIGH** (Unintended identity overwrite during active operations, instant address collision).
* **Likelihood:** **MEDIUM** (Buffer retries in serial converters, stale command replay).
* **Overall Risk Level:** 🟠 **HIGH**

#### 5.3 Mandatory Mitigation Contracts
* **CONTRACT-V05-A (State Machine Parsing Guard):** `SET_ID` commands MUST be accepted ONLY when node state machine is in `DEVICE_SELECTED` or `STAGED_T99`. Nodes in `NORMAL_OPERATION` MUST reject `SET_ID` unless preceded by an explicit `T<MY_ID>:RESET_ID` or authenticated `STAGE_TO_99` command.
* **CONTRACT-V05-B (Monotonic Session Nonce):** Master MUST include a 16-bit transaction counter / nonce in `SET_ID` frames (`T99:SET_ID:<NEW_ID>:<UID24>:<NONCE>`). Slave verifies that `<NONCE>` matches the active discovery session nonce.
* **CONTRACT-V05-C (Rejection Telemetry):** If `SET_ID` is received in illegal state, slave transmits `T<MY_ID>:ERR_SET_ID:ILLEGAL_STATE` and ignores payload.

---

### Vector 6: 96-bit UID Mismatch in Unicast Frame Parsing

#### 6.1 Exact Failure Scenario
1. Master transmits broadcast claim frame: `T0:CLAIM_UID:36FFD8054655383712501843:99` targeting **Node A**.
2. **Node B** (UID `48AAF1239876543210987654`) receives the broadcast.
3. If Node B's command parser contains a string matching defect (e.g. checking only prefix substring `"36FF"`, utilizing un-bounded `strncmp`, or failing to check string length):
   * Node B falsely evaluates match as TRUE.
   * Both Node A and Node B adopt temporary address `T99` and respond to `T99:SET_ID:2`.
4. Both nodes write `ID = 2` to Flash Page 127, corrupting array configuration and creating twin nodes on the bus.

#### 6.2 Risk Rating
* **Severity:** **CRITICAL** (Multi-node identity takeover, corrupted Flash on unintended nodes).
* **Likelihood:** **MEDIUM** (Loose string comparison implementations, parser off-by-one errors).
* **Overall Risk Level:** 🔴 **CRITICAL**

#### 6.3 Mandatory Mitigation Contracts
* **CONTRACT-V06-A (Canonical 24-Hex Character Formatting):** Slave startup code MUST format its 96-bit MCU UID read from `0x1FFF7590` into a canonical 24-byte uppercase hex string:
  ```c
  char local_uid_str[25];
  snprintf(local_uid_str, sizeof(local_uid_str), "%08X%08X%08X",
           *(uint32_t*)(0x1FFF7590), *(uint32_t*)(0x1FFF7594), *(uint32_t*)(0x1FFF7598));
  ```
* **CONTRACT-V06-B (Exact 24-Byte Memory Compare):** Command parser MUST enforce exact length validation ($24\text{ chars}$) and full 24-byte comparison:
  ```c
  if (strlen(frame_uid_str) != 24 || memcmp(local_uid_str, frame_uid_str, 24) != 0) {
      return; // Ignore frame - UID mismatch
  }
  ```
* **CONTRACT-V06-C (Dual Payload Verification):** Claim frames MUST append a 16-bit CRC of the UID. Node validates BOTH the 24-char string and `CRC16(UID96)` before transitioning state.

---

### Vector 7: ESP32 Master Crash/Reset During 3-Way Atomic Swap

#### 7.1 Exact Failure Scenario
1. Master initiates atomic swap between **Node B (ID 2)** and **Node D (ID 4)**.
2. Step 1 executes successfully: Node B transitions to staging address `T99` (`MY_TANK_ID = 99`). `ID 2` is now vacant.
3. At this exact microsecond, ESP32 Master experiences a Watchdog Timer (WDT) reset, brownout, or stack overflow.
4. ESP32 reboots. Node B is left staged at `T99`. Node D remains at `ID 4`.
5. If ESP32 lacks persistent transaction tracking:
   * Master does not poll `T99` during normal telemetry monitoring.
   * Node B remains isolated at `T99` indefinitely, disabling Tank 2 on HMI.

#### 7.2 Risk Rating
* **Severity:** **HIGH** (Orphaned node at `T99`, broken process telemetry, system degradation).
* **Likelihood:** **MEDIUM** (Software crash, supply voltage dip during maintenance).
* **Overall Risk Level:** 🟠 **HIGH**

#### 7.3 Mandatory Mitigation Contracts
* **CONTRACT-V07-A (ESP32 NVS Write-Ahead Logging):** Before issuing ANY swap command frame, Master MUST write transaction record to NVS:
  `NVS Key "SWAP_LOG": { state: IN_PROGRESS, nodeA_UID: ..., nodeB_UID: ..., idA: 2, idB: 4, step: 1 }`
* **CONTRACT-V07-B (Master Boot Recovery Routine):** On ESP32 `setup()`, Master checks `SWAP_LOG`. If `state == IN_PROGRESS`:
  1. Master sends `T99:PING`.
  2. Reads UID response from staged node.
  3. Resumes swap execution pipeline from `step == 1` (assigns vacant `ID 2` to Node D, then reassigns `T99` node to `ID 4`).
  4. Updates `SWAP_LOG` state to `COMPLETED`.
* **CONTRACT-V07-C (Slave Staging Auto-Timeout):** A slave node in `STAGED_T99` state MUST run a 10,000 ms countdown timer. If no valid `SET_ID` frame is received within 10 seconds, slave reverts to its persistent Flash ID (`ID 2`) and transmits `T2:STAT:STAGING_TIMEOUT`.

---

### Vector 8: STM32 Slave Crash/Reset During T99 Staging

#### 8.1 Exact Failure Scenario
1. Master sends `T2:SET_ID:99` to Node B. Node B updates RAM `MY_TANK_ID = 99`.
2. Before Master can transmit `T99:SET_ID:4`, Node B suffers an ESD glitch, IWDG reset, or local power cycle.
3. Node B reboots:
   * If Flash Page 127 was NOT modified during staging (retaining `ID = 2`), Node B boots as `ID = 2`.
4. Meanwhile, Master believes Node B is at `T99` and issues `T4:SET_ID:2` to Node D.
5. **Duplicate ID Collision:** Both Node B and Node D now respond to `ID 2`, triggering immediate bus collision.

#### 8.2 Risk Rating
* **Severity:** **HIGH** (Master-Slave state desynchronization, unexpected duplicate ID creation).
* **Likelihood:** **MEDIUM** (ESD event during industrial handling, supply noise).
* **Overall Risk Level:** 🟠 **HIGH**

#### 8.3 Mandatory Mitigation Contracts
* **CONTRACT-V08-A (RAM-Only Staging Rule):** Staging command `SET_ID:99` MUST NOT write `99` as a persistent address to Flash Page 127. Flash Page 127 MUST retain previous valid ID (`ID 2`). Staging address `99` exists strictly in volatile RAM.
* **CONTRACT-V08-B (Pre-Assignment Active Ping):** Before assigning a newly vacated ID (e.g. `ID 2`) to Node D, Master MUST issue `T2:PING`. If Node B reboots and answers at `T2:PING`, Master detects staging failure, aborts swap transaction, and alerts HMI (`SWAP_ABORTED_SLAVE_RESET`).

---

### Vector 9: Re-Commissioning a Node to an Invalid ID (<1 or >10 or ID 0 via User Command)

#### 9.1 Exact Failure Scenario
1. Operator inputs invalid ID (e.g. `0`, `15`, or `-1`) on HMI, or bug in Master script generates frame `T99:SET_ID:0`.
2. If slave parser lacks bounds checking:
   * Node writes `0` or `15` to Flash Page 127 and RAM `MY_TANK_ID`.
3. **If `ID = 0` (Broadcast Address):** Slave interprets `MY_TANK_ID = 0` as matching ALL broadcast commands (`T0:`). Every broadcast frame triggers simultaneous transmission from this node, destroying RS485 bus communications.
4. **If `ID > 10`:** Node operates outside system allocation bounds ($1 \dots 10$), triggering buffer overflow in ESP32 status arrays (`tank_status[ID]`).

#### 9.2 Risk Rating
* **Severity:** **CRITICAL** (Total bus destruction via ID 0 broadcast response, ESP32 memory corruption).
* **Likelihood:** **LOW** (Requires invalid HMI input or corrupted serial command).
* **Overall Risk Level:** 🟠 **HIGH**

#### 9.3 Mandatory Mitigation Contracts
* **CONTRACT-V09-A (Hard Bounds Checking on Slave Parser):**
  ```c
  long new_id = strtol(&cmd[7], &endptr, 10);
  if (endptr == &cmd[7] || new_id < 1 || new_id > 10) {
      ESP32_UART_SendResponse("T99:ERR_SET_ID:INVALID_RANGE\n");
      return; // Reject command
  }
  ```
* **CONTRACT-V09-B (Hard Bounds Checking on Master Engine):** ESP32 Master commissioning module MUST validate $1 \le \text{target\_id} \le 10$ prior to serial frame generation.
* **CONTRACT-V09-C (Forbidden ID 0 Invariant):** Address `0` is permanently reserved for Master Broadcast (`T0:`). No slave MCU shall ever accept, store, or transmit with `MY_TANK_ID = 0`.

---

### Vector 10: Multi-Drop Bus Saturation with 10 Nodes During Discovery

#### 10.1 Exact Failure Scenario
1. 10 uncommissioned nodes (`ID = 1`) are connected to RS485 bus. Master broadcasts `T0:DISCOVER`.
2. All 10 nodes transmit `T0:UID_ANNOUNCE...` within the 16-slot window ($800\text{ ms}$).
3. High burst of incoming UART frames at 115200 Baud ($3.5\text{ ms}$ per 35-byte frame).
4. If ESP32 Master UART RX FIFO buffer is undersized (e.g. 128 bytes) or processed in single-threaded event loop blocked by HMI GUI redraws:
   * ESP32 suffers UART RX FIFO Overrun (`UART_FIFO_OVF_INT`).
   * UID frames are dropped or truncated, causing discovery failures and endless re-discovery cycles.

#### 10.2 Risk Rating
* **Severity:** **MEDIUM** (Startup delay, discovery loop lockup, high CPU overhead).
* **Likelihood:** **HIGH** (Standard behavior when deploying 10-node array).
* **Overall Risk Level:** 🟡 **MEDIUM**

#### 10.3 Mandatory Mitigation Contracts
* **CONTRACT-V10-A (ESP32 Dedicated Ring Buffer & Task):** Master UART RX buffer MUST be allocated $\ge 1024$ bytes with dedicated high-priority FreeRTOS RX Task decoupled from Nextion HMI task.
* **CONTRACT-V10-B (Paced Epoch Slot Timing):** Slot duration fixed to $50\text{ ms}$ minimum ($3.5\text{ ms}$ transmission $+ 46.5\text{ ms}$ processing window). Total epoch length $= 16 \times 50\text{ ms} = 800\text{ ms}$.
* **CONTRACT-V10-C (Single Node Staging Monotonic Reduction):** Master claims and commissions ONE node per discovery epoch. Once claimed (`T99` / `ID N`), node drops out of subsequent discovery epochs, monotonically reducing bus load ($10 \rightarrow 9 \rightarrow 8 \dots \rightarrow 0$).
* **CONTRACT-V10-D (Inter-Epoch Quiet Window):** Master enforces a $200\text{ ms}$ bus quiet window between discovery rounds to allow UART RX queues to clear completely.

---

## 4. Phase 5.2 Report Specification & Architectural Directives

To ensure absolute safety and alignment across implementation teams, the following **Architectural Directives** are formally mandated for the Phase 5.2 Release:

1. **Production Code Guard (`BENCH_DEV_MODE_ID`):**
   * Firmware builds MUST set `#define BENCH_DEV_MODE_ID 0` for all production releases.
   * Uncommissioned boards MUST boot with Flash evaluation (`TankId_Load()`), defaulting to `UNCOMMISSIONED` state (`MY_TANK_ID = 1` passive listening mode).

2. **Refactored Flash API (`TankId_SaveAndVerifyOverride`):**
   * Direct calls to unverified `TankId_SaveOverride` are strictly DEPRECATED.
   * All Flash operations MUST invoke `TankId_SaveAndVerifyOverride()` enforcing 64-bit doubleword payload, `0xA5A5A5A5` magic key, and post-write memory readback.

3. **ESP32 NVS Transaction Logging:**
   * ESP32 Master MUST write all identity mutations (Commissioning, Swap, Recovery) to NVS write-ahead logs (`SWAP_LOG`) prior to serial frame transmission.

4. **HIL Automated Verification Test Suite:**
   * Every mitigation contract (V-01 through V-10) MUST be validated against HIL automated test cases on ST-Link COM11 / ESP32 console before final design freeze.

---
*Report Compiled & Certified by Adversarial Reviewer for EAGLEULTRASONiK Phase 5.2 Design Gate.*
