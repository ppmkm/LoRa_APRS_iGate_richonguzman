/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 * 
 * This file is part of LoRa APRS iGate.
 * 
 * LoRa APRS iGate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version.
 * 
 * LoRa APRS iGate is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with LoRa APRS iGate. If not, see <https://www.gnu.org/licenses/>.
 */

#include <APRSPacketLib.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <time.h>
#include "telemetry_utils.h"
#include "configuration.h"
#include "station_utils.h"
#include "battery_utils.h"
#include "aprs_is_utils.h"
#include "board_pinout.h"
#include "syslog_utils.h"
#include "A7670_utils.h"
#include "lora_utils.h"
#include "wifi_utils.h"
#include "gps_utils.h"
#include "wx_utils.h"
#include "display.h"
#include "utils.h"


extern Configuration        Config;
extern TinyGPSPlus          gps;
extern String               versionDate;
extern String               firstLine;
extern String               secondLine;
extern String               thirdLine;
extern String               fourthLine;
extern String               fifthLine;
extern String               sixthLine;
extern String               seventhLine;
extern String               iGateBeaconPacket;
extern String               iGateLoRaBeaconPacket;
extern int                  rssi;
extern float                snr;
extern int                  freqError;
extern String               distance;
extern bool                 WiFiConnected;
extern int                  wxModuleType;
extern bool                 backUpDigiMode;
extern bool                 shouldSleepLowVoltage;
extern bool                 transmitFlag;
extern bool                 passcodeValid;

extern std::vector<LastHeardStation>    lastHeardStations;

bool        statusAfterBoot     = true;
bool        sendStartTelemetry  = true;
bool        beaconUpdate        = false;
uint32_t    lastBeaconTx        = 0;
uint32_t    lastWxTx            = 0;
uint32_t    lastScreenOn        = millis();
String      beaconPacket;
String      secondaryBeaconPacket;


namespace Utils {

    void processStatus() {
        String status = APRSPacketLib::generateBasePacket(Config.callsign, "APLRG1", Config.beacon.path);

        if (WiFi.status() == WL_CONNECTED && Config.aprs_is.active && Config.beacon.sendViaAPRSIS) {
            delay(1000);
            status.concat(",qAC:>");
            status.concat(Config.beacon.statusPacket);
            APRS_IS_Utils::upload(status);
            SYSLOG_Utils::log(2, status, 0, 0.0, 0);   // APRSIS TX
            statusAfterBoot = false;
        }
        if (statusAfterBoot && !Config.beacon.sendViaAPRSIS && Config.beacon.sendViaRF) {
            status.concat(":>");
            status.concat(Config.beacon.statusPacket);
            STATION_Utils::addToOutputPacketBuffer(status, true);   // treated also as beacon on Tx Freq
            statusAfterBoot = false;
        }
    }

    String getLocalIP() {
        if (Config.digi.ecoMode == 1 || Config.digi.ecoMode == 2) {
            return "** WiFi AP  Killed **";
        } else if (!WiFiConnected) {
            return "IP :  192.168.4.1";
        } else if (backUpDigiMode) {
            return "- BACKUP DIGI MODE -";
        } else {
            return "IP :  " + String(WiFi.localIP()[0]) + "." + String(WiFi.localIP()[1]) + "." + String(WiFi.localIP()[2]) + "." + String(WiFi.localIP()[3]);
        }
    }

    void setupDisplay() {
        if (Config.digi.ecoMode != 1) displaySetup();
        #ifdef INTERNAL_LED_PIN
            digitalWrite(INTERNAL_LED_PIN,HIGH);
        #endif
        Serial.println("\nStarting Station: " + Config.callsign + "   Version: " + versionDate);
        Serial.print("(DigiEcoMode: ");
        if (Config.digi.ecoMode == 0) {
            Serial.println("OFF)");
        } else if (Config.digi.ecoMode == 1) {
            Serial.println("ON)");
        } else {
            Serial.println("ON / Only Serial Output)");
        }
        displayShow(" LoRa APRS", "", "", "   ( iGATE & DIGI )", "", "" , "  CA2RXU  " + versionDate, 4000);
        #ifdef INTERNAL_LED_PIN
            digitalWrite(INTERNAL_LED_PIN,LOW);
        #endif
        firstLine   = Config.callsign;
        seventhLine = "     listening...";
    }

