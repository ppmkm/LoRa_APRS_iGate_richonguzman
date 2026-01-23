# CLAUDE.md - AI Assistant Guide for LoRa APRS iGate

**Last Updated:** 2026-01-23
**Firmware Version:** 3.1.7 (2025-12-29)
**Author:** Ricardo Guzman - CA2RXU
**License:** GNU General Public License v3.0

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Codebase Structure](#codebase-structure)
3. [Architecture and Design Patterns](#architecture-and-design-patterns)
4. [Development Workflow](#development-workflow)
5. [Code Conventions and Standards](#code-conventions-and-standards)
6. [Key Components and Modules](#key-components-and-modules)
7. [Testing and Debugging](#testing-and-debugging)
8. [Common Tasks and Workflows](#common-tasks-and-workflows)
9. [Important Notes for AI Assistants](#important-notes-for-ai-assistants)
10. [Resources and Documentation](#resources-and-documentation)

---

## Project Overview

### What is This Project?

LoRa APRS iGate is an ESP32-based firmware that bridges LoRa radio packets to the APRS-IS (Automatic Packet Reporting System - Internet Service) network. It serves as both an iGate (Internet Gateway) and a Digipeater (Digital Repeater) for amateur radio operators using LoRa technology on APRS frequencies.

### Key Features

- **Dual Mode Operation**: Functions as both iGate and Digipeater
- **Multi-Hardware Support**: 44+ board variants (TTGO, Heltec, DIY boards)
- **LoRa Module Support**: SX1278, SX1262, SX1268, LLCC68
- **Web Configuration**: Embedded web interface with REST API
- **GPS Integration**: Real-time position beacons with Base91 encoding
- **Weather Sensors**: BME280/680, BMP280, Si7021, AHT20, INA219
- **Power Management**: Battery monitoring, sleep modes, eco-mode (10% idle power)
- **Remote Management**: APRS message commands, MQTT, Syslog
- **KISS TNC**: TCP/Serial bridge for APRS applications
- **OTA Updates**: Web-based firmware and filesystem updates
- **Cross-Frequency Operation**: RX on one frequency, TX on another

### Target Hardware

- ESP32, ESP32-S2, ESP32-S3, ESP32-C3, NRF52840 platforms
- LoRa modules: 433MHz, 868MHz, 915MHz variants
- Optional GPS, OLED displays, E-Paper displays, PMICs (AXP192/AXP2101)
- Optional 4G/LTE modems (A7670) for cellular APRS-IS connectivity

---

## Codebase Structure

### Directory Layout

```
/home/user/LoRa_APRS_iGate_richonguzman/
├── .github/workflows/         # CI/CD automation
│   ├── build.yml              # Matrix build for all 44 variants
│   └── commit.yml             # Commit validation
│
├── .vscode/                   # VSCode configuration
│   ├── extensions.json        # Recommended extensions
│   └── settings.json          # Editor settings
│
├── data/                      # Default configuration templates
│   └── igate_conf.json        # Default station configuration
│
├── data_embed/                # Web interface assets (embedded in firmware)
│   ├── index.html             # Main web UI
│   ├── style.css              # Custom styles
│   ├── script.js              # Web UI logic
│   ├── bootstrap.css/js       # Bootstrap framework
│   └── favicon.png            # Favicon
│
├── images/                    # Documentation images
│
├── include/                   # C++ header files (public APIs)
│   ├── configuration.h        # Configuration structures
│   ├── aprs_is_utils.h        # APRS-IS connectivity
│   ├── lora_utils.h           # LoRa radio interface
│   ├── web_utils.h            # Web server and REST API
│   ├── display.h              # Display management
│   ├── gps_utils.h            # GPS parsing and encoding
│   ├── wx_utils.h             # Weather sensor support
│   ├── digi_utils.h           # Digipeater logic
│   ├── battery_utils.h        # Battery monitoring
│   ├── power_utils.h          # PMIC management
│   ├── sleep_utils.h          # Power-saving modes
│   ├── mqtt_utils.h           # MQTT client
│   ├── tnc_utils.h            # KISS TNC server
│   ├── syslog_utils.h         # Remote logging
│   ├── query_utils.h          # APRS query handling
│   ├── station_utils.h        # Station tracking
│   ├── kiss_utils.h           # KISS protocol
│   ├── telemetry_utils.h      # Telemetry encoding
│   ├── ntp_utils.h            # NTP time sync
│   ├── ota_utils.h            # OTA updates
│   ├── wifi_utils.h           # WiFi management
│   ├── A7670_utils.h          # 4G/LTE modem
│   └── utils.h                # General utilities
│
├── installer/                 # Deployment tools
│   └── bin/esptool/           # ESP32 flashing utilities
│
├── lib/                       # Project-specific libraries (empty - uses PlatformIO)
│
├── src/                       # Main source code (26 .cpp files)
│   ├── LoRa_APRS_iGate.cpp    # Main entry point (setup/loop)
│   ├── configuration.cpp      # JSON config, SPIFFS I/O
│   ├── aprs_is_utils.cpp      # APRS-IS network
│   ├── lora_utils.cpp         # RadioLib integration
│   ├── web_utils.cpp          # AsyncWebServer, REST API
│   ├── display.cpp            # OLED/E-Paper rendering
│   ├── gps_utils.cpp          # TinyGPS++ integration
│   ├── wx_utils.cpp           # Sensor drivers
│   ├── digi_utils.cpp         # WIDE1-1/WIDE2-n routing
│   ├── battery_utils.cpp      # Battery ADC, INA219
│   ├── power_utils.cpp        # AXP192/AXP2101 PMICs
│   ├── sleep_utils.cpp        # Deep sleep, eco-mode
│   ├── mqtt_utils.cpp         # PubSubClient
│   ├── tnc_utils.cpp          # KISS TNC server
│   ├── syslog_utils.cpp       # Syslog protocol
│   ├── query_utils.cpp        # Message/query parsing
│   ├── station_utils.cpp      # Heard stations, blacklist
│   ├── kiss_utils.cpp         # KISS frame encoding
│   ├── telemetry_utils.cpp    # APRS telemetry
│   ├── ntp_utils.cpp          # NTPClient
│   ├── ota_utils.cpp          # ElegantOTA
│   ├── wifi_utils.cpp         # WiFi client/AP
│   ├── A7670_utils.cpp        # A7670 AT commands
│   └── utils.cpp              # Validation, helpers
│
├── tools/                     # Build automation
│   └── compress.py            # Gzip compression for web assets
│
├── variants/                  # 44 board-specific configurations
│   ├── ttgo-lora32-v21/
│   │   ├── platformio.ini     # Build environment
│   │   └── board_pinout.h     # Pin definitions, feature flags
│   ├── ttgo-t-beam-v1_2/
│   ├── heltec_wifi_lora_32_V3/
│   ├── ESP32_DIY_LoRa/
│   └── ... (40 more variants)
│
├── common_settings.ini        # Shared PlatformIO configuration
├── platformio.ini             # Main PlatformIO project file
├── min_spiffs.csv             # Partition table
└── README.md                  # User documentation

```

### Source File Responsibilities

| File | Lines | Purpose |
|------|------:|---------|
| `LoRa_APRS_iGate.cpp` | 220 | Main entry point, orchestrates all modules |
| `configuration.cpp` | 27,801 | Configuration management, JSON parsing, SPIFFS I/O |
| `aprs_is_utils.cpp` | 17,337 | APRS-IS network connectivity, packet upload/download |
| `web_utils.cpp` | 19,983 | AsyncWebServer, REST API endpoints, web UI serving |
| `utils.cpp` | 21,175 | General utilities, display management, validation |
| `lora_utils.cpp` | 11,120 | RadioLib integration, LoRa RX/TX, frequency management |
| `wx_utils.cpp` | 12,922 | Weather sensor integration (I2C auto-detection) |
| `battery_utils.cpp` | 11,094 | Battery monitoring (internal, external, INA219) |
| `display.cpp` | 11,668 | OLED/E-Paper display rendering |
| `station_utils.cpp` | 8,911 | Station tracking, packet buffering, blacklist management |
| `digi_utils.cpp` | 8,326 | Digipeater logic, WIDE1-1/WIDE2-n routing |
| `A7670_utils.cpp` | 9,454 | 4G/LTE modem support for cellular APRS-IS |
| `gps_utils.cpp` | 8,008 | GPS parsing (TinyGPS++), beacon encoding |
| `syslog_utils.cpp` | 7,106 | Remote logging via Syslog protocol |
| `query_utils.cpp` | 7,776 | APRS query handling (messages, commands) |
| `tnc_utils.cpp` | 5,632 | KISS TNC server (TCP/Serial), APRS bridge |
| `power_utils.cpp` | 10,603 | Power management for different PMICs |
| `telemetry_utils.cpp` | 4,979 | Telemetry encoding for beacons |
| `mqtt_utils.cpp` | 3,673 | MQTT client for packet pub/sub |
| `wifi_utils.cpp` | 6,609 | WiFi connection management, Auto-AP mode |
| `ntp_utils.cpp` | 1,687 | Network time synchronization |
| `ota_utils.cpp` | 2,703 | ElegantOTA integration for firmware updates |
| `sleep_utils.cpp` | 3,880 | Low-power sleep modes, eco-mode |
| `kiss_utils.cpp` | 6,200 | KISS protocol encoding/decoding |

---

## Architecture and Design Patterns

### 1. Variant-Based Hardware Abstraction

The project uses a **variant system** to support 44+ different hardware boards from a single codebase:

```
Compilation Flow:
1. Select build environment: pio run -e <variant_name>
2. Include variant-specific board_pinout.h
3. Conditional compilation based on HAS_* macros
4. Link variant-specific libraries
```

**board_pinout.h** defines:
- LoRa module type: `HAS_SX1278`, `HAS_SX1262`, `HAS_SX1268`, `HAS_LLCC68`
- SPI pins: `RADIO_SCLK_PIN`, `RADIO_MISO_PIN`, `RADIO_MOSI_PIN`, `RADIO_CS_PIN`, `RADIO_RST_PIN`, `RADIO_BUSY_PIN`
- Display: `HAS_DISPLAY`, `HAS_EPAPER`, `OLED_SDA`, `OLED_SCL`, `OLED_RST`
- GPS: `HAS_GPS`
- Power: `HAS_AXP2101`, `HAS_AXP192`
- Features: `HAS_A7670` (4G modem), `HAS_ADC_CALIBRATION`

### 2. Modular Component Design

Each major feature is isolated into a dedicated module:
- **One header file** in `include/` (public API)
- **One implementation file** in `src/` (private implementation)
- **Utility functions** exported as needed

Example: `lora_utils.h` / `lora_utils.cpp` encapsulates all RadioLib interactions.

### 3. Configuration Management

**JSON-based runtime configuration:**
- Stored in SPIFFS filesystem (`/igate_conf.json`)
- Loaded at boot into `Configuration Config` global object
- Modified via web UI
- Persisted back to SPIFFS
- Changes require reboot to take effect

**Configuration Structure (see configuration.h):**
```cpp
class Configuration {
public:
    String              callsign;
    std::vector<WiFi_AP> wifiAPs;
    WiFi_Auto_AP        wifiAutoAP;
    BEACON              beacon;
    APRS_IS             aprsIS;
    DIGI                digi;
    LoraModule          loraModule;
    // ... many more fields
};
```

### 4. State Machine Pattern

The main loop (`LoRa_APRS_iGate.cpp`) uses a polling-based state machine:
```cpp
void loop() {
    // Check for LoRa packets (interrupt-driven)
    // Check WiFi connection
    // Check APRS-IS connection
    // Process beacon timers
    // Handle web server requests (async)
    // Update display
    // Check battery levels
    // Process GPS data
    // Handle sleep/eco-mode transitions
}
```

### 5. Interrupt-Driven LoRa Reception

LoRa packets trigger a hardware interrupt (`lora_utils.cpp`):
```cpp
void setFlag() {
    receivedFlag = true;  // ISR sets flag
}

// In loop():
if (receivedFlag) {
    processLoRaPacket();
    receivedFlag = false;
}
```

### 6. Asynchronous Web Server

Uses `ESPAsyncWebServer` for non-blocking web requests:
- Serves gzip-compressed static files
- REST API endpoints for configuration
- Handles multiple simultaneous connections
- ElegantOTA for firmware updates

### 7. Conditional Compilation

Heavy use of preprocessor directives for hardware-specific code:
```cpp
#ifdef HAS_GPS
    gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
#endif

#ifdef HAS_AXP2101
    setupPMIC_AXP2101();
#endif
```

---

## Development Workflow

### Build System: PlatformIO

**Primary Configuration:** `platformio.ini`
```ini
[platformio]
default_envs = ttgo-lora32-v21

[env]
platform = espressif32 @ 6.7.0
framework = arduino
monitor_speed = 115200
board_build.partitions = min_spiffs.csv
```

**Common Settings:** `common_settings.ini`
- Shared compiler flags: `-Werror -Wall`
- Library dependencies
- RadioLib feature exclusions (reduces binary size)

### Building for Different Boards

```bash
# Build default environment (ttgo-lora32-v21)
pio run

# Build specific variant
pio run -e ttgo-t-beam-v1_2

# Build all variants (CI/CD does this)
pio run -e all

# Upload to board
pio run -e ttgo-lora32-v21 -t upload

# Monitor serial output
pio device monitor
```

### Pre-Build Process

**tools/compress.py** automatically:
1. Compresses web assets (HTML, CSS, JS) with gzip
2. Embeds version information
3. Creates `.gz` files in `data_embed/`
4. These are embedded in firmware binary at compile time

### CI/CD Pipeline (GitHub Actions)

**build.yml workflow:**
- Triggers on release publication
- Matrix build for all 44 variants
- For each variant:
  1. Build firmware binary
  2. Build SPIFFS filesystem
  3. Collect partition files (bootloader, partitions, firmware, SPIFFS)
  4. Merge into single flashable binary using `esptool.py`
  5. Package as `installer.zip`
  6. Upload to GitHub release

### Version Management

**Manual version updates** in `src/LoRa_APRS_iGate.cpp`:
```cpp
String versionDate   = "2025-12-29";
String versionNumber = "3.1.7";
```

**Update checklist:**
1. Update version strings in `LoRa_APRS_iGate.cpp`
2. Update README.md timeline
3. Commit changes
4. Create Git tag: `git tag v3.1.7`
5. Push tag: `git push origin v3.1.7`
6. Create GitHub release (triggers CI/CD build)

### Web Flasher

Users can install firmware via browser:
- https://richonguzman.github.io/lora-igate-web-flasher/installer.html
- Uses WebSerial API to flash ESP32
- Downloads firmware from GitHub releases
- No toolchain required for end users

---

## Code Conventions and Standards

### File Organization

1. **Headers (.h)**: Located in `include/`
   - Include guards: `#ifndef FILENAME_H_` / `#define FILENAME_H_` / `#endif`
   - Function declarations, class definitions, enums
   - Minimal inline code

2. **Implementation (.cpp)**: Located in `src/`
   - Include own header first
   - Include other project headers
   - Include Arduino/library headers last

3. **Board Definitions**: Located in `variants/*/board_pinout.h`
   - Hardware-specific #defines
   - Pin mappings
   - Feature flags (HAS_*)

### Naming Conventions

| Element | Convention | Example |
|---------|-----------|---------|
| **Global Variables** | camelCase | `modemLoggedToAPRSIS` |
| **Local Variables** | camelCase | `receivedPacket` |
| **Functions** | camelCase | `processLoRaPacket()` |
| **Classes** | PascalCase | `Configuration`, `WiFi_AP` |
| **Constants** | UPPER_SNAKE_CASE | `RADIO_CS_PIN` |
| **Macros** | UPPER_SNAKE_CASE | `HAS_SX1278` |
| **Header Guards** | UPPER_SNAKE_CASE | `CONFIGURATION_H_` |

### Code Style

```cpp
// Indentation: 4 spaces (not tabs)
// Braces: K&R style (opening brace on same line)

void function() {
    if (condition) {
        doSomething();
    } else {
        doSomethingElse();
    }
}

// Class member alignment
class Example {
public:
    String  name;           // Align types
    int     value;          // and names
    bool    enabled;        // consistently
};
```

### Comments

```cpp
// Single-line comments for brief explanations

/* Multi-line comments for longer descriptions
 * or block documentation
 */

// File headers: Include GNU GPLv3 license block
/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS iGate.
 * ...
 */
```

### Conditional Compilation

```cpp
// Always use conditional compilation for hardware features
#ifdef HAS_GPS
    // GPS-specific code
#endif

#ifdef HAS_DISPLAY
    // Display-specific code
#endif

// Use #undef to clear default pins before redefining
#undef OLED_SDA
#define OLED_SDA 21
```

### Error Handling

```cpp
// Check return values
if (!WiFi.begin(ssid, password)) {
    Serial.println("WiFi connection failed");
    return false;
}

// Validate configuration before use
if (Config.callsign.length() < 3) {
    Serial.println("Invalid callsign");
    return;
}
```

### Serial Output

```cpp
// Use descriptive messages
Serial.println("=== LoRa APRS iGate ===");
Serial.print("Callsign: ");
Serial.println(Config.callsign);

// Include context in debug messages
Serial.print("[LoRa] Received packet from ");
Serial.println(source);
```

---

## Key Components and Modules

### 1. LoRa Communication (`lora_utils.cpp`)

**Purpose:** Interface with LoRa radio modules using RadioLib

**Supported Modules:**
- SX1278, SX1276 (older, common)
- SX1262, SX1268 (newer, lower power)
- LLCC68 (low-power variant)

**Key Functions:**
```cpp
void setupLoRa();                           // Initialize radio
void startReceiving();                      // Enable RX mode
void sendPacket(String packet);             // Transmit APRS packet
float getLastPacketRssi();                  // Get signal strength
float getLastPacketSnr();                   // Get signal quality
float getLastPacketFreqError();             // Get frequency offset
```

**Configuration:**
- RX/TX frequencies (separate for cross-band operation)
- Spreading factor (6-12, typically 12 for APRS)
- Bandwidth (10.4kHz - 500kHz, typically 125kHz)
- Coding rate (4/5, 4/6, 4/7, 4/8)
- TX power (2-20dBm, limited by regulation)

### 2. APRS-IS Integration (`aprs_is_utils.cpp`)

**Purpose:** Bridge between LoRa RF and APRS-IS servers

**Key Functions:**
```cpp
void connectToAPRS();                       // Connect to rotate.aprs2.net
void uploadToAPRSIS(String packet);         // Send packet to APRS-IS
void checkForAPRSISPackets();               // Check for incoming packets
bool validateCallsign(String callsign);     // Verify SSID format
String calculatePasscode(String callsign);  // Generate APRS-IS passcode
```

**Server Connection:**
- Default: `rotate.aprs2.net:14580`
- Login format: `user CALLSIGN pass PASSCODE vers CA2RXU-LoRa-iGate 3.1.7 filter r/lat/lon/range`
- Supports server-side filters (radius, object, prefix)

**Third-Party Packets:**
- RF→IS: Forwarded as-is (if valid)
- IS→RF: Formatted as third-party: `IGATE>APRS,TCPIP,qAC,CALLSIGN:}SOURCE>DEST,PATH:payload`

### 3. Digipeater Logic (`digi_utils.cpp`)

**Purpose:** Repeat APRS packets for extended coverage

**Supported Paths:**
- `WIDE1-1`: Single-hop digipeating
- `WIDE2-n`: Multi-hop digipeating (decrements n)

**Key Functions:**
```cpp
void processDigiPacket(String packet);      // Handle digipeating logic
bool shouldDigipeat(String path);           // Check if packet should be repeated
String updatePath(String path);             // Decrement hop count
void sendDigipeatedPacket(String packet);   // Transmit with updated path
```

**Eco-Mode:**
- Ultra Eco Mode: Deep sleep between packets (10% idle power)
- Wake on LoRa packet (GPIO interrupt)
- Remote enable/disable via APRS messages

### 4. Web Configuration (`web_utils.cpp`)

**Purpose:** Embedded web UI for configuration and monitoring

**REST API Endpoints:**
```
GET  /                          # Serve web UI (gzip-compressed)
GET  /readConfiguration         # Get current config as JSON
POST /saveConfiguration         # Update config (requires auth)
GET  /receivedPackets           # Recent packet history
GET  /reboot                    # Restart device
GET  /updateBeacon              # Force beacon transmission
GET  /manualBeacon?text=...     # Send custom beacon
```

**Authentication:**
- HTTP Basic Auth (configurable username/password)
- OTA update requires separate password

**Web UI Features:**
- Real-time packet display
- Configuration editor
- Station statistics
- Firmware update (ElegantOTA)

### 5. Display System (`display.cpp`)

**Purpose:** Visual feedback on OLED or E-Paper displays

**Supported Displays:**
- OLED: SSD1306 (128x64, I2C)
- E-Paper: Heltec Wireless Paper (296x128, SPI)

**Display Content:**
- Line 1: Callsign, mode, version
- Line 2: WiFi status, IP address
- Line 3: APRS-IS connection status
- Line 4: Last heard station
- Line 5: RSSI, SNR, distance
- Line 6: GPS satellites, battery voltage
- Line 7: Timestamp

**Key Functions:**
```cpp
void setupDisplay();                        // Initialize display
void displayLine(String text, int line);    // Update single line
void displayClear();                        // Clear screen
void displayToggle();                       // Turn on/off
```

### 6. GPS Integration (`gps_utils.cpp`)

**Purpose:** Parse GPS NMEA data and encode position beacons

**Library:** TinyGPS++

**Key Functions:**
```cpp
void setupGPS();                            // Initialize GPS serial
void parseGPS();                            // Process NMEA sentences
String encodeGPSBeacon();                   // Create Base91-encoded packet
float getGPSLatitude();                     // Get current latitude
float getGPSLongitude();                    // Get current longitude
int getGPSSatellites();                     // Get satellite count
```

**Position Ambiguity:**
- Level 0: Full precision (~11m)
- Level 1: ~1.85km
- Level 2: ~18.5km
- Level 3: ~111km
- Level 4: ~1110km

### 7. Weather Sensors (`wx_utils.cpp`)

**Purpose:** Read environmental sensors and encode weather reports

**Supported Sensors:**
- BME280: Temperature, Humidity, Pressure
- BME680: Temperature, Humidity, Pressure, Gas
- BMP280: Temperature, Pressure
- Si7021: Temperature, Humidity
- AHT20: Temperature, Humidity
- INA219: Voltage, Current, Power

**Auto-Detection:**
- I2C address scanning (0x76, 0x77, 0x40, etc.)
- Sensor type detection via chip ID
- Graceful fallback if sensors not found

**Key Functions:**
```cpp
void setupWeatherSensors();                 // Detect and initialize
String encodeWeatherReport();               // Create APRS weather packet
float readTemperature();                    // Get temp in Celsius
float readHumidity();                       // Get humidity %
float readPressure();                       // Get pressure (height-corrected)
```

### 8. Power Management (`power_utils.cpp`, `battery_utils.cpp`)

**Purpose:** Manage battery and power consumption

**Supported PMICs:**
- AXP192 (older T-Beam boards)
- AXP2101 (newer T-Beam boards)

**Battery Monitoring:**
- Internal ADC (ESP32 pin)
- External voltage divider (up to 15V)
- INA219 current sensor

**Low-Battery Protection:**
- Automatic sleep below threshold
- Prevents over-discharge
- Configurable voltage thresholds

**Key Functions:**
```cpp
void setupPower();                          // Initialize PMIC
float getBatteryVoltage();                  // Read battery voltage
float getExternalVoltage();                 // Read external voltage
void enterSleepMode();                      // Deep sleep
void setLowPowerMode();                     // Reduce power consumption
```

### 9. KISS TNC Bridge (`tnc_utils.cpp`, `kiss_utils.cpp`)

**Purpose:** Provide KISS TNC interface for APRS applications

**Modes:**
- TCP server (port 8001)
- Serial KISS (USB/UART)

**Compatible Software:**
- PinPoint APRS
- APRSIS32
- Xastir
- YAAC

**Key Functions:**
```cpp
void setupKISS();                           // Initialize TNC server
void sendKISSFrame(String packet);          // Encode and send KISS frame
void receiveKISSFrame();                    // Decode received KISS frame
```

### 10. MQTT Integration (`mqtt_utils.cpp`)

**Purpose:** Publish/subscribe APRS packets via MQTT

**Topics:**
- Publish: `<callsign>/received`
- Subscribe: `<callsign>/send`

**Key Functions:**
```cpp
void setupMQTT();                           // Connect to broker
void publishPacket(String packet);          // Publish to MQTT
void subscribeToCommands();                 // Listen for commands
void sendBeaconOverMQTT();                  // Publish beacon
```

### 11. Sleep/Eco Modes (`sleep_utils.cpp`)

**Purpose:** Reduce power consumption for battery operation

**Modes:**
1. **Normal**: All features active (~150mA with WiFi)
2. **Display Off**: Screen timeout after inactivity
3. **Eco Mode**: WiFi off, serial on (~50mA)
4. **Ultra Eco Mode**: Deep sleep between RX (~10% idle)

**Wake Sources:**
- LoRa packet (GPIO interrupt)
- Timer (periodic wake for beacon)
- Serial input (USB)

**Key Functions:**
```cpp
void enterEcoMode();                        // Enable eco mode
void enterUltraEcoMode();                   // Enable ultra eco mode
void wakeFromSleep();                       // Resume normal operation
```

---

## Testing and Debugging

### Serial Debugging

**Baud Rate:** 115200

**Enable Debug Output:**
```cpp
Serial.begin(115200);
Serial.println("[Module] Debug message");
```

**Startup Messages:**
```
=== LoRa APRS iGate ===
Callsign: CA2RXU-10
Version: 3.1.7 (2025-12-29)
Board: TTGO LoRa32 V2.1
LoRa: SX1278 @ 433.775 MHz
[WiFi] Connecting to MySSID...
[WiFi] Connected! IP: 192.168.1.100
[APRS-IS] Connecting to rotate.aprs2.net:14580...
[APRS-IS] Logged in!
```

### Syslog Remote Logging

**Enable Syslog:**
1. Configure syslog server IP in web UI
2. Set syslog port (default: 514)
3. All Serial output is duplicated to syslog

**Example:**
```cpp
syslog.log("Received packet from N0CALL-5");
```

### Web UI Monitoring

**Real-Time Packet Display:**
- Access `http://<device-ip>/` in browser
- View last 20 received packets
- Shows: callsign, RSSI, SNR, distance, timestamp

**Configuration Validation:**
- Check `http://<device-ip>/readConfiguration`
- Returns full config as JSON
- Verify settings before committing

### KISS TNC Testing

**Connect via TCP:**
```bash
telnet <device-ip> 8001
```

**Use with PinPoint APRS:**
1. Configure TNC: TCP/IP, `<device-ip>:8001`
2. Enable KISS mode
3. Monitor packets in real-time

### OTA Update Testing

**Access ElegantOTA:**
1. Navigate to `http://<device-ip>/update`
2. Enter OTA password
3. Upload firmware or filesystem
4. Device reboots automatically

### Common Issues and Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| No LoRa packets received | Wrong frequency/SF | Check config, verify antenna |
| WiFi won't connect | Wrong password | Check credentials, monitor serial |
| APRS-IS login fails | Invalid passcode | Verify callsign, recalculate passcode |
| Display blank | Wrong I2C pins | Check board_pinout.h, verify wiring |
| GPS no fix | Poor antenna | Move outdoors, wait 2-5 minutes |
| OTA fails | Insufficient space | Use min_spiffs.csv partition |
| Battery drains fast | WiFi/display on | Enable eco-mode, reduce beacon rate |

---

## Common Tasks and Workflows

### Adding a New Board Variant

1. **Create variant directory:**
   ```bash
   mkdir -p variants/my_new_board
   ```

2. **Create `board_pinout.h`:**
   ```cpp
   #ifndef BOARD_PINOUT_H_
   #define BOARD_PINOUT_H_

   #define HAS_SX1278
   #define RADIO_SCLK_PIN  5
   #define RADIO_MISO_PIN  19
   #define RADIO_MOSI_PIN  27
   #define RADIO_CS_PIN    18
   #define RADIO_RST_PIN   14
   #define RADIO_BUSY_PIN  26

   #define HAS_DISPLAY
   #define OLED_SDA        21
   #define OLED_SCL        22
   #define OLED_RST        -1

   #define BATTERY_PIN     35

   #endif
   ```

3. **Create `platformio.ini`:**
   ```ini
   [env:my_new_board]
   board = esp32dev
   build_flags =
       ${env.build_flags}
       -D ARDUINO_TTGO_LoRa32_V2
   lib_deps =
       ${env.lib_deps}
   ```

4. **Build and test:**
   ```bash
   pio run -e my_new_board
   pio run -e my_new_board -t upload
   ```

### Adding a New Feature Module

1. **Create header in `include/`:**
   ```cpp
   // include/my_feature.h
   #ifndef MY_FEATURE_H_
   #define MY_FEATURE_H_

   void setupMyFeature();
   void processMyFeature();

   #endif
   ```

2. **Create implementation in `src/`:**
   ```cpp
   // src/my_feature.cpp
   #include "my_feature.h"

   void setupMyFeature() {
       // Initialization code
   }

   void processMyFeature() {
       // Processing code
   }
   ```

3. **Add to main file:**
   ```cpp
   // src/LoRa_APRS_iGate.cpp
   #include "my_feature.h"

   void setup() {
       // ...
       setupMyFeature();
   }

   void loop() {
       // ...
       processMyFeature();
   }
   ```

### Modifying Configuration Schema

1. **Update `include/configuration.h`:**
   ```cpp
   class Configuration {
   public:
       // ... existing fields
       String  myNewField;
   };
   ```

2. **Update JSON parsing in `src/configuration.cpp`:**
   ```cpp
   void Configuration::readConfigFile(String fileName) {
       // ...
       myNewField = doc["myNewField"] | "default";
   }

   void Configuration::writeConfigFile(String fileName) {
       // ...
       doc["myNewField"] = myNewField;
   }
   ```

3. **Update web UI:**
   ```javascript
   // data_embed/script.js
   config.myNewField = document.getElementById('myNewField').value;
   ```

   ```html
   <!-- data_embed/index.html -->
   <input type="text" id="myNewField" placeholder="My New Field">
   ```

### Adding a New Weather Sensor

1. **Add library to `common_settings.ini`:**
   ```ini
   lib_deps =
       ${env.lib_deps}
       adafruit/Adafruit MY_SENSOR Library @ ^1.0.0
   ```

2. **Implement in `src/wx_utils.cpp`:**
   ```cpp
   #include <Adafruit_MY_SENSOR.h>

   Adafruit_MY_SENSOR mySensor;

   void setupWeatherSensors() {
       if (mySensor.begin(0x48)) {  // I2C address
           Serial.println("[WX] MY_SENSOR detected");
           hasMySensor = true;
       }
   }

   String encodeWeatherReport() {
       if (hasMySensor) {
           float temp = mySensor.readTemperature();
           // Encode into APRS format
       }
   }
   ```

### Creating a Release

1. **Update version:**
   ```cpp
   // src/LoRa_APRS_iGate.cpp
   String versionDate   = "2026-01-23";
   String versionNumber = "3.1.8";
   ```

2. **Update README.md timeline:**
   ```markdown
   - 2026-01-23 New feature added, bug fixes.
   ```

3. **Commit and tag:**
   ```bash
   git add .
   git commit -m "Release v3.1.8"
   git tag v3.1.8
   git push origin main
   git push origin v3.1.8
   ```

4. **Create GitHub release:**
   - Go to GitHub → Releases → New Release
   - Select tag `v3.1.8`
   - Add release notes
   - Publish (triggers CI/CD build)

5. **Verify build:**
   - Check GitHub Actions workflow
   - Download installer.zip from release
   - Test web flasher

---

## Important Notes for AI Assistants

### Critical Guidelines

1. **ALWAYS Read Before Editing:**
   - Read the full file before making changes
   - Understand context and surrounding code
   - Verify #ifdef conditions apply to your changes

2. **Respect Variant System:**
   - Never hardcode pins in `src/` files
   - Use #ifdef to check for hardware features
   - Define pins in `variants/*/board_pinout.h` only

3. **Preserve License Headers:**
   - Every `.h` and `.cpp` file has GNU GPLv3 header
   - Never remove or modify license text
   - Add header to new files

4. **Configuration Changes Require:**
   - Update `configuration.h` struct
   - Update `configuration.cpp` JSON parsing
   - Update web UI HTML/JS
   - Test JSON save/load cycle

5. **Test Serial Output:**
   - All changes should include Serial.println() debug
   - Use consistent format: `[Module] Message`
   - Monitor at 115200 baud

6. **Maintain Backwards Compatibility:**
   - Don't remove configuration fields without migration
   - Don't break existing JSON configs
   - Provide default values for new fields

7. **Security Considerations:**
   - Validate all user input from web UI
   - Sanitize callsigns before APRS-IS upload
   - Check packet length before transmission
   - Never expose WiFi passwords in serial output

8. **Amateur Radio Regulations:**
   - This is HAM radio software (licensed operators only)
   - TX power must comply with local regulations
   - Beacons must include valid callsign
   - No encryption allowed on amateur bands

### Code Review Checklist

Before committing changes, verify:

- [ ] Code compiles for default variant (`pio run`)
- [ ] No compiler warnings (`-Werror` enforced)
- [ ] License header present
- [ ] Serial debug output included
- [ ] Hardware features use #ifdef
- [ ] Configuration changes propagate to JSON/web UI
- [ ] No hardcoded credentials or secrets
- [ ] Comments added for complex logic
- [ ] Variable names follow conventions
- [ ] Indentation is 4 spaces
- [ ] No trailing whitespace

### Known Limitations

1. **Memory Constraints:**
   - ESP32: ~320KB RAM, ~4MB Flash (typically)
   - Keep strings in PROGMEM when possible
   - Avoid large buffers, use streaming

2. **SPIFFS Limitations:**
   - Slow writes (~100KB/s)
   - Wear leveling limited
   - Minimize config saves

3. **RadioLib Quirks:**
   - SX1278 vs SX1262 have different APIs
   - Some modules require TCXO (temperature-compensated oscillator)
   - DIO pin mapping varies by module

4. **APRS-IS Protocol:**
   - Max packet length: 510 bytes
   - Login must include passcode
   - Third-party format required for IS→RF

5. **Arduino Framework:**
   - Single-threaded (no real multitasking)
   - Use non-blocking code (no delay() in loop)
   - WiFi library has timeout issues (use retries)

### When to Ask for Clarification

- Hardware pinout is ambiguous
- Feature requires regulatory compliance
- Configuration change affects existing users
- Performance impact unclear
- Security implications present

### Useful Debugging Macros

```cpp
// Add to utils.h for conditional debug output
#ifdef DEBUG_MODE
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif
```

---

## Resources and Documentation

### Official Links

- **GitHub Repository:** https://github.com/richonguzman/LoRa_APRS_iGate
- **Web Flasher:** https://richonguzman.github.io/lora-igate-web-flasher/installer.html
- **User Manual (PDF):** https://drive.google.com/file/d/1Hff_Szd7ks8RC7_RiV6POxPJlclbO05M/view
- **Buying Guide:** https://github.com/richonguzman/LoRa_APRS_iGate/wiki/Supported-Boards-and-Buying-Links
- **Tracker Firmware:** https://github.com/richonguzman/LoRa_APRS_Tracker

### Library Documentation

| Library | Documentation |
|---------|---------------|
| RadioLib | https://github.com/jgromes/RadioLib |
| APRSPacketLib | https://github.com/peterus/APRSPacketLib |
| ESPAsyncWebServer | https://github.com/mathieucarbou/ESPAsyncWebServer |
| ElegantOTA | https://github.com/ayushsharma82/ElegantOTA |
| TinyGPS++ | https://github.com/mikalhart/TinyGPSPlus |
| ArduinoJson | https://arduinojson.org/ |
| PlatformIO | https://docs.platformio.org/ |

### APRS Protocol References

- **APRS Protocol Specification:** http://www.aprs.org/doc/APRS101.PDF
- **APRS-IS Server List:** http://www.aprs2.net/
- **Base91 Encoding:** http://www.aprs.org/doc/APRS101.PDF (Chapter 9)
- **Mic-E Format:** http://www.aprs.org/doc/APRS101.PDF (Chapter 10)
- **KISS Protocol:** http://www.ax25.net/kiss.aspx

### Amateur Radio Regulations

- **USA (FCC Part 97):** https://www.ecfr.gov/current/title-47/chapter-I/subchapter-D/part-97
- **Europe (CEPT):** https://www.ecodocdb.dk/download/2d1e8e2c-6336/ERCREC7003E.PDF
- **Callsign Validation:** Must match `[A-Z0-9]{1,3}[0-9][A-Z0-9]{0,3}-[0-9]{1,2}` format

### Community Support

- **GitHub Issues:** https://github.com/richonguzman/LoRa_APRS_iGate/issues
- **GitHub Discussions:** https://github.com/richonguzman/LoRa_APRS_iGate/discussions
- **Author:** Ricardo Guzman - CA2RXU (Valparaiso, Chile)
- **Email:** Via GitHub profile

### Related Projects

- **LoRa APRS Tracker:** https://github.com/richonguzman/LoRa_APRS_Tracker
- **APRS.fi:** https://aprs.fi/ (APRS packet viewer)
- **PinPoint APRS:** https://www.pinpointaprs.com/
- **APRSIS32:** http://aprsisce.wikidot.com/

### Hardware Suppliers

See: https://github.com/richonguzman/LoRa_APRS_iGate/wiki/Supported-Boards-and-Buying-Links

- **LILYGO (TTGO):** https://www.lilygo.cc/
- **Heltec Automation:** https://heltec.org/
- **RAK Wireless:** https://www.rakwireless.com/
- **Generic LoRa Modules:** AliExpress, Amazon

---

## Changelog for CLAUDE.md

- **2026-01-23:** Initial creation based on firmware v3.1.7
  - Comprehensive codebase structure analysis
  - Architecture and design patterns documented
  - Development workflows defined
  - Code conventions standardized
  - Key components detailed
  - Testing and debugging guidelines
  - Common tasks and workflows
  - AI assistant guidelines established

---

## License

This documentation follows the same license as the project:

**GNU General Public License v3.0**

Copyright (C) 2025 Ricardo Guzman - CA2RXU

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. If not, see https://www.gnu.org/licenses/.

---

**73 de CA2RXU!** (Best regards from CA2RXU)
