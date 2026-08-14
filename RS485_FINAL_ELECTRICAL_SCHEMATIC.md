# EAGLEULTRASONİK — RS485 FINAL ELECTRICAL SCHEMATIC & WIRING SPECIFICATION

**Document Version:** 1.0.0  
**Transceiver IC:** 2x MAX485ESA (+2306) SOIC-8 Modules  
**Operating Voltage:** VCC = 5.0V DC  
**Date:** 2026-08-11  

---

## 1. System Electrical Block Diagram (ASCII)

```text
+---------------------------------------------------------------------------------------------+
|                                    NUCLEO-G474RE NODE                                       |
|                                                                                             |
|   PB10 (USART3_TX)  ----------------------------->  DI  [ MAX485 #1 ]                       |
|   PB11 (USART3_RX)  <-----------------------------  RO  (Direct Connection, 5V FT Pin)       |
|   PB1  (RS485_DE_RE)----------------------------->  DE  & /RE (Tied Together)               |
|   5V Rail           ----------------------------->  VCC (5.0V Supply)                       |
|   GND               ----------------------------->  GND                                     |
+---------------------------------------------------------------------------------------------+
                                                             | | |
                                                             | | |
=================================== DIFFERENTIAL RS485 BUS = | | | ============================
                                                             | | |
                                                  Line A (+) = | |
                                                  Line B (-) === |
                                                  Signal GND ====+
                                                             | | |
                                                             | | |
+---------------------------------------------------------------------------------------------+
|                                      ESP32-S3 NODE                                          |
|                                                                                             |
|   GPIO8  (UART1_TX) ----------------------------->  DI  [ MAX485 #2 ]                       |
|   GPIO18 (UART1_RX) <--- [ 10kΩ ] ---+------------  RO  (5V Output)                         |
|                                      |                                                      |
|                                   [ 18kΩ ] (Voltage Divider: 5V -> 3.21V)                   |
|                                      |                                                      |
|                                     GND                                                     |
|   GPIO5  (RS485_DE) ----------------------------->  DE  & /RE (Tied Together)               |
|   5V Rail           ----------------------------->  VCC (5.0V Supply)                       |
|   GND               ----------------------------->  GND                                     |
+---------------------------------------------------------------------------------------------+
```

---

## 2. Voltage Divider Calculation on MAX485 #2 RO Pin

Because **MAX485ESA** operates on a **5.0V VCC supply**, its Receiver Output (`RO`) generates a 5.0V CMOS HIGH signal ($V_{OH} \approx 4.5\text{V} \dots 5.0\text{V}$).  
While **STM32G474RE PB11 is a 5V tolerant (FT) pin**, **ESP32-S3 GPIO18 has an absolute maximum voltage rating of 3.6V**. 

To protect the ESP32-S3 silicon, a precision resistor voltage divider is placed between MAX485 #2 `RO` output and ESP32 `GPIO18`:

$$R_{\text{top}} = 10\text{k}\Omega, \quad R_{\text{bottom}} = 18\text{k}\Omega$$

### Nominal Voltage Calculation ($V_{\text{in}} = 5.0\text{V}$):
$$V_{\text{out}} = V_{\text{in}} \times \left( \frac{R_{\text{bottom}}}{R_{\text{top}} + R_{\text{bottom}}} \right) = 5.0\text{V} \times \left( \frac{18\text{k}\Omega}{10\text{k}\Omega + 18\text{k}\Omega} \right) = 5.0\text{V} \times 0.6428 = \mathbf{3.214\text{V}}$$
*Result:* $3.214\text{V} \le 3.3\text{V}$ (Safe, strictly below ESP32 3.6V maximum limit).

### Minimum $V_{OH}$ Voltage Calculation ($V_{\text{in}} = 4.5\text{V}$ loaded):
$$V_{\text{out\_min}} = 4.5\text{V} \times 0.6428 = \mathbf{2.892\text{V}}$$
*Result:* $2.892\text{V} \ge V_{IH\text{\_min}} = 0.75 \times V_{DD\_IO} = 2.475\text{V}$ (High noise margin logic 1 recognition).

---

## 3. Comprehensive Master Pinout Connection Table

| # | Device A | Pin A | Signal | Component / Resistor | Device B | Pin B | Signal | Voltage Level | Notes |
| :-: | :--- | :--- | :--- | :-: | :--- | :--- | :--- | :-: | :--- |
| **1** | STM32 | PB10 | USART3_TX | Direct / 1kΩ | MAX485 #1 | Pin 4 (DI) | Driver Input | 3.3V TTL | Direct Drive |
| **2** | MAX485 #1| Pin 1 (RO)| Receiver Out| Direct | STM32 | PB11 | USART3_RX | 5.0V CMOS | PB11 is 5V FT |
| **3** | STM32 | PB1 | DE/RE Control| Direct | MAX485 #1 | Pins 2&3 (DE,/RE)| Enable | 3.3V TTL | High=TX, Low=RX |
| **4** | Nucleo 5V| 5V Rail | 5V Supply | Direct | MAX485 #1 | Pin 8 (VCC) | Supply | 5.0V DC | Dedicated 5V |
| **5** | GND Bus | GND | Common GND | Direct | MAX485 #1 | Pin 5 (GND) | Reference | 0V | Common GND Bus |
| **6** | ESP32-S3 | GPIO8 | UART1_TX | Direct / 1kΩ | MAX485 #2 | Pin 4 (DI) | Driver Input | 3.3V TTL | Direct Drive |
| **7** | MAX485 #2| Pin 1 (RO)| Receiver Out| **10kΩ Series** | ESP32-S3 | GPIO18 | UART1_RX | **3.21V** | **Divider Node** |
| **8** | ESP32 Node| Node Pin | **18kΩ Pulldown** | GND Bus | GND | GND Reference | 0V | Bottom Resistor |
| **9** | ESP32-S3 | GPIO5 | DE/RE Control| Direct | MAX485 #2 | Pins 2&3 (DE,/RE)| Enable | 3.3V TTL | High=TX, Low=RX |
| **10**| ESP32 5V | 5V Rail | 5V Supply | Direct | MAX485 #2 | Pin 8 (VCC) | Supply | 5.0V DC | Dedicated 5V |
| **11**| MAX485 #1| Pin 6 (A) | Differential A| Twisted Pair | MAX485 #2 | Pin 6 (A) | Differential A| ±1.5V..5V | 120Ω Term end A |
| **12**| MAX485 #1| Pin 7 (B) | Differential B| Twisted Pair | MAX485 #2 | Pin 7 (B) | Differential B| ±1.5V..5V | 120Ω Term end B |
