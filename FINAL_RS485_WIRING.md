# EAGLEULTRASONİK — FINAL RS485 PHYSICAL WIRING SPECIFICATION

**Document Version:** 2.0.0  
**Transceiver IC:** 2x MAX485ESA (+2306) SOIC-8 Modules  
**Date:** 2026-08-11  

---

## 1. Physical Node Pinout Matrix

### 1.1 STM32 Master Node (STM32G474RE) <-> MAX485 #1
| Signal Name | STM32 MCU Pin | MAX485 #1 Pin | Direction | Level & Protection |
| :--- | :--- | :--- | :--- | :--- |
| **USART3_TX** | PB10 (CN10 Pin 25) | Pin 4 (DI) | Output $\to$ Input | 3.3V TTL (Direct) |
| **USART3_RX** | PB11 (CN10 Pin 18) | Pin 1 (RO) | Input $\leftarrow$ Output | 5.0V CMOS (Direct, PB11 is 5V FT) |
| **RS485 DE/RE** | PB1 (CN10 Pin 24) | Pins 2 & 3 (DE,/RE) | Output $\to$ Input | 3.3V CMOS (High=TX, Low=RX) |
| **VCC Supply** | Nucleo 5V Rail | Pin 8 (VCC) | Supply | 5.0V DC (Isolated Rail) |
| **Ground** | Common Signal GND Bus | Pin 5 (GND) | Reference | 0V Common GND |

### 1.2 ESP32 Master Node (ESP32-S3) <-> MAX485 #2
| Signal Name | ESP32-S3 Pin | MAX485 #2 Pin | Direction | Level & Protection |
| :--- | :--- | :--- | :--- | :--- |
| **UART1_TX** | GPIO8 | Pin 4 (DI) | Output $\to$ Input | 3.3V TTL (Direct) |
| **UART1_RX** | GPIO18 | Pin 1 (RO) | Input $\leftarrow$ Output | **3.21V Max (10k/18k Voltage Divider)** |
| **RS485 DE/RE** | GPIO5 | Pins 2 & 3 (DE,/RE) | Output $\to$ Input | 3.3V CMOS (High=TX, Low=RX) |
| **VCC Supply** | ESP32 5V Rail | Pin 8 (VCC) | Supply | 5.0V DC (Isolated Rail) |
| **Ground** | Common Signal GND Bus | Pin 5 (GND) | Reference | 0V Common GND |

---

## 2. Voltage Divider Specification on MAX485 #2 RO Pin

```text
MAX485 #2 Pin 1 (RO - 5.0V Output)
             |
          [ 10kΩ ] (Rtop)
             |
             +---------> ESP32 GPIO18 (3.21V Max Input)
             |
          [ 18kΩ ] (Rbottom)
             |
       Common GND Bus
```

$$V_{\text{out\_nominal}} = 5.0\text{V} \times \left( \frac{18\text{k}\Omega}{10\text{k}\Omega + 18\text{k}\Omega} \right) = \mathbf{3.214\text{V}} \le 3.3\text{V}$$

$$V_{\text{out\_min}} = 4.5\text{V} \times \left( \frac{18\text{k}\Omega}{10\text{k}\Omega + 18\text{k}\Omega} \right) = \mathbf{2.892\text{V}} \ge V_{IH\text{\_min}} (2.475\text{V})$$

---

## 3. Differential Interconnect Wiring Table

| MAX485 #1 (STM32 Side) | Cable Type | MAX485 #2 (ESP32 Side) | Bus Termination & Biasing |
| :--- | :--- | :--- | :--- |
| **Pin 6 (Line A)** | Shielded Twisted Pair | **Pin 6 (Line A)** | 120Ω termination resistor at both ends |
| **Pin 7 (Line B)** | Shielded Twisted Pair | **Pin 7 (Line B)** | 120Ω termination resistor at both ends |
| **Pin 5 (GND)** | Common Shield Drain Wire | **Pin 5 (GND)** | Connected to Common GND Bus at both nodes |