    void showActiveStations() {
        char buffer[30]; // Adjust size as needed
        sprintf(buffer, "Stations (%dmin) = %2d", Config.rememberStationTime, lastHeardStations.size());
        fourthLine = buffer;
    }

    void checkBeaconInterval() {
        uint32_t lastTx = millis() - lastBeaconTx;
        if (lastBeaconTx == 0 || lastTx >= Config.beacon.interval * 60 * 1000) {
            beaconUpdate = true;
        }

        #ifdef HAS_GPS
            if (Config.beacon.gpsActive && gps.location.lat() == 0.0 && gps.location.lng() == 0.0 && Config.beacon.latitude == 0.0 && Config.beacon.longitude == 0.0) {
                GPS_Utils::getData();
                beaconUpdate = false;
            }
        #endif

        if (beaconUpdate) {
            if (!Config.display.alwaysOn && Config.display.timeout != 0) displayToggle(true);

            if (sendStartTelemetry &&
                Config.battery.sendVoltageAsTelemetry &&
                (Config.battery.sendInternalVoltage || Config.battery.sendExternalVoltage) &&
                (lastBeaconTx > 0)) {
                TELEMETRY_Utils::sendEquationsUnitsParameters();
            }

            STATION_Utils::deleteNotHeard();

            showActiveStations();

            beaconPacket            = iGateBeaconPacket;
            secondaryBeaconPacket   = iGateLoRaBeaconPacket;
            #ifdef HAS_GPS
                if (Config.beacon.gpsActive && Config.digi.ecoMode == 0) {
                    GPS_Utils::getData();
                    if (gps.location.isUpdated() && gps.location.lat() != 0.0 && gps.location.lng() != 0.0) {
                        String basePacket   = APRSPacketLib::generateBasePacket(Config.callsign, "APLRG1", Config.beacon.path);
                        String encodedGPS   = APRSPacketLib::encodeGPSIntoBase91(gps.location.lat(),gps.location.lng(), 0, 0, Config.beacon.symbol, false, 0, true, Config.beacon.ambiguityLevel);

                        beaconPacket    = basePacket;
                        beaconPacket    += ",qAC:!";
                        beaconPacket    += Config.beacon.overlay;
                        beaconPacket    += encodedGPS;

                        secondaryBeaconPacket   = basePacket;
                        secondaryBeaconPacket   += ":=";
                        secondaryBeaconPacket   += Config.beacon.overlay;
                        secondaryBeaconPacket   += encodedGPS;
                    }
                }
            #endif

            // Weather data is now sent as separate positionless WX packets via checkWxInterval()
            beaconPacket            += Config.beacon.comment;
            secondaryBeaconPacket   += Config.beacon.comment;

            #if defined(BATTERY_PIN) || defined(HAS_AXP192) || defined(HAS_AXP2101)
                if (Config.battery.sendInternalVoltage || Config.battery.monitorInternalVoltage) {
                    float internalVoltage       = BATTERY_Utils::checkInternalVoltage();
                    if (Config.battery.monitorInternalVoltage && internalVoltage < Config.battery.internalSleepVoltage) {
                        beaconPacket            += " **IntBatWarning:SLEEP**";
                        secondaryBeaconPacket   += " **IntBatWarning:SLEEP**";
                        shouldSleepLowVoltage   = true;
                    }

                    if (Config.battery.sendInternalVoltage) {
                        char internalVoltageInfo[10];   // Enough to hold "xx.xxV\0"
                        snprintf(internalVoltageInfo, sizeof(internalVoltageInfo), "%.2fV", internalVoltage);

                        char sixthLineBuffer[25];       // Enough to hold "    (Batt=xx.xxV)"
                        snprintf(sixthLineBuffer, sizeof(sixthLineBuffer), "    (Batt=%s)", internalVoltageInfo);
                        sixthLine = sixthLineBuffer;

                        if (!Config.battery.sendVoltageAsTelemetry) {
                            beaconPacket            += " Batt=";
                            beaconPacket            += internalVoltageInfo;
                            secondaryBeaconPacket   += " Batt=";
                            secondaryBeaconPacket   += internalVoltageInfo;
                        }
                    }
                }
            #endif

            #ifndef HELTEC_WP_V1
                if (Config.battery.sendExternalVoltage || Config.battery.monitorExternalVoltage) {
                    float externalVoltage       = BATTERY_Utils::checkExternalVoltage();
                    if (Config.battery.monitorExternalVoltage && externalVoltage < Config.battery.externalSleepVoltage) {
                        beaconPacket            += " **ExtBatWarning:SLEEP**";
                        secondaryBeaconPacket   += " **ExtBatWarning:SLEEP**";
                        shouldSleepLowVoltage   = true;
                    }

                    if (Config.battery.sendExternalVoltage) {
                        char externalVoltageInfo[10];  // "xx.xxV\0" (max 7 chars)
                        snprintf(externalVoltageInfo, sizeof(externalVoltageInfo), "%.2fV", externalVoltage);
                    
                        char sixthLineBuffer[25];  // Ensure enough space
                        snprintf(sixthLineBuffer, sizeof(sixthLineBuffer), "    (Ext V=%s)", externalVoltageInfo);
                        sixthLine = sixthLineBuffer;

                        if (!Config.battery.sendVoltageAsTelemetry) {
                            beaconPacket            += " Ext=";
                            beaconPacket            += externalVoltageInfo;
                            secondaryBeaconPacket   += " Ext=";
                            secondaryBeaconPacket   += externalVoltageInfo;
                        }
                    }
                }
            #endif

            if (Config.battery.sendVoltageAsTelemetry && (Config.battery.sendInternalVoltage || Config.battery.sendExternalVoltage)) {
                String encodedTelemetry = TELEMETRY_Utils::generateEncodedTelemetry();
                beaconPacket += encodedTelemetry;
                secondaryBeaconPacket += encodedTelemetry;
            }

            if (Config.beacon.sendViaAPRSIS && Config.aprs_is.active && passcodeValid && !backUpDigiMode) {
                Utils::println("-- Sending Beacon to APRSIS --");
                displayShow(firstLine, secondLine, thirdLine, fourthLine, fifthLine, sixthLine, "SENDING IGATE BEACON", 0);
                seventhLine = "     listening...";
                #ifdef HAS_A7670
                    A7670_Utils::uploadToAPRSIS(beaconPacket);
                #else
                    APRS_IS_Utils::upload(beaconPacket);
                #endif
                if (Config.syslog.logBeaconOverTCPIP) SYSLOG_Utils::log(1, "tcp" + beaconPacket, 0, 0.0, 0);   // APRSIS TX
            }

            if (Config.beacon.sendViaRF || backUpDigiMode) {
                Utils::println("-- Sending Beacon to RF --");
                displayShow(firstLine, secondLine, thirdLine, fourthLine, fifthLine, sixthLine, "SENDING DIGI BEACON", 0);
                seventhLine = "     listening...";
                STATION_Utils::addToOutputPacketBuffer(secondaryBeaconPacket, true);
            }

            lastBeaconTx = millis();
            lastScreenOn = millis();
            beaconUpdate = false;
        }

        if (statusAfterBoot && Config.beacon.statusActive && !Config.beacon.statusPacket.isEmpty()) {
            processStatus();
        }
    }

