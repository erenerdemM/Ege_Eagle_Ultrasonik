# EAGLEULTRASONİK — Tank ID & DIP Switch Architecture Audit

> **Document Status:** Comprehensive Source-Level Audit & Architecture Analysis  
> **Author:** Antigravity AI (Pair Programming Auditor)  
> **Target Repository:** `C:\Users\ern0e\EAGLEULTRASONiK`  
> **Primary Source Files Audited:**  
> - [`STM32/Ultrasonik_G4_Master/Core/Src/main.c`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c)  
> - [`STM32/Ultrasonik_G4_Master/Core/Inc/main.h`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Inc/main.h)  
> - [`STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c)  
> - [`STM32/Ultrasonik_G4_Master/Core/Inc/system_state.h`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Inc/system_state.h)  
> - [`esp32/ekran_kontrol/ekran_kontrol.ino`](file:///C:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino)  
> - [`.agent/reports/phase-5.2-id-state-machine.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/.agent/reports/phase-5.2-id-state-machine.md)  
> - [`dip_switch_test.py`](file:///C:/Users/ern0e/EAGLEULTRASONiK/dip_switch_test.py)  
> - [`test_hil_uart.py`](file:///C:/Users/ern0e/EAGLEULTRASONiK/test_hil_uart.py)  
> - [`HW_001_FINAL_PINOUT_DETERMINATION.md`](file:///C:/Users/ern0e/EAGLEULTRASONiK/HW_001_FINAL_PINOUT_DETERMINATION.md)  
> **Date:** August 17, 2026  

---

## 1. Current ID Architecture Overview

In the **EAGLEULTRASONİK** multi-node system, up to 10 STM32G474RE slave nodes share a single RS485 multi-drop ASCII UART bus connected to an ESP32-S3 master controller. Node addressability is essential to prevent bus collisions and ensure target command routing.

The identity of an STM32 slave is governed by a **Dual Identity Model**:

1. **Hardware Unique Identifier (UID24):** A 24-character hexadecimal ASCII string derived from the 96-bit STM32 Factory Unique Device ID register at address `0x1FFF7590` (read via `HAL_GetUIDWord0/1/2()`). UID24 is immutable and physically unique per MCU chip. It is used during discovery and provisioning frames (`DISCOVER_ACK`, `ASSIGN_ID`, `STAGE_ID`, `RESET_ID`) for collision-free target verification.
2. **Logical Tank ID (`MY_TANK_ID`):** An 8-bit unsigned integer in the range `1..10` (or `0` when uncommissioned). Every RS485 command sent by the ESP32 is prefixed with `T<id>:` (e.g. `T1:START`, `T5:SET_TEMP:60`). Slave nodes inspect this prefix and silently discard frames not addressed to their `MY_TANK_ID` (except broadcast frames prefixed with `T0:`).

### Identity States (`ProvState_t`)

The slave node identity state is tracked by `g_system_state.prov_state` in [`system_state.h`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Inc/system_state.h):

- `PROV_STATE_UNCOMMISSIONED` (0): `MY_TANK_ID = 0`. Status telemetry generation is suppressed to prevent bus contention. The node listens only for broadcast commands (`T0:DISCOVER`, `T0:ASSIGN_ID`, `T0:STAGE_ID`, `T0:RESET_ID`, `GET_UID`).
- `PROV_STATE_STAGING` (1): `MY_TANK_ID = 0`. Volatile RAM staging state adopted during multi-node atomic ID swaps. A non-blocking 10,000 ms auto-timeout timer is armed to roll back to the previous ID if the swap transaction is not confirmed.
- `PROV_STATE_ACTIVE` (2): `MY_TANK_ID` is set to a valid logical address (`1..10`). The node actively generates 500 ms heartbeat telemetry telegrams (`STAT,<id>,...`) and processes all operational commands (`START`, `STOP`, `SET_TEMP`, `SET_POWER`, `SET_FREQ`, `SWEEP`).

---

## 2. ID Source Catalog & Verification Status

The architecture contains five distinct identity mechanisms across firmware build settings, physical hardware, non-volatile storage, and communication protocols:

| Identity Source | Location / Mechanism | Values | Status | Role in Architecture |
| :--- | :--- | :---: | :---: | :--- |
| **Factory UID** | STM32 MCU Register `0x1FFF7590` | 24-char Hex String | `IMPLEMENTED` & `PHYSICALLY VERIFIED` | Permanent hardware identity for bus discovery and target UID verification. |
| **Persistent Flash NVS** | Flash Bank 2 Page 127 (`0x0807F800`) | `0xA5A5A5A5` Magic + ID (1..10) + State | `IMPLEMENTED` & `PHYSICALLY VERIFIED` | Primary software persistent identity across power cycles and resets. |
| **Hardware DIP Switch** | GPIO `PC8..PC11` (`DIP_SW1..4`), Active-Low | Raw binary 4-bit $\to$ ID 1..10 | `IMPLEMENTED`, `PARTIALLY VERIFIED` (ID 1 verified; IDs 2..10 unverified on physical bench) | Boot-time hardware fallback when Flash is uncommissioned/erased. |
| **ESP32 Service Provisioning** | RS485 ASCII Bus Protocol (`EAGLE-PROV-v2`) | `ASSIGN_ID`, `STAGE_ID`, `RESET_ID` | `IMPLEMENTED` & `PHYSICALLY VERIFIED` | Dynamic software assignment over serial bus. Modifies RAM & Flash. |
| **Build Macro Override** | [`main.c`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L405) `#define BENCH_DEV_MODE_ID` | `1` (or `0`) | `IMPLEMENTED` & `PHYSICALLY VERIFIED` | Single-node bench development compile-time override. |

---

## 3. Exact Precedence Between Identity Sources

### Boot Initialization Precedence (Power-On / SWD Reset)

When the STM32 MCU boots, `main()` initializes system state and evaluates identity sources in the following **strict top-down order** ([`main.c:L405-L435`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L405-L435)):

```
+-------------------------------------------------------------------+
|                        MCU BOOT / RESET                           |
+-------------------------------------------------------------------+
                                  |
                                  v
                   /-----------------------------\
                  / Is BENCH_DEV_MODE_ID > 0 ?   \
                  \ (Currently set to 1 in main.c)/
                   \-----------------------------/
                             /          \
                       YES  /            \ NO (Production Mode)
                           v              v
            +--------------------+  /------------------------------------\
            | MY_TANK_ID = 1     |  | Call TankId_Load(&init_state)      |
            | State = ACTIVE     |  | Read Flash Bank 2 Page 127         |
            +--------------------+  \------------------------------------/
                                                   /           \
                                            VALID /             \ INVALID / EMPTY
                                          MAGIC  /               \ MAGIC
                                                v                 v
                              +--------------------+  /---------------------------\
                              | MY_TANK_ID = Flash |  | Call ReadDipSwitchId()    |
                              | State = ACTIVE     |  | Read GPIO PC8..PC11       |
                              +--------------------+  \---------------------------/
                                                               /          \
                                                     1..10    /            \ 0 or >10
                                                    VALID    /              \ INVALID
                                                            v                v
                                             +--------------------+  +--------------------+
                                             | MY_TANK_ID = DIP   |  | MY_TANK_ID = 0     |
                                             | State = ACTIVE     |  | State =            |
                                             +--------------------+  |   UNCOMMISSIONED   |
                                                                     +--------------------+
```

1. **Level 1 — Dev Mode Compile-Time Override (`BENCH_DEV_MODE_ID`):**
   - **Condition:** `#if (BENCH_DEV_MODE_ID > 0)` in `main.c`.
   - **Action:** Forces `MY_TANK_ID = BENCH_DEV_MODE_ID` (1) and `g_system_state.prov_state = PROV_STATE_ACTIVE`.
   - **Effect:** **Completely bypasses Flash reads and DIP switch GPIO reads.**
2. **Level 2 — Persistent Flash NVS (`TankId_Load`):**
   - **Condition:** `BENCH_DEV_MODE_ID == 0` AND Flash Bank 2 Page 127 contains valid magic doubleword `0xA5A5A5A5`, ID $\in [1..10]$, and state `PROV_STATE_ACTIVE`.
   - **Action:** Sets `MY_TANK_ID = override_id` and `prov_state = PROV_STATE_ACTIVE`.
   - **Effect:** **Completely overrides physical DIP switch settings.** The DIP switch is NOT read.
3. **Level 3 — Hardware DIP Switch (`ReadDipSwitchId`):**
   - **Condition:** `BENCH_DEV_MODE_ID == 0` AND Flash Bank 2 Page 127 is erased/uninitialized (`magic != 0xA5A5A5A5`).
   - **Action:** Reads GPIO pins `PC8..PC11`. If raw binary value yields $1..10$, sets `MY_TANK_ID = dip_id` and `prov_state = PROV_STATE_ACTIVE`.
4. **Level 4 — Default Uncommissioned State:**
   - **Condition:** `BENCH_DEV_MODE_ID == 0`, Flash is erased/invalid, AND DIP switch reading is 0 (all switches OFF) or invalid (>10).
   - **Action:** Sets `MY_TANK_ID = 0` and `prov_state = PROV_STATE_UNCOMMISSIONED`.

### Runtime Precedence (Superloop Execution)

Once `main()` enters the `while(1)` superloop:
- **DIP Switches are NEVER read at runtime.** `ReadDipSwitchId()` is only called during startup in `main()`. Changing DIP switches while the board is powered on has **zero effect**.
- **Service Commands (`ASSIGN_ID`, `STAGE_ID`, `RESET_ID`) take precedence at runtime:**
  - `ASSIGN_ID` writes Flash and updates `MY_TANK_ID` live in RAM.
  - `STAGE_ID` updates RAM to `MY_TANK_ID = 0` (volatile staging) and arms the 10s rollback timer.
  - `RESET_ID` erases Flash Page 127 and sets `MY_TANK_ID = 0` live in RAM.

---

## 4. Specific Conditions for DIP Switch Evaluation & Bypass

### Conditions Under Which DIP is Read

The DIP switch is read **if and only if** all of the following conditions are met simultaneously:
1. The MCU undergoes a power-on reset, hardware reset, or SWD reset.
2. `BENCH_DEV_MODE_ID == 0` in firmware build.
3. `TankId_Load()` fails magic verification (`magic != 0xA5A5A5A5`) or stored state is not `PROV_STATE_ACTIVE` (i.e. Flash Page 127 is erased or uncommissioned).

### Conditions Under Which DIP is Ignored

The DIP switch is **ignored** under any of the following conditions:
1. `BENCH_DEV_MODE_ID > 0` (currently set to `1` in `main.c`).
2. Flash Bank 2 Page 127 contains a valid `0xA5A5A5A5` magic key and valid active ID 1..10.
3. The MCU is in normal superloop execution (`while(1)`).
4. Service provisioning commands (`ASSIGN_ID`, `STAGE_ID`, `RESET_ID`) are executing.

---

## 5. Event-Driven Behavior Breakdown

The exact system behavior following key lifecycle events is documented below:

| Event / Trigger | Initial State | Post-Event `MY_TANK_ID` | Post-Event `prov_state` | Flash Page 127 State | DIP Switch Evaluated? | Behavior Summary |
| :--- | :---: | :---: | :---: | :---: | :---: | :--- |
| **First Boot (Factory Fresh)** | Unpowered | `DIP_ID` (if 1..10)<br>or `0` (if DIP 0) | `ACTIVE` (if DIP 1..10)<br>`UNCOMMISSIONED` (if 0) | Erased (`0xFFFFFFFF`) | **YES** (if DevMode=0) | Evaluates DIP switch. If DIP is valid (1..10), enters `ACTIVE`. If DIP is 0, enters `UNCOMMISSIONED`. |
| **Boot with Valid Flash ID** | Unpowered | Flash ID ($X$) | `ACTIVE` | Valid (`0xA5A5A5A5`) | **NO** | `TankId_Load()` succeeds. Node enters `ACTIVE` with Flash ID. Physical DIP switch is ignored. |
| **Boot with Erased Flash ID** | Unpowered | `DIP_ID` (if 1..10)<br>or `0` (if DIP 0) | `ACTIVE` (if DIP 1..10)<br>`UNCOMMISSIONED` (if 0) | Erased (`0xFFFFFFFF`) | **YES** (if DevMode=0) | `TankId_Load()` returns 0. Node falls back to DIP switch. |
| **`RESET_ID` Command Received** | `ACTIVE` ($X$) | `0` | `UNCOMMISSIONED` | Erased (`0xFFFFFFFF`) | **NO** | `TankId_EraseOverride()` erases Flash and sets RAM `MY_TANK_ID = 0`. DIP switch is NOT re-read. |
| **Power Cycle after `RESET_ID`** | `UNCOMMISSIONED` | `DIP_ID` (if 1..10)<br>or `0` (if DIP 0) | `ACTIVE` (if DIP 1..10)<br>`UNCOMMISSIONED` (if 0) | Erased (`0xFFFFFFFF`) | **YES** (if DevMode=0) | MCU restarts `main()`. Since Flash is erased, MCU re-reads DIP. **If DIP is non-zero, board jumps back to ACTIVE with DIP ID!** |
| **SWD Hardware Reset** | Any | Depends on Flash/DIP | Depends on Flash/DIP | Unchanged | **YES** (if DevMode=0 & Flash empty) | Equivalent to cold power cycle. Re-evaluates boot precedence. |
| **`ASSIGN_ID:<new_id>:<UID>`** | `UNCOMMISSIONED` | `new_id` | `ACTIVE` | Written (`0xA5A5A5A5`) | **NO** | Erases & writes Flash Page 127 with `new_id`. Updates RAM live. DIP switch ignored. |
| **`STAGE_ID:<UID>`** | `ACTIVE` ($X$) | `0` | `STAGING` | Unchanged ($X$) | **NO** | Saves previous ID/state to RAM backups (`s_saved_tank_id`). Sets `MY_TANK_ID = 0`, arms 10s timer. |
| **Staging 10s Timeout** | `STAGING` | Saved ID ($X$) | Saved State (`ACTIVE`) | Unchanged ($X$) | **NO** | 10,000 ms elapses without confirmation. Restores previous RAM ID/state. |
| **`CONFIRM_ID:<final_id>`** | `STAGING` | `final_id` | `ACTIVE` | Written (`0xA5A5A5A5`) | **NO** | Clears staging flag, writes Flash Page 127 with `final_id`, sets RAM to `ACTIVE`. |

---

## 6. Persistence & Flash Interaction

1. **Does DIP change persistent Flash ID?**  
   **NO.** `ReadDipSwitchId()` is a pure GPIO input read function. It never invokes `HAL_FLASH_Program()` or `TankId_SaveAndVerifyOverride()`. Setting DIP switches affects only the volatile RAM variable `MY_TANK_ID` during startup.
2. **Does changing DIP while Flash contains a valid ID have any effect?**  
   **NO EFFECT AT ALL.** If Flash Page 127 contains a valid `0xA5A5A5A5` magic key and ID, `TankId_Load()` matches the magic at boot and sets `MY_TANK_ID`. `ReadDipSwitchId()` is never executed. Furthermore, DIP switches are not polled in the main superloop.

---

## 7. Valid/Invalid DIP Switch Range & Truth Table

DIP switches are wired to GPIOC pins `PC8..PC11` (`DIP_SW1` through `DIP_SW4`). The inputs are configured with internal pull-up resistors (`GPIO_PULLUP`).
- Switch **ON** $\to$ Pin connected to GND $\to$ `HAL_GPIO_ReadPin() == GPIO_PIN_RESET` $\to$ Bit set to `1`.
- Switch **OFF** $\to$ Pin pulled HIGH (3.3V) $\to$ `HAL_GPIO_ReadPin() == GPIO_PIN_SET` $\to$ Bit set to `0`.

The raw binary value is calculated as:
$$\text{raw} = (\text{SW1} \times 1) + (\text{SW2} \times 2) + (\text{SW3} \times 4) + (\text{SW4} \times 8)$$

### DIP Switch Mapping Truth Table

| SW4 (PC11) Bit 3 | SW3 (PC10) Bit 2 | SW2 (PC9) Bit 1 | SW1 (PC8) Bit 0 | Raw Binary (`raw`) | Returned `MY_TANK_ID` | Resulting Identity State |
| :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| OFF (0) | OFF (0) | OFF (0) | OFF (0) | `0x0` (0) | `0` | `PROV_STATE_UNCOMMISSIONED` |
| OFF (0) | OFF (0) | OFF (0) | **ON (1)** | `0x1` (1) | `1` | `PROV_STATE_ACTIVE` |
| OFF (0) | OFF (0) | **ON (1)** | OFF (0) | `0x2` (2) | `2` | `PROV_STATE_ACTIVE` |
| OFF (0) | OFF (0) | **ON (1)** | **ON (1)** | `0x3` (3) | `3` | `PROV_STATE_ACTIVE` |
| OFF (0) | **ON (1)** | OFF (0) | OFF (0) | `0x4` (4) | `4` | `PROV_STATE_ACTIVE` |
| OFF (0) | **ON (1)** | OFF (0) | **ON (1)** | `0x5` (5) | `5` | `PROV_STATE_ACTIVE` |
| OFF (0) | **ON (1)** | **ON (1)** | OFF (0) | `0x6` (6) | `6` | `PROV_STATE_ACTIVE` |
| OFF (0) | **ON (1)** | **ON (1)** | **ON (1)** | `0x7` (7) | `7` | `PROV_STATE_ACTIVE` |
| **ON (1)** | OFF (0) | OFF (0) | OFF (0) | `0x8` (8) | `8` | `PROV_STATE_ACTIVE` |
| **ON (1)** | OFF (0) | OFF (0) | **ON (1)** | `0x9` (9) | `9` | `PROV_STATE_ACTIVE` |
| **ON (1)** | OFF (0) | **ON (1)** | OFF (0) | `0xA` (10) | `10` | `PROV_STATE_ACTIVE` |
| **ON (1)** | OFF (0) | **ON (1)** | **ON (1)** | `0xB` (11) | `0` | `PROV_STATE_UNCOMMISSIONED` (Invalid range) |
| **ON (1)** | **ON (1)** | OFF (0) | OFF (0) | `0xC` (12) | `0` | `PROV_STATE_UNCOMMISSIONED` (Invalid range) |
| **ON (1)** | **ON (1)** | OFF (0) | **ON (1)** | `0xD` (13) | `0` | `PROV_STATE_UNCOMMISSIONED` (Invalid range) |
| **ON (1)** | **ON (1)** | **ON (1)** | OFF (0) | `0xE` (14) | `0` | `PROV_STATE_UNCOMMISSIONED` (Invalid range) |
| **ON (1)** | **ON (1)** | **ON (1)** | **ON (1)** | `0xF` (15) | `0` | `PROV_STATE_UNCOMMISSIONED` (Invalid range) |

- **Valid Representation:** All Tank IDs $1..10$ are theoretically and mathematically representable by a 4-bit DIP switch array ($2^4 = 16$ possible states).
- **Invalid DIP Behavior:** Any binary setting yielding `0` (all switches OFF) or `11..15` is treated as invalid by `ReadDipSwitchId()` ([`main.c:L107-L108`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L107-L108)) and returns `0U`, forcing the node into `PROV_STATE_UNCOMMISSIONED`.

---

## 8. Source-Level Code Traceability & Verification Matrix

### Source-Level Symbol Traceability

| Function / Symbol | Target File & Line Numbers | Implementation Summary |
| :--- | :--- | :--- |
| `ReadDipSwitchId()` | [`main.c:L99-L109`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L99-L109) | Static function reading active-low GPIO pins `PC8..PC11`. Bounds check `1U <= raw <= 10U`. |
| `TankId_Load()` | [`main.c:L118-L143`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L118-L143) | Reads Flash `0x0807F800`. Checks magic `0xA5A5A5A5` and state `PROV_STATE_ACTIVE`. |
| `TankId_SaveAndVerifyOverride()` | [`main.c:L146-L208`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L146-L208) | Erases Flash Page 127, programs 64-bit doubleword (`0xA5A5A5A5` + ID + State), verifies readback. |
| `TankId_EraseOverride()` | [`main.c:L211-L245`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L211-L245) | Erases Flash Page 127. Sets `MY_TANK_ID = 0` and `prov_state = PROV_STATE_UNCOMMISSIONED`. |
| `TankId_StartStaging()` | [`main.c:L249-L266`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L249-L266) | Saves current ID/state to backup vars. Sets `MY_TANK_ID = 0`, arms 10,000 ms timeout tick. |
| `TankId_ProcessStagingTimeout()` | [`main.c:L268-L283`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L268-L283) | Non-blocking superloop check. Restores backed-up ID/state on 10s timer expiry. |
| `TankId_ConfirmStaging()` | [`main.c:L295-L302`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L295-L302) | Clears staging flag and calls `TankId_SaveAndVerifyOverride()` for `final_id`. |
| `RESET_ID` Handler | [`esp32_uart.c:L382-L405`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L382-L405) | Verifies optional UID24, calls `TankId_EraseOverride()`, transmits `ACK,RESET_ID,<UID>`. |
| `ASSIGN_ID` Handler | [`esp32_uart.c:L333-L381`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L333-L381) | Verifies UID24, checks `PROV_STATE_ACTIVE` rejection interlock, calls `TankId_SaveAndVerifyOverride()`. |
| `STAGE_ID` Handler | [`esp32_uart.c:L309-L332`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/esp32_uart.c#L309-L332) | Verifies UID24, calls `TankId_StartStaging()`, transmits `ACK,STAGE_ID,<UID>`. |
| Boot Initialization | [`main.c:L405-L435`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L405-L435) | Boot sequence in `main()` executing precedence check between DevMode, Flash, and DIP. |
| DIP GPIO Pin Definitions | [`main.h:L108-L115`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Inc/main.h#L108-L115) | `#define DIP_SW1_Pin GPIO_PIN_8` .. `DIP_SW4_Pin GPIO_PIN_11` on `GPIOC`. |
| DIP GPIO Clock & Mode Init | [`main.c:L860-L875`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L860-L875) | Enables `__HAL_RCC_GPIOC_CLK_ENABLE()`, sets `GPIO_MODE_INPUT` with `GPIO_PULLUP`. |

### Existing Verification Evidence vs Missing Evidence

| Verification Target | Evidence Source | Status | Observed Findings / Gaps |
| :--- | :--- | :---: | :--- |
| **DIP SW Bit 0 (ID 1) Reading** | [`PHASE_6_2_BENCH_TEST_MATRIX.md:L13`](file:///C:/Users/ern0e/EAGLEULTRASONiK/PHASE_6_2_BENCH_TEST_MATRIX.md#L13) | `PHYSICALLY VERIFIED` | TEST-02 set DIP SW1=ON (binary 0001) and verified `STAT,1,...` telemetry output. |
| **DIP Fallback After Flash Erase** | [`dip_switch_test.py:L77-L86`](file:///C:/Users/ern0e/EAGLEULTRASONiK/dip_switch_test.py#L77-L86) | `PHYSICALLY VERIFIED` | Script erases Flash via `RESET_ID`, triggers SWD reset, and verifies telemetry fallback to DIP ID. |
| **Service Provisioning (`ASSIGN_ID`)** | [`test_hil_uart.py:L330-L365`](file:///C:/Users/ern0e/EAGLEULTRASONiK/test_hil_uart.py#L330-L365) | `PHYSICALLY VERIFIED` | HIL test suite verifies `STAGE_ID` and `ASSIGN_ID` Flash programming and telemetry return. |
| **DIP SW IDs 2..10 Reading** | Bench Hardware | `NOT PHYSICALLY VERIFIED` | Code implementation exists, but physical DIP switch settings for IDs 2..10 have not been tested individually on bench hardware. |
| **Invalid DIP Range (11..15) Handling** | Bench Hardware | `NOT PHYSICALLY VERIFIED` | Code returns `0U`, but physical verification of DIP setting >10 forcing `UNCOMMISSIONED` has not been logged on physical bench. |
| **Production Boot (`BENCH_DEV_MODE_ID=0`)** | `main.c` Source | `NOT PHYSICALLY VERIFIED` | `main.c` currently retains `#define BENCH_DEV_MODE_ID 1`. Production boot mode (`0`) is unverified in live environment. |

---

## 9. Architectural Ambiguities & Contradictions

The audit identified three major architectural contradictions in the current implementation:

### Contradiction 1: Dev Mode Macro Left Active in Source

- **Finding:** In [`main.c:L405`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L405), `#define BENCH_DEV_MODE_ID 1` is hardcoded.
- **Impact:** When `BENCH_DEV_MODE_ID == 1`, both `TankId_Load()` and `ReadDipSwitchId()` are completely bypassed at boot. Every board compiled with this file forces `MY_TANK_ID = 1`. If multiple boards with this binary are connected to the RS485 bus, all nodes reply to address `T1:` simultaneously, causing instant bus collision.
- **Documentation Mismatch:** Reports (`phase-5.0-baseline-validation.md`, `engineering-change-plan.md`) note that `BENCH_DEV_MODE_ID` must be `0` for production, but the source code has not yet been set to `0`.

### Contradiction 2: Soft `RESET_ID` vs Power-Cycle Inconsistency

- **Finding:** When a node receives `RESET_ID`, `TankId_EraseOverride()` erases Flash Bank 2 Page 127 and sets `MY_TANK_ID = 0` (`PROV_STATE_UNCOMMISSIONED`) live in RAM. It does **not** read the DIP switch.
- **Contradiction:** If the node is subsequently power-cycled or reset via SWD, the boot logic evaluates `TankId_Load()` (fails), falls through to `ReadDipSwitchId()`, and if the physical DIP switches are set to a non-zero value (e.g. ID 1), the node **immediately boots back into `PROV_STATE_ACTIVE` at ID 1**, completely aborting the uncommissioned state!
- **Impact:** A technician attempting to reset a board to `UNCOMMISSIONED` state for service re-provisioning will find that power-cycling the board resurrects the old DIP identity rather than staying uncommissioned.

### Contradiction 3: Multi-Drop Bus Collision Risk with DIP Switches

- **Finding:** If multiple uncommissioned replacement boards are plugged into the RS485 bus with factory default DIP settings (e.g. all set to ID 1 or all set to OFF), two scenarios occur:
  - If DIP switches are left at default ON/ON (e.g. ID 1), both boards boot as `MY_TANK_ID = 1` in `ACTIVE` state, destroying RS485 bus communication due to simultaneous status telegram transmissions.
  - If DIP switches are left OFF (raw = 0), `ReadDipSwitchId()` returns `0`, forcing the board into `PROV_STATE_UNCOMMISSIONED` (`MY_TANK_ID = 0`).
- **Conclusion:** DIP switches fail to guarantee unique addressing on a multi-drop bus because they rely on manual human switch configuration prior to power-on.

---

## 10. Perspective Evaluation of DIP Switch

### Perspective A: DIP Switch as PRIMARY ID SOURCE

- **Is DIP Necessary?** **NO.**
- **Is DIP Redundant?** **YES.** Flash NVS persistence (`TankId_SaveAndVerifyOverride`) combined with ESP32 service provisioning (`EAGLE-PROV-v2` over RS485) provides a complete, persistent, software-managed identity model.
- **Evaluation:** Relying on DIP switches as the primary ID source is obsolete. It limits the system to manual hardware configuration, exposes the RS485 bus to human error and address collisions, and provides no dynamic re-addressing capability required for multi-tank atomic swapping.

### Perspective B: DIP Switch as RECOVERY / BOOTSTRAP FALLBACK

- **Is DIP Necessary?** **NO.** The `EAGLE-PROV-v2` slotted 96-bit UID discovery protocol (`T0:DISCOVER`) allows uncommissioned boards (when Flash is empty and `MY_TANK_ID == 0`) to announce their presence and be assigned a Tank ID over the bus without any manual physical switch intervention.
- **Is DIP Useful or Redundant?**
  - **Redundant & Hazardous:** When DIP switches are present and non-zero, they interfere with the uncommissioned lifecycle. If a board's Flash is erased via `RESET_ID` to prepare it for service provisioning, any unexpected power disruption causes the MCU to fall back to the physical DIP switch, jumping out of `UNCOMMISSIONED` state and resuming active operation under the DIP ID.
  - **Zero Value when OFF:** If DIP switches are left at `0` (all switches OFF), `ReadDipSwitchId()` returns `0`, yielding `MY_TANK_ID = 0` (`PROV_STATE_UNCOMMISSIONED`). This behavior is identical to having no DIP switch at all.

---

## 11. Comprehensive Trade-off & Impact Analysis

### Advantages of Keeping DIP Switch

1. Allows manual offline ID assignment (1..10) on a standalone bench setup without requiring an ESP32 master or service provisioning tool.
2. Provides a hardware fallback if Flash Bank 2 Page 127 hardware fails or suffers endurance degradation.

### Risks & Costs of Keeping DIP Switch

1. **GPIO Consumption:** Consumes 4 valuable MCU pins (`PC8`, `PC9`, `PC10`, `PC11`) on the STM32G474RE.
2. **BOM & PCB Cost:** Requires a physical 4-position DIP switch module, pull-up resistors, PCB real estate, and manual soldering/inspection.
3. **Multi-Drop Bus Collision Vulnerability:** Two uncommissioned boards with identical DIP settings will collide on the RS485 bus at power-on.
4. **State Inconsistency:** Creates a divergence between soft `RESET_ID` (RAM ID = 0) and cold boot after `RESET_ID` (RAM ID = DIP ID).
5. **Human Factor:** Susceptible to wrong switch settings by field assembly personnel.

### Consequences of Removing DIP Switch

1. Frees GPIO pins `PC8..PC11` on the STM32G474RE for other hardware functions or diagnostics.
2. Eliminates DIP switch hardware BOM cost and PCB surface area.
3. Guarantees 100% deterministic, collision-free multi-drop bus operation: uncommissioned boards ALWAYS boot into `MY_TANK_ID = 0` (`PROV_STATE_UNCOMMISSIONED`) and wait silently for slotted `T0:DISCOVER` / `ASSIGN_ID` provisioning.
4. Ensures soft `RESET_ID` and power-cycle after `RESET_ID` produce identical state (`PROV_STATE_UNCOMMISSIONED`).
5. **Operational Requirement:** All uncommissioned boards MUST be assigned an ID via the ESP32 HMI service menu or automated provisioning script using the 96-bit UID24.

---

## 12. Final Architecture Recommendation

Based on rigorous source-level traceability and system safety evaluation:

### **RECOMMENDATION: REMOVE (or KEEP AS RECOVERY ONLY IF MANUAL OFFLINE BENCH SETTING IS MANDATORY)**

#### Technical Justification:

1. **As Primary Identity:** The physical DIP switch is completely superseded by Flash Bank 2 Page 127 persistence (`TankId_SaveAndVerifyOverride`).
2. **As Recovery Fallback:** The DIP switch is redundant because the `EAGLE-PROV-v2` slotted UID discovery mechanism handles uncommissioned board address assignment over RS485 cleanly and safely.
3. **Safety & Determinism:** Removing the DIP switch fallback from `main.c` ensures that uncommissioned or erased boards **always** boot into `MY_TANK_ID = 0` (`PROV_STATE_UNCOMMISSIONED`), suppressing heartbeat telemetry and eliminating any possibility of bus collisions caused by identical hardware switch positions.
4. **Production Readiness Action:** In [`main.c`](file:///C:/Users/ern0e/EAGLEULTRASONiK/STM32/Ultrasonik_G4_Master/Core/Src/main.c#L405), `#define BENCH_DEV_MODE_ID` must be set to `0` for production release.

---
*Audit Document Complete.*
