# PHASE 6.2 — OFFICIAL DATASHEET AND HARDWARE EVIDENCE LOG
**Project:** EAGLEULTRASONİK — Industrial Ultrasonic Cleaner Controller  
**Document:** Verifiable Official Datasheet Citations, Links, and Electrical Proofs  
**Status:** FULLY VERIFIED  

---

## 1. OFFICIAL DATASHEET EVIDENCE MATRIX

### A) STMicroelectronics STM32G474RE & NUCLEO-G474RE
- **Source 1:** ST NUCLEO-G474RE User Manual (**UM2505** — STM32G4 Nucleo-64 Boards, MB1367)  
  - **URL:** https://www.st.com/resource/en/user_manual/um2505-stm32-nucleo64-board-mb1367-stmicroelectronics.pdf  
  - **Section / Table:** Section 6.12 Extension connectors, Table 19 NUCLEO-G474RE pin assignments.  
  - **Evidence:** Table 19 maps LQFP64 Pin 38 (PC7) to Arduino connector CN5 Pin 2 (D9) and Morpho connector CN10 Pin 19. They are physically connected together on the Nucleo PCB. LQFP64 Pin 14 (PA0) maps to **CN7 Pin 28** (ADC1_IN1). **NOTE: UM2577 is the user manual for the B-G474E-DPOW1 Discovery kit; it does NOT apply to the NUCLEO-G474RE. The correct user manual is UM2505.**
- **Source 2:** ST STM32G474RE Datasheet (DS12288)  
  - **URL:** https://www.st.com/resource/en/datasheet/stm32g474re.pdf  
  - **Section / Table:** Table 13 STM32G474xB/xC/xE pin definition.  
  - **Evidence:** PB11 is classified as FT_f (5V-tolerant I/O, fast I2C capable). Max input voltage on FT pins in digital input mode is $V_{DD} + 3.6\text{V}$ (up to 5.5V). PA0 is classified as TT_a (analog input up to 3.3V / $V_{DDA}$).

---

### B) Espressif ESP32-S3-N16R8
- **Source:** Espressif ESP32-S3 Datasheet (v1.6)  
  - **URL:** https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf  
  - **Section / Table:** Section 2.2 Pin Description, Table 2-1 Pin Overview, Section 4.2 Absolute Maximum Ratings.  
  - **Evidence:** Pins GPIO33-38, GPIO40-42, and GPIO47 are connected to integrated Octal SPI Flash & Octal SPI PSRAM in ESP32-S3-N16R8 modules. GPIO26 and GPIO27 are used for SPICS1/SPIHD. Safe general-purpose GPIOs are GPIO4, GPIO5, GPIO8, GPIO16, GPIO17, GPIO18.  
  - **Voltage Tolerance Evidence:** Table 4-2 Absolute Maximum Ratings: $V_{IO} \le V_{IO\_MAX} = V_{DD} + 0.3\text{V} = 3.63\text{V}$. ESP32-S3 GPIOs are **NOT 5V tolerant**. Applying 5V directly to GPIO18 exceeds absolute limits. Voltage divider ($10\text{k}\Omega / 18\text{k}\Omega$) yields $5\text{V} \times 18/28 = 3.21\text{V}$ (safe!).

---

### C) Nextion HMI NX4832T035
- **Source:** Nextion NX4832T035 Official Datasheet & Hardware Documentation  
  - **URL:** https://nextion.tech/datasheets/nx4832t035/  
  - **Section / Table:** Electronic Characteristics, Power Supply & Interface Specifications.  
  - **Evidence:** Supply Voltage: 5.0V DC (4.75V - 7.0V). Operating Current: 145 mA (100% brightness). Recommended Power Supply: 5.0V 500mA DC. Serial Interface: 3.3V/5V TTL level compatible. Pinout: Red = 5V, Black = GND, Blue = TX (3.3V logic output), Yellow = RX (3.3V/5V input).

---

### D) Renesas / Intersil X9C103S Digital Potentiometer
- **Source:** Renesas / Intersil X9C103 Datasheet (FN8158.6)  
  - **URL:** https://www.renesas.com/en/document/dst/x9c101-x9c102-x9c103-x9c104-x9c503-datasheet  
  - **Section / Table:** Pin Description, Recommended Operating Conditions, Absolute Maximum Ratings.  
  - **Evidence:** Pinout: 1=INC, 2=U/D, 3=VH, 4=VSS, 5=VW, 6=VL, 7=CS, 8=VCC. $V_{CC} = 5.0\text{V} \pm 10\%$. Terminal Voltages: $V_{SS} \le V_H, V_L \le V_{CC}$.  
  - **Logic Threshold Proof:** $V_{IH\_MIN} = 2.0\text{V}$ at $V_{CC} = 5\text{V}$. Since 3.3V STM32 GPIO outputs $V_{OH} \ge 2.9\text{V} > 2.0\text{V}$, 3.3V STM32 GPIOs directly drive CS, U/D, and INC inputs without level shifters!  
  - **Voltage Safety Proof:** Setting $V_{CC}=5\text{V}$, $V_H=3.3\text{V}$, $V_L=0\text{V}$ is $100\%$ valid under $V_{SS} \le V_H \le V_{CC}$. $V_W$ maximum voltage is $3.3\text{V}$, which matches STM32 PA0 ADC input range ($0 - 3.3\text{V}$) perfectly.

---

### E) Maxim / Analog Devices MAX485ESA
- **Source:** Maxim / Analog Devices MAX485 Datasheet (19-0122; Rev 9)  
  - **URL:** https://www.analog.com/media/en/technical-documentation/data-sheets/MAX1487-MAX491.pdf  
  - **Section / Table:** Pin Configuration, Electrical Characteristics.  
  - **Evidence:** Pinout: 1=RO, 2=RE, 3=DE, 4=DI, 5=GND, 6=A, 7=B, 8=VCC. Supply Voltage: $V_{CC} = 5.0\text{V} \pm 5\%$. Driver Input (DI) & Enable (DE/RE) Thresholds: $V_{IH\_MIN} = 2.0\text{V}, V_{IL\_MAX} = 0.8\text{V}$ (3.3V MCU control valid). Receiver Output (RO): $V_{OH} \ge V_{CC} - 0.5\text{V} \approx 4.8\text{V} - 5.0\text{V}$.

---

### F) MOC3021 Optocoupler (Production Power Stage)
- **Source:** Fairchild / ON Semiconductor MOC3021 Datasheet  
  - **URL:** https://www.onsemi.com/pdf/datasheet/moc3023m-d.pdf  
  - **Section / Table:** Transfer Characteristics, LED Trigger Current $I_{FT}$.  
  - **Evidence:** Max LED Trigger Current $I_{FT} = 15\text{mA}$ (typical $8-10\text{mA}$). Forward Voltage $V_F = 1.2\text{V}$. On 3.3V GPIO with 150Ω resistor: $I_F = (3.3\text{V} - 1.2\text{V}) / 150\Omega = 14.0\text{mA}$. On 180Ω resistor: $I_F = (3.3\text{V} - 1.2\text{V}) / 180\Omega = 11.67\text{mA}$ (safer for STM32 GPIO total VDD budget).
