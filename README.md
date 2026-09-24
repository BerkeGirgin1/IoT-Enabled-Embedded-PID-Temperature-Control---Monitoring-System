# IoT-Based Temperature Monitoring System (Modbus RTU to MQTT)

[![MCU](https://img.shields.io/badge/MCU-ATmega8A-red.svg)]()
[![Protocol](https://img.shields.io/badge/Protocol-Modbus_RTU-blue.svg)]()
[![Cloud](https://img.shields.io/badge/IoT-MQTT_HiveMQ-orange.svg)]()
[![Language](https://img.shields.io/badge/Language-C_&_Python-green.svg)]()


## 📌 Project Overview
This project presents a complete end-to-end IoT monitoring system bridging low-level embedded hardware with cloud-based telemetry. An **ATmega8A microcontroller** acts as a bare-metal Modbus RTU slave, reading precise temperature data from a **J-Type thermocouple**. 

The raw analog signal is conditioned through an LM358 Op-Amp and processed using a Moving Average filter and NIST polynomial calculations. A custom Python application serves as the Modbus Master and IoT Gateway, fetching the data over USART and publishing it to a **HiveMQ MQTT cloud broker** for real-time global monitoring.

## ⚙️ System Architecture

### 1. Embedded Layer (ATmega8A & Signal Processing)
*   **Sensor Interfacing:** A J-Type thermocouple provides millivolt-level signals, amplified via an LM358 operational amplifier before being sampled by the MCU's ADC.
*   **Digital Signal Processing (DSP):** The MCU applies a Moving Average filter to eliminate noise and utilizes NIST polynomial algorithms to convert the non-linear thermocouple voltages into accurate Celsius readings.
*   **Hardware Interrupts:** USART communication operates at 19200 bps. Incoming PC requests are captured instantly via the `USART_RXC_vect` hardware interrupt, ensuring no CPU blocking during main execution loops.

### 2. Communication Layer (Modbus RTU)
A custom, bare-metal Modbus RTU protocol was implemented on the ATmega8A.
*   **Architecture:** Master-Slave configuration.
*   **Frame Structure:** Contains a Slave ID (Set to 2), Function Codes (e.g., `0x03` for Read Holding Register), Data Address, and a strict 16-bit CRC to guarantee data integrity against electrical noise. 

### 3. IoT Cloud Layer (MQTT Gateway)

*   **Python IoT Gateway:** A Python-based PC application acts as the Modbus Master to poll the ATmega8A. It simultaneously acts as an MQTT Publisher.
*   **HiveMQ Broker:** The data is published to the public `broker.hivemq.com` cloud server.
*   **Topic Architecture:** The telemetry is routed to the specific topic `iyte/ee446/berke/sicaklik`. Any authorized client worldwide can subscribe to this topic to monitor the industrial process in real-time.

## 📂 Repository Structure
```text
├── src/                # ATmega8A bare-metal C code and Python MQTT Gateway script
├── Hardware/           # Proteus circuit schematics
├── Docs/               # Technical project report and system architecture details
└── README.md
