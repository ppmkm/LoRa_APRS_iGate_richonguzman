# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

LoRa APRS iGate/Digipeater firmware for ESP32 boards with LoRa modules. Bridges LoRa RF packets with APRS-IS network for amateur radio position reporting and telemetry.

**Author**: Ricardo Guzman - CA2RXU
**License**: GPL v3

## Build Commands

This project uses PlatformIO. Build for specific board variants:

```bash
# Build default variant (TTGO LoRa32 V2.1)
pio run -e ttgo-lora32-v21

# Build specific variant
pio run -e heltec_wifi_lora_32_V3
pio run -e heltec_wireless_tracker
pio run -e lilygo_t_beam_v1_2

# Upload firmware via USB
pio run -e ttgo-lora32-v21 --target upload

# Serial monitor (115200 baud)
pio run --target monitor
```

All 46+ board variants are in `variants/*/platformio.ini` and automatically included.

## Architecture

### Main Loop Flow (LoRa_APRS_iGate.cpp)

```
setup() → Load config, init LoRa/WiFi/Display, connect APRS-IS
loop()  → Poll LoRa RX → Process packet → Upload/Repeat/Forward → Display update
```

### Namespace Organization

Each functional area has dedicated `.cpp/.h` pair in `src/` and `include/`:

| Namespace | Purpose |
|-----------|---------|
| `LoRa_Utils` | RadioLib wrapper, packet TX/RX |
| `APRS_IS_Utils` | APRS-IS TCP client |
| `DIGI_Utils` | Digipeater packet repeating |
| `STATION_Utils` | Station tracking, packet buffers |
| `WIFI_Utils` | WiFi connection, AutoAP fallback |
| `WEB_Utils` | HTTP REST API, configuration UI |
| `BATTERY_Utils` | Voltage monitoring, telemetry |
| `WX_Utils` | Weather sensor integration |
| `GPS_Utils` | GPS parsing, position encoding |
| `MQTT_Utils` | MQTT pub/sub |
| `TNC_Utils` | KISS protocol server |

### Configuration System

- JSON file `igate_conf.json` in SPIFFS
- Config classes defined in `include/configuration.h`
- Serialization in `src/configuration.cpp`
- Global `Config` object loaded at startup

To add new configuration:
1. Add class/fields in `configuration.h`
2. Add JSON serialization in `configuration.cpp`
3. Add web UI fields in `data_embed/index.html`

### Board Variants

Each variant in `variants/[BOARD_NAME]/` contains:
- `platformio.ini` - Board-specific build flags and libraries
- `board_pinout.h` - GPIO pin assignments for radio, display, sensors

### Web Interface

Embedded gzip-compressed files in `data_embed/`:
- `index.html` - Bootstrap-based configuration UI
- `script.js` - Form handling, packet display
- API endpoints in `web_utils.cpp`

## Operating Modes

| Mode | Behavior |
|------|----------|
| 0 | iGate only (LoRa→APRS-IS) |
| 1 | Digipeater only (RF repeat) |
| 2 | iGate + Digipeater |
| 3 | Dual-frequency digi |
| 5 | Smart mode (auto-fallback) |

## Key Libraries

- **RadioLib** - LoRa chipset abstraction (SX1262, SX1268, SX1278, LLCC68)
- **APRSPacketLib** - APRS packet encoding/decoding
- **ArduinoJson** - Configuration JSON
- **ESPAsyncWebServer** - Non-blocking HTTP server
- **ElegantOTA** - Over-the-air updates at `/update`

## Code Patterns

- 25-segment packet output buffer in `station_utils.cpp` prevents RF congestion
- EcoMode deep sleep for remote digipeaters (reduces idle current to ~10-24mA)
- Weather sensors auto-detected at startup (BME280, BME680, BMP280, AHT20, Si7021)
- Configuration persisted to SPIFFS as JSON
