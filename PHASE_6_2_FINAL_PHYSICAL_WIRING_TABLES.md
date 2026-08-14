# PHASE_6_2_FINAL_PHYSICAL_WIRING_TABLES

## POWER & GND WIRING TABLE

| ID | Source Board | Source MCU GPIO | Source Connector | Source Connector Pin | Signal Name | Resistor ID | Resistor Value | Resistor Terminal A | Resistor Terminal B | Target Component | Target Pin | Direction | Purpose |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **PWR-1** | PC | N/A | USB Type-A | N/A | VBUS | N/A | 0Ω | N/A | N/A | NUCLEO-G474RE | USB_PWR (CN1) | PC -> NUCLEO | NUCLEO Power |
| **PWR-2** | PC | N/A | USB Type-A | N/A | VBUS | N/A | 0Ω | N/A | N/A | ESP32-S3-DevKitC | Micro-USB | PC -> ESP32 | ESP32 Power |
| **PWR-3** | PC | N/A | USB Type-A | N/A | VBUS | N/A | 0Ω | N/A | N/A | NEXTION-HMI | 5V IN | PC -> NEXTION | HMI Power |
| **PWR-4** | NUCLEO-G474RE | N/A | Morpho CN7 | Pin 18 | 5V_OUT | N/A | 0Ω | N/A | N/A | X9C103S | Pin 8 (VCC) | NUCLEO -> X9C | Digital Pot Power |
| **PWR-5** | NUCLEO-G474RE | N/A | Morpho CN7 | Pin 16 | 3V3_OUT | N/A | 0Ω | N/A | N/A | X9C103S | Pin 3 (VH) | NUCLEO -> X9C | High Voltage Ref |
| **GND-1** | NUCLEO-G474RE | N/A | Morpho CN7 | Pin 20 | GND | N/A | 0Ω | N/A | N/A | BREADBOARD | GND Rail | NUCLEO -> BBD | Common Ground |
| **GND-2** | ESP32-S3-DevKitC | N/A | Header J1 | Pin GND | GND | N/A | 0Ω | N/A | N/A | BREADBOARD | GND Rail | ESP32 -> BBD | Common Ground |
| **GND-3** | NEXTION-HMI | N/A | 4-Pin JST | Pin GND | GND | N/A | 0Ω | N/A | N/A | BREADBOARD | GND Rail | NEXTION -> BBD | Common Ground |
| **GND-4** | BREADBOARD | N/A | GND Rail | Any | GND | N/A | 0Ω | N/A | N/A | X9C103S | Pin 4 (VSS) | BBD -> X9C | Digital Pot GND |
| **GND-5** | BREADBOARD | N/A | GND Rail | Any | GND | N/A | 0Ω | N/A | N/A | X9C103S | Pin 6 (VL) | BBD -> X9C | Low Voltage Ref |

## MASTER PHYSICAL WIRING TABLE

| ID | Source Board | Source MCU GPIO | Source Connector | Source Connector Pin | Signal Name | Resistor ID | Resistor Value | Resistor Terminal A | Resistor Terminal B | Target Component | Target Pin | Direction | Purpose |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **SIG-1** | ESP32-S3-DevKitC | GPIO8 | Header J1 | GPIO8 | UART_TX | R1 | 1kΩ | ESP32 GPIO8 | NUCLEO PB11 | NUCLEO-G474RE | CN10 Pin 18 (PB11) | ESP32 -> NUCLEO | ESP UART TX to STM RX |
| **SIG-2** | NUCLEO-G474RE | PB10 | Morpho CN10 | Pin 25 | UART_TX | R2 | 1kΩ | NUCLEO PB10 | ESP32 GPIO18 | ESP32-S3-DevKitC | Header J2 GPIO18 | NUCLEO -> ESP32 | STM UART TX to ESP RX |
| **SIG-3** | ESP32-S3-DevKitC | GPIO17 | Header J2 | GPIO17 | HMI_TX | R3 | 1kΩ | ESP32 GPIO17 | NEXTION RX | NEXTION-HMI | 4-Pin JST RX | ESP32 -> NEXTION | ESP UART TX to HMI RX |
| **SIG-4** | NEXTION-HMI | N/A | 4-Pin JST | TX | HMI_TX | R4 | 1kΩ | NEXTION TX | ESP32 GPIO16 | ESP32-S3-DevKitC | Header J2 GPIO16 | NEXTION -> ESP32 | HMI UART TX to ESP RX |
| **SIG-5** | ESP32-S3-DevKitC | GPIO4 | Header J1 | GPIO4 | ZC_SIM | R5 | 1kΩ | ESP32 GPIO4 | NUCLEO PC7 | NUCLEO-G474RE | CN7 Pin 19 (PC7) | ESP32 -> NUCLEO | Zero Cross Sim |
| **SIG-6** | NUCLEO-G474RE | PB14 | Morpho CN10 | Pin 27 | X9C_INC | N/A | 0Ω | N/A | N/A | X9C103S | Pin 1 (INC) | NUCLEO -> X9C | X9C_INC Control |
| **SIG-7** | NUCLEO-G474RE | PB13 | Morpho CN10 | Pin 30 | X9C_UD | N/A | 0Ω | N/A | N/A | X9C103S | Pin 2 (U/D) | NUCLEO -> X9C | X9C_UD Control |
| **SIG-8** | X9C103S | N/A | Header | Pin 5 (VW) | WIPER_OUT | R6 | 1kΩ | X9C Pin 5 | NUCLEO PA0 | NUCLEO-G474RE | CN7 Pin 28 (PA0) | X9C -> NUCLEO | Wiper Reading |
| **SIG-9** | NUCLEO-G474RE | PB12 | Morpho CN10 | Pin 16 | X9C_CS | N/A | 0Ω | N/A | N/A | X9C103S | Pin 7 (CS) | NUCLEO -> X9C | X9C_CS Control |
| **LPB-1** | NUCLEO-G474RE | PB15 | Morpho CN10 | Pin 26 | HTR_RELAY | R7 | 1kΩ | NUCLEO PB15 | NUCLEO PA4 | NUCLEO-G474RE | CN7 Pin 32 (PA4) | NUCLEO -> NUCLEO | HTR Relay Loopback |
| **LPB-2** | NUCLEO-G474RE | PC6 | Morpho CN10 | Pin 4 | TRIAC_CTRL| R8 | 1kΩ | NUCLEO PC6 | NUCLEO PA6 | NUCLEO-G474RE | CN7 Pin 12 (PA6) | NUCLEO -> NUCLEO | Triac Loopback |
| **LPB-3** | NUCLEO-G474RE | PB12 | Morpho CN10 | Pin 16 | X9C_CS | R9 | 1kΩ | NUCLEO PB12 | NUCLEO PB4 | NUCLEO-G474RE | CN10 Pin 27 (PB4) | NUCLEO -> NUCLEO | X9C CS Loopback |
| **LPB-4** | NUCLEO-G474RE | PB13 | Morpho CN10 | Pin 30 | X9C_UD | R10 | 1kΩ | NUCLEO PB13 | NUCLEO PB5 | NUCLEO-G474RE | CN10 Pin 29 (PB5) | NUCLEO -> NUCLEO | X9C UD Loopback |
| **LPB-5** | NUCLEO-G474RE | PB14 | Morpho CN10 | Pin 27 | X9C_INC | R11 | 1kΩ | NUCLEO PB14 | NUCLEO PB6 | NUCLEO-G474RE | CN10 Pin 17 (PB6) | NUCLEO -> NUCLEO | X9C INC Loopback |

