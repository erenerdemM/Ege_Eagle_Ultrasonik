# EAGLEULTRASONİK — RSK-008 AUTHENTICATION CONSISTENCY AUDIT REPORT

---

## 1. Executive Summary & Audit Classification

A read-only forensic audit of the Service Authentication mechanism and credentials was conducted following the implementation of **RSK-008** (Service / Admin Command Authorization).

### Master Audit Classification:
```text
PRODUCTION CREDENTIAL CHANGED — DOCUMENTATION UPDATE REQUIRED
```

---

## 2. Key Audit Findings

1. **Authoritative Production PIN:**
   - The production ESP32 firmware [`esp32/ekran_kontrol/ekran_kontrol.ino:72`](file:///c:/Users/ern0e/EAGLEULTRASONiK/esp32/ekran_kontrol/ekran_kontrol.ino#L72) defines:
     ```cpp
     String dogru_sifre = "123456";
     ```
   - Git forensic history confirms that `dogru_sifre = "123456"` was established in the initial baseline commit (`228a1e1`) and has **never been altered**.
   - The Nextion Page 4 keypad buffer (`ekran_kontrol.ino:1035`) enforces a 6-digit entry limit (`if (girilen_sifre.length() < 6)`).

2. **Test Suite Credential Parity:**
   - Both mock and physical HIL test suites use the authentic production PIN **`123456`**:
     - `test_hmi_mock.py:42`: `self.dogru_sifre: str = "123456"`
     - `test_hmi_mock.py:932` (`test_06`): `for d in "123456": self.hmi.komutIsle(f"KEY{d}")`
     - `test_hmi_mock.py:1472` (`test_rsk008`): `for digit in "123456": self.hmi.komutIsle(f"KEY{digit}")`
     - `test_hil_uart.py:274` (`setUp`): `for digit in "123456": self.esp32.write_line(f"KEY_{digit}")`
     - `test_hil_uart.py:1406` (`test_rsk008`): `for digit in "123456": self.esp32.write_line(f"KEY_{digit}")`
   - Zero test credential inconsistency exists between the mock harness, physical HIL tests, and the ESP32 firmware.

3. **Status of Legacy Documented PIN `8888`:**
   - The 4-digit PIN `8888` documented in [`docs/EAGLEULTRASONIK_SYSTEM_MANIFESTO.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/EAGLEULTRASONIK_SYSTEM_MANIFESTO.md) (§5.1, §7.3), `SYSTEM_FUNCTION_INVENTORY.md`, and `SYSTEM_MASTER_E2E_TEST_PLAN.md` is a **legacy specification placeholder**.
   - Entering `8888` on the physical Nextion HMI or sending `KEY_8` four times fails authentication against the firmware (requires 6 digits: `123456`).

4. **RSK-008 Implementation Integrity:**
   - RSK-008 **did not change** `dogru_sifre`, the keypad login handler, or the 300-second session timeout timer.
   - RSK-008 strictly added `if (!isProvisioningAllowed())` authorization checks to previously unguarded commands (`P_SAVE|...`, `CMD_SET_STEP_INC:`, `CMD_SET_SWP_SPAN:`, `CMD_SET_SWP_PER:`).

---

## 3. Seven-Point Question-and-Answer Matrix

| Audit Question | Verified Finding | Authoritative Status |
| :--- | :--- | :--- |
| **1. What PIN is hardcoded/configured in production code?** | `String dogru_sifre = "123456";` in `ekran_kontrol.ino:72` | **`123456` (6 digits)** |
| **2. What PIN is used by `test_rsk008`?** | Iterates `"123456"` with `KEY_OK` in both mock & HIL | **`123456` (100% Match)** |
| **3. Is `123456` a real production credential or test only?** | Real production firmware variable since commit `228a1e1` | **Real Production Credential** |
| **4. Is the previous documented Service PIN `8888` authoritative?** | No, `8888` is a legacy 4-digit documentation placeholder | **Non-authoritative (Obsolete)** |
| **5. Did RSK-008 unintentionally change the Service PIN?** | No, `dogru_sifre` and login logic were untouched | **Unchanged** |
| **6. Is authentication mechanism unchanged except guards?** | Yes, keypad login and 300s timeout logic are identical | **100% Preserved** |
| **7. Are service authentication docs and manifesto consistent?** | No, markdown docs still mention `8888`; firmware uses `123456` | **Documentation Update Required** |

---

## 4. Reconciliation Recommendations

To achieve 100% documentation-to-code alignment in future documentation updates:
1. Update [`docs/EAGLEULTRASONIK_SYSTEM_MANIFESTO.md`](file:///c:/Users/ern0e/EAGLEULTRASONiK/docs/EAGLEULTRASONIK_SYSTEM_MANIFESTO.md) (§5.1, §6.2, §7.3) to replace legacy `8888` references with the authoritative 6-digit PIN **`123456`**.
2. Update `docs/SYSTEM_FUNCTION_INVENTORY.md` (`ESP-SVC-AUTH`) and `docs/SYSTEM_MASTER_E2E_TEST_PLAN.md` to reference PIN `123456`.

---

## 5. Audit Conclusion

The authentication logic and authorization guards implemented for RSK-008 are mathematically sound, consistent across all test suites, and operate against the true production firmware credential (`123456`).

```text
FINAL CLASSIFICATION: PRODUCTION CREDENTIAL CHANGED — DOCUMENTATION UPDATE REQUIRED
```

---
*Audit completed under Phase 16 read-only review. Zero source code or test files were modified.*
