# ESP FreeRTOS IoT Monitoring System

This repository contains the firmware for an ESP-based IoT monitoring system developed for a water-harvesting monitoring project.

The firmware uses **FreeRTOS** to organize the system into three main tasks responsible for:

1. **Sensor data acquisition** — periodically measures data from the connected sensors.
2. **HTTP communication** — attempts to send the collected data to the remote server using HTTP.
3. **LoRa communication** — transmits data through LoRa when the HTTP communication is unavailable or fails.

For this project, Arduino IDE could work but is not optimal in time and modularization, that is the reason why the project is developed using **PlatformIO** and is structured as a PlatformIO project inside the `sources/` directory.

---

## Repository Structure

```text
.
├── archived_scripts/
|
├── Documentation/
│
└── sources/
    ├── include/
    ├── lib/
    ├── platformio.ini
    └── src/
        └── main.cpp
```

---

## `sources/`

This directory contains the **current and functional version of the project**.

It is a complete [PlatformIO](https://platformio.org/) project. PlatformIO commands such as building, uploading, and monitoring the board should be executed from this directory.

```bash
cd sources
```

For example:

```bash
pio run
```

To upload the firmware to a connected board:

```bash
pio run --target upload
```

To open the serial monitor:

```bash
pio device monitor
```

The specific board, framework, libraries, and other PlatformIO configuration are defined in:

```text
sources/platformio.ini
```
---

## `archived_scripts/`

This directory contains **older experimental and testing code**.

These scripts were created while developing and testing individual sensors and system features before integrating them into the main project.

Examples include:

* Individual sensor testing
* LoRa communication experiments
* SD-card logging experiments

The contents of this directory should **not be considered the current implementation**.

Some scripts may be outdated, incomplete, depend on libraries or configurations that are no longer used, or may not work correctly with the current hardware/software configuration.

However, they can be useful as **examples or references** when investigating how a particular sensor or feature was initially tested.
---

## Getting Started

### Requirements

The project requires:

* An ESP-compatible development board supported by the PlatformIO configuration
* Installing PlatformIO
* The sensors and peripherals required by the current firmware
* The corresponding communication hardware for HTTP and LoRa functionality

### Clone the repository

```bash
git clone <repository-url>
cd <repository-directory>
```

### Enter the PlatformIO project

The PlatformIO project is located in `sources/`:

```bash
cd sources
```

All PlatformIO commands should be executed from this directory.

### Build

```bash
pio run
```

### Upload

Connect the ESP board to the computer and run:

```bash
pio run --target upload
```

### Serial monitor

To monitor the board's serial output:

```bash
pio device monitor
```

Depending on the board and PlatformIO configuration, the baud rate may be specified explicitly if necessary.

---

## Important Notes

* **`sources/` is the authoritative version of the firmware.** Changes and development should be performed there.
* **`archived_scripts/` contains historical/experimental code** and should not be assumed to work with the current implementation.
* The archived scripts are kept primarily as references for the development and testing of individual components.
* The `platformio.ini` file in `sources/` defines the PlatformIO project configuration.
* PlatformIO commands should be executed from `sources/`, not from the repository root.
* Additional information about the hardware and system design can be found in `Documentation/`.