    // Helper to format latitude for APRS (DDMM.MMN/S)
    String formatLatitudeAPRS(double lat) {
        char ns = (lat >= 0) ? 'N' : 'S';
        lat = fabs(lat);
        int degrees = (int)lat;
        double minutes = (lat - degrees) * 60.0;
        int minInt = (int)minutes;
        int minFrac = (int)((minutes - minInt) * 100 + 0.5);
        // Handle rollover when rounding pushes fraction to 100
        if (minFrac >= 100) {
            minFrac -= 100;
            minInt++;
            if (minInt >= 60) {
                minInt -= 60;
                degrees++;
            }
        }
        char buf[48];
        snprintf(buf, sizeof(buf), "%02d%02d.%02d%c", degrees, minInt, minFrac, ns);
        return String(buf);
    }

    // Helper to format longitude for APRS (DDDMM.MME/W)
    String formatLongitudeAPRS(double lon) {
        char ew = (lon >= 0) ? 'E' : 'W';
        lon = fabs(lon);
        int degrees = (int)lon;
        double minutes = (lon - degrees) * 60.0;
        int minInt = (int)minutes;
        int minFrac = (int)((minutes - minInt) * 100 + 0.5);
        // Handle rollover when rounding pushes fraction to 100
        if (minFrac >= 100) {
            minFrac -= 100;
            minInt++;
            if (minInt >= 60) {
                minInt -= 60;
                degrees++;
            }
        }
        char buf[48];
        snprintf(buf, sizeof(buf), "%03d%02d.%02d%c", degrees, minInt, minFrac, ew);
        return String(buf);
    }

