# AKAI Retro Web EQ

An ESP-IDF project for a web-controlled audio equalizer. The firmware processes audio through I2S, applies equalizer and volume settings, and serves its web interface from the device.

## Requirements

- ESP-IDF and its supported toolchain installed and configured
- An ESP-IDF-supported target board with the audio hardware required by this project

## Configure

The project uses the ESP-IDF Component Manager. Component versions are declared in `main/idf_component.yml` and resolved in `dependencies.lock`.

Wi-Fi credentials are kept in `main/include/secrets.h`, which is ignored by Git. The component CMake configuration creates this file from `main/include/secrets.h.example` if it is missing. Update the SSID and password before connecting the device to Wi-Fi.

## Continuous integration

GitHub Actions builds the firmware for the ESP32-S3 on every push using ESP-IDF v6.0.1.

Select the appropriate target for your board, then build and flash using the ESP-IDF command prompt:

```console
idf.py set-target esp32
idf.py build
idf.py -p PORT flash monitor
```

Replace `esp32` with the correct target for your hardware and `PORT` with the device's serial port.

## Project layout

- `main/src/` — audio, DSP, networking, task, and memory-management code
- `main/include/` — shared headers and the local Wi-Fi credentials template
- `main/web/` — embedded web interface assets
- `main/defaults/config.csv` — default NVS configuration
- `.devcontainer/` — development-container configuration
