# EAGLEULTRASONİK — RS485 PHYSICAL WIRING SPECIFICATION

**Document Version:** 1.0.0  
**Phase:** Phase 5.2 — RS485 Physical Communication Implementation  
**Date:** 2026-08-11  
**Status:** READY FOR HARDWARE CONNECTION (Pending Physical RS485 Module Connection)

---

## 1. Physical Node Pinout Matrix

### 1.1 STM32 Master Node (STM32G474RE) <-> RS485 Module A
| Signal Name | STM32 Pin / Connector | RS485 Module A Pin | Electrical Standard | Notes & Protection |
| :--- | :--- | :--- | :--- | :--- |
| **USART3_TX** | PB10 (CN10 Pin 25) | DI (Data Input) | 3.3V TTL Logic | Direct / 1kΩ Series Resistor |
| **USART3_RX** | PB11 (CN10 Pin 18) | RO (Receiver Output) | 3.3V TTL Logic | Direct (3.3V Module) / 1kΩ Series |
| **RS485 DE/RE** | PB1 (CN10 Pin 24) | DE + /RE (Tied together) | 3.3V CMOS Output | High = TX Mode, Low = RX Mode |
| **VCC Supply** | Nucleo 3.3V (or 5V for MAX485) | VCC | Power Rail | Independent Module Supply |
| **Ground** | Common Signal GND Bus | GND | 0V Reference | Common Logic Ground Bus |

### 1.2 ESP32 Master Node (ESP32-S3) <-> RS485 Module B
| Signal Name | ESP32-S3 Pin | RS485 Module B Pin | Electrical Standard | Notes & Protection |
| :--- | :--- | :--- | :--- | :--- |
| **UART1_TX** | GPIO8 | DI (Data Input) | 3.3V TTL Logic | Direct / 1kΩ Series Resistor |
| **UART1_RX** | GPIO18 | RO (Receiver Output) | 3.3V TTL Logic | Direct |
| **RS485 DE/RE** | GPIO5 | DE + /RE (Tied together) | 3.3V Output | High = TX Mode, Low = RX Mode |
| **VCC Supply** | ESP32 3.3V (or 5V for MAX485) | VCC | Power Rail | Independent Module Supply |
| **Ground** | Common Signal GND Bus | GND | 0V Reference | Common Logic Ground Bus |

---

## 2. Differential Bus Interconnect & Topology

```
+-------------------+                          +-------------------+
|  RS485 MODULE A   |                          |  RS485 MODULE B   |
|   (STM32 Node)    |                          |   (ESP32 Node)    |
|                   |                          |                   |
|           Line A  |==========================| Line A            |
|                   |   Shielded Twisted Pair  |                   |
|           Line B  |==========================| Line B            |
|                   |      (A-B Differential)  |                   |
|           GND     |--------------------------| GND               |
+-------------------+                          +-------------------+
      [ 120Ω ]                                       [ 120Ω ]
  (Term. Resistor)                               (Term. Resistor)
```

### 2.1 Differential Wiring Table
| Terminal | Interconnect Wire | Signal | Termination Requirement |
| :--- | :--- | :--- | :--- |
| **Module A Line A (+)** | Twisted Pair Wire 1 | Non-inverting Differential (+) | 120Ω across A-B at Module A |
| **Module A Line B (-)** | Twisted Pair Wire 2 | Inverting Differential (-) | 120Ω across A-B at Module B |
| **Common GND** | Cable Shield / Drain | Signal Ground Reference | Connected to Common GND Bus at both ends |

---

## 3. Multimeter & Safety Verification Procedure

1. **Unpowered Check:**
   - Verify A ↔ B resistance is ~60Ω (two 120Ω termination resistors in parallel).
   - Verify A ↔ GND and B ↔ GND resistance is > 1kΩ (no short to ground).
   - Verify VCC ↔ GND resistance is > 100Ω (no power supply short).
2. **Powered Check:**
   - Verify VCC is stable 3.3V DC (or 5.0V DC if MAX485 is used).
   - Verify DE/RE pins (STM32 PB1 and ESP32 GPIO5) measure 0.0V DC when idle (RX mode).
   - Verify differential idle voltage: $V_A - V_B > +200\text{mV}$ (Line A higher than Line B in idle state).