    // Helper to get APRS timestamp (DDHHMMz in UTC)
    String getAPRSTimestamp() {
        time_t now;
        time(&now);
        struct tm* utc = gmtime(&now);
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d%02d%02dz", utc->tm_mday, utc->tm_hour, utc->tm_min);
        return String(buf);
    }

    void checkWxInterval() {
        // Only process if weather sensor is active
        if (!Config.wxsensor.active) {
            return;
        }

        uint32_t lastWx = millis() - lastWxTx;
        if (lastWxTx != 0 && lastWx < Config.wunderground.interval * 60 * 1000) {
            return;  // Interval not yet elapsed
        }

        // Get weather data from appropriate source
        WX_Data wxData;
        if (Config.wunderground.active) {
            Serial.println("-- Fetching Weather Underground data --");
            wxData = WX_Utils::getWeatherData();
        } else if (wxModuleType != 0) {
            Serial.println("-- Reading local weather sensor --");
            wxData = WX_Utils::getWeatherData();
        } else {
            Serial.println("No weather source available, skipping WX packet");
            return;
        }

        // Check if we got valid data
        if (!wxData.valid) {
            Serial.println("Weather data invalid, skipping WX packet");
            return;
        }

        // -----------------------------------------------------------------------
        // Build complete weather report with compressed position and timestamp
        // Compressed format: CALL>TOCALL,PATH,qAC:@DDHHMMz/YYYYXXXXscsGgNNNtNNN...
        // Where:
        //   @ = timestamp indicator
        //   DDHHMMz = day/hour/minute UTC
        //   / = overlay character (table select)
        //   YYYY = compressed latitude (4 base91 chars)
        //   XXXX = compressed longitude (4 base91 chars)
        //   s = weather symbol (e.g., _)
        //   c = wind direction encoded in course byte (1 base91 char)
        //   s = wind speed encoded in speed byte (1 base91 char)
        //   G = compression type byte (0x47)
        //   gNNN... = additional weather data (gust, temp, rain, humidity, pressure, etc.)
        // -----------------------------------------------------------------------

        // Use wxsensor config if set, otherwise fall back to beacon/main config
        String wxCallsign = Config.wxsensor.callsign.length() > 0 ? Config.wxsensor.callsign : Config.callsign;
        String wxOverlay = Config.wxsensor.overlay.length() > 0 ? Config.wxsensor.overlay : "/";
        String wxSymbol = Config.wxsensor.symbol.length() > 0 ? Config.wxsensor.symbol : "_";
        double wxLatitude = (Config.wxsensor.latitude != 0.0 || Config.wxsensor.longitude != 0.0) ? Config.wxsensor.latitude : Config.beacon.latitude;
        double wxLongitude = (Config.wxsensor.latitude != 0.0 || Config.wxsensor.longitude != 0.0) ? Config.wxsensor.longitude : Config.beacon.longitude;

        // Convert wind speed from m/s to knots for APRS encoding
        float windSpeedKnots = isnan(wxData.windspeed) ? 0 : wxData.windspeed * 1.94384;
        float windDir = isnan(wxData.winddir) ? 0 : wxData.winddir;

        // Generate compressed position with wind direction/speed encoded in course/speed bytes
        String compressedPos = APRSPacketLib::encodeGPSIntoBase91(
            wxLatitude,
            wxLongitude,
            windDir,           // Course byte = wind direction (0-360 degrees)
            windSpeedKnots,    // Speed byte = wind speed (knots)
            wxSymbol,          // Weather symbol (typically "_")
            false,             // Don't send altitude
            0,                 // altitude (unused)
            false,             // Not a standing update (we want course/speed encoded, not blank)
            0                  // No position ambiguity
        );

        // Generate additional weather data
        String additionalWxData = WX_Utils::generateCompressedWeatherData(wxData);

        // Use WU observation timestamp if available, otherwise use current time
        String timestamp;
        if (wxData.obsTimeUtc.length() > 0) {
            timestamp = WX_Utils::parseObsTimeToAPRS(wxData.obsTimeUtc);
            Serial.println("Using WU observation timestamp: " + timestamp);
        }
        if (timestamp.length() == 0) {
            timestamp = getAPRSTimestamp();
            Serial.println("Using current time timestamp: " + timestamp);
        }

        String wxPacket = APRSPacketLib::generateBasePacket(wxCallsign, "APLRG1", Config.beacon.path);
        wxPacket += ",qAC:@";
        wxPacket += timestamp;
        wxPacket += wxOverlay;
        wxPacket += compressedPos;
        wxPacket += additionalWxData;

        // Send via APRS-IS
        if (Config.wxsensor.sendViaAPRSIS && Config.aprs_is.active && passcodeValid && !backUpDigiMode) {
            Serial.println("-- Sending WX Packet to APRS-IS (compressed format) --");
            Serial.println(wxPacket);
            displayShow(firstLine, secondLine, thirdLine, fourthLine, fifthLine, sixthLine, "SENDING WX PACKET", 0);
            #ifdef HAS_A7670
                A7670_Utils::uploadToAPRSIS(wxPacket);
            #else
                APRS_IS_Utils::upload(wxPacket);
            #endif
        }

        // Send via RF if configured
        if (Config.wxsensor.sendViaRF || backUpDigiMode) {
            String wxPacketRF = APRSPacketLib::generateBasePacket(wxCallsign, "APLRG1", Config.beacon.path);
            wxPacketRF += ":@";
            wxPacketRF += timestamp;
            wxPacketRF += wxOverlay;
            wxPacketRF += compressedPos;
            wxPacketRF += additionalWxData;
            Serial.println("-- Sending WX Packet to RF (compressed format) --");
            // wxFreq: 0 = RX freq (pass true), 1 = TX freq (pass false)
            STATION_Utils::addToOutputPacketBuffer(wxPacketRF, Config.wxsensor.wxFreq == 0);
        }

        lastWxTx = millis();
        lastScreenOn = millis();
    }