## MASTER NETLIST

| ID | Source Board | Source MCU GPIO | Source Connector | Source Connector Pin | Signal Name | Resistor ID | Resistor Value | Resistor Terminal A | Resistor Terminal B | Target Component | Target Pin | Direction | Purpose |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **NET-1** | ESP32-S3-DevKitC | GPIO8 | Header J1 | GPIO8 | UART_TX | R1 | 1kΩ | ESP32 GPIO8 | NUCLEO PB11 | NUCLEO-G474RE | CN10 Pin 18 (PB11) | ESP32 -> NUCLEO | ESP UART TX Net |
| **NET-2** | NUCLEO-G474RE | PB10 | Morpho CN10 | Pin 25 | UART_TX | R2 | 1kΩ | NUCLEO PB10 | ESP32 GPIO18 | ESP32-S3-DevKitC | Header J2 GPIO18 | NUCLEO -> ESP32 | STM UART TX Net |
| **NET-3** | ESP32-S3-DevKitC | GPIO17 | Header J2 | GPIO17 | HMI_TX | R3 | 1kΩ | ESP32 GPIO17 | NEXTION RX | NEXTION-HMI | 4-Pin JST RX | ESP32 -> NEXTION | ESP UART TX Net |
| **NET-4** | NEXTION-HMI | N/A | 4-Pin JST | TX | HMI_TX | R4 | 1kΩ | NEXTION TX | ESP32 GPIO16 | ESP32-S3-DevKitC | Header J2 GPIO16 | NEXTION -> ESP32 | HMI UART TX Net |
| **NET-5** | ESP32-S3-DevKitC | GPIO4 | Header J1 | GPIO4 | ZC_SIM | R5 | 1kΩ | ESP32 GPIO4 | NUCLEO PC7 | NUCLEO-G474RE | CN7 Pin 19 (PC7) | ESP32 -> NUCLEO | ZC Sim Net |
| **NET-6** | X9C103S | N/A | Header | Pin 5 (VW) | WIPER_OUT | R6 | 1kΩ | X9C Pin 5 | NUCLEO PA0 | NUCLEO-G474RE | CN7 Pin 28 (PA0) | X9C -> NUCLEO | Wiper Out Net |
| **NET-7** | NUCLEO-G474RE | PB15 | Morpho CN10 | Pin 26 | HTR_RELAY | R7 | 1kΩ | NUCLEO PB15 | NUCLEO PA4 | NUCLEO-G474RE | CN7 Pin 32 (PA4) | NUCLEO -> NUCLEO | HTR Relay Loopback |
| **NET-8** | NUCLEO-G474RE | PC6 | Morpho CN10 | Pin 4 | TRIAC_CTRL| R8 | 1kΩ | NUCLEO PC6 | NUCLEO PA6 | NUCLEO-G474RE | CN7 Pin 12 (PA6) | NUCLEO -> NUCLEO | Triac Loopback Net |
| **NET-9** | NUCLEO-G474RE | PB12 | Morpho CN10 | Pin 16 | X9C_CS | R9 | 1kΩ | NUCLEO PB12 | NUCLEO PB4 | NUCLEO-G474RE | CN10 Pin 27 (PB4) | NUCLEO -> NUCLEO | X9C CS Loopback Net |
| **NET-10**| NUCLEO-G474RE | PB13 | Morpho CN10 | Pin 30 | X9C_UD | R10 | 1kΩ | NUCLEO PB13 | NUCLEO PB5 | NUCLEO-G474RE | CN10 Pin 29 (PB5) | NUCLEO -> NUCLEO | X9C UD Loopback Net |
| **NET-11**| NUCLEO-G474RE | PB14 | Morpho CN10 | Pin 27 | X9C_INC | R11 | 1kΩ | NUCLEO PB14 | NUCLEO PB6 | NUCLEO-G474RE | CN10 Pin 17 (PB6) | NUCLEO -> NUCLEO | X9C INC Loopback Net |