    void checkDisplayInterval() {
        uint32_t lastDisplayTime = millis() - lastScreenOn;
        if (!Config.display.alwaysOn && lastDisplayTime >= Config.display.timeout * 1000) {
            displayToggle(false);
        }
    }

    void validateFreqs() {
        if (Config.loramodule.txFreq != Config.loramodule.rxFreq && abs(Config.loramodule.txFreq - Config.loramodule.rxFreq) < 125000) {
            Serial.println("Tx Freq less than 125kHz from Rx Freq ---> NOT VALID");
            displayShow("Tx Freq is less than ", "125kHz from Rx Freq", "device will autofix", "and then reboot", 1000);
            Config.loramodule.txFreq = Config.loramodule.rxFreq; // Inform about that but then change the TX QRG to RX QRG and reset the device
            Config.beacon.beaconFreq = 1;   // return to LoRa Tx Beacon Freq
            Config.writeFile();
            ESP.restart();
        }
    }

    void typeOfPacket(const String& packet, const uint8_t packetType) {
        String sender = packet.substring(0,packet.indexOf(">"));
        switch (packetType) {
            case 0: // LoRa-APRS
                fifthLine = "LoRa Rx ----> APRS-IS";
                break;
            case 1: // APRS-LoRa
                fifthLine = "APRS-IS ----> LoRa Tx";
                break;
            case 2: // Digipeater
                fifthLine = "LoRa Rx ----> LoRa Tx";
                break;
        }

        int firstColonIndex = packet.indexOf(":");
        char nextChar       = packet[firstColonIndex + 1];

        for (int i = sender.length(); i < 9; i++) {
            sender += " ";
        }
        sixthLine = sender;

        if (nextChar == ':') {
            sixthLine += "> MESSAGE";
        } else if (nextChar == '>') {
            sixthLine += "> NEW STATUS";
        } else if (nextChar == '!' || nextChar == '=' || nextChar == '@') {
            sixthLine += "> GPS BEACON";
            if (!Config.syslog.active) GPS_Utils::getDistanceAndComment(packet);       // to be checked!!!
            seventhLine = "RSSI:";
            seventhLine += String(rssi);
            seventhLine += "dBm";
            seventhLine += (rssi <= -100) ? " " : "  ";
            if (distance.indexOf(".") == 1) seventhLine += " ";
            seventhLine += "D:";
            seventhLine += distance;
            seventhLine += "km";
        } else if (nextChar == '`' || nextChar == '\'') {
            sixthLine += ">  MIC-E";
        } else if (nextChar == ';') {
            sixthLine += ">  OBJECT";
        } else if (packet.indexOf(":T#") >= 10 && packet.indexOf(":=/") == -1) {
            sixthLine += "> TELEMETRY";
        } else {
            sixthLine += "> ??????????";
        }
        if (nextChar != '!' && nextChar != '=' && nextChar != '@') {    // Common assignment for non-GPS cases
            seventhLine = "RSSI:";
            seventhLine += String(rssi);
            seventhLine += "dBm SNR: ";
            seventhLine += String(snr);
            seventhLine += "dBm";
        }
    }

    void print(const String& text) {
        if (!Config.tnc.enableSerial) {
            Serial.print(text);
        }
    }

    void println(const String& text) {
        if (!Config.tnc.enableSerial) {
            Serial.println(text);
        }
    }

    void checkRebootMode() {
        if (Config.rebootMode && Config.rebootModeTime > 0) {
            Serial.println("(Reboot Time Set to " + String(Config.rebootModeTime) + " hours)");
        }
    }

    void checkRebootTime() {
        if (Config.rebootMode && Config.rebootModeTime > 0) {
            if (millis() > Config.rebootModeTime * 60 * 60 * 1000) {
                Serial.println("\n*** Automatic Reboot Time Restart! ****\n");
                ESP.restart();
            }
        }
    }

    void checkSleepByLowBatteryVoltage(uint8_t mode) {
        if (shouldSleepLowVoltage) {
            if (mode == 0) {    // at startup
                delay(3000);
            }
            Serial.println("\n\n*** Sleeping Low Battey Voltage ***\n\n");
            esp_sleep_enable_timer_wakeup(30 * 60 * 1000000); // sleep 30 min
            if (mode == 1) {    // low voltage detected after a while
                displayToggle(false);
            }
            #ifdef VEXT_CTRL
                #ifndef HELTEC_WSL_V3
                    digitalWrite(VEXT_CTRL, LOW);
                #endif
            #endif
            LoRa_Utils::sleepRadio();
            transmitFlag = true;
            delay(100);
            esp_deep_sleep_start();
        }
    }

    bool checkValidCallsign(const String& callsign) {
        if (callsign == "WLNK-1") return true;
        
        String cleanCallsign;
        if (callsign.indexOf("-") > 0) {    // SSID Validation
            cleanCallsign = callsign.substring(0, callsign.indexOf("-"));
            String ssid = callsign.substring(callsign.indexOf("-") + 1);
            if (ssid.indexOf("-") != -1 || ssid.length() > 2) return false;
            if (ssid.length() == 2 && ssid[0] == '0') return false;
            for (int i = 0; i < ssid.length(); i++) {
                if (!isAlphaNumeric(ssid[i])) return false;
            }
        } else {
            cleanCallsign = callsign;
        }

        if (cleanCallsign.length() < 4 || cleanCallsign.length() > 6) return false;

        if (cleanCallsign.length() < 6 && isAlpha(cleanCallsign[0]) && isDigit(cleanCallsign[1]) && isAlpha(cleanCallsign[2]) && isAlpha(cleanCallsign[3]) ) {
            cleanCallsign = " " + cleanCallsign;    // A0AA --> _A0AA
        }

        if (!isDigit(cleanCallsign[2]) || !isAlpha(cleanCallsign[3])) {     // __0A__ must be validated
            if (cleanCallsign[0] != 'R' && !isDigit(cleanCallsign[1]) && !isAlpha(cleanCallsign[2])) return false;    // to accepto R0A___
        }

        bool isValid = false;
        if ((isAlphaNumeric(cleanCallsign[0]) || cleanCallsign[0] == ' ') && isAlpha(cleanCallsign[1])) {
            isValid = true;     //  AA0A (+A+A) + _A0AA (+A) + 0A0A (+A+A)
        } else if (isAlpha(cleanCallsign[0]) && isDigit(cleanCallsign[1])) {
            isValid = true;     //  A00A (+A+A)
        } else if (cleanCallsign[0] == 'R' && cleanCallsign.length() == 6 && isDigit(cleanCallsign[1]) && isAlpha(cleanCallsign[2]) && isAlpha(cleanCallsign[3]) && isAlpha(cleanCallsign[4])) {
            isValid = true;     //  R0AA (+A+A)
        }
        if (!isValid) return false;   // also 00__ avoided

        if (cleanCallsign.length() > 4) {   // to validate ____AA
            for (int i = 5; i <= cleanCallsign.length(); i++) {
                if (!isAlpha(cleanCallsign[i - 1])) return false;
            }
        }
        return true;
    }

    void startupDelay() {
        if (Config.startupDelay > 0) {
            displayShow("", "  STARTUP DELAY ...", "", "", 0);
            delay(Config.startupDelay * 60 * 1000);
        }
    }

}
