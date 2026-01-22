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

#include <TinyGPS++.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#ifdef LIGHTGATEWAY_PLUS_1_0
#include "Adafruit_SHTC3.h"
#endif
#include "configuration.h"
#include "board_pinout.h"
#include "wx_utils.h"
#include "display.h"


#define SEALEVELPRESSURE_HPA    (1013.25)
#define CORRECTION_FACTOR       (8.2296)      // for meters

extern Configuration            Config;
extern String                   fifthLine;
#ifdef HAS_GPS
extern TinyGPSPlus              gps;
#endif

int         wxModuleType        = 0;
uint8_t     wxModuleAddress     = 0x00;

float newHum, newTemp, newPress, newGas;


Adafruit_BME280     bme280;
Adafruit_AHTX0      aht20; 
#if defined(HELTEC_V3) || defined(HELTEC_V3_2)
Adafruit_BMP280     bmp280(&Wire1);
Adafruit_Si7021     si7021  = Adafruit_Si7021();
#else
Adafruit_BMP280     bmp280;
Adafruit_BME680     bme680;
Adafruit_Si7021     si7021  = Adafruit_Si7021();
#endif
#ifdef LIGHTGATEWAY_PLUS_1_0
Adafruit_SHTC3      shtc3   = Adafruit_SHTC3();
#endif


WX_Data initWxData() {
    WX_Data data;
    data.temperature       = NAN;
    data.humidity          = NAN;
    data.pressure          = NAN;
    data.winddir           = NAN;
    data.windspeed         = NAN;
    data.windgust          = NAN;
    data.rainLastHour      = NAN;
    data.rainLast24Hours   = NAN;
    data.rainSinceMidnight = NAN;
    data.luminosity        = NAN;
    data.gasResistance     = NAN;
    data.valid             = false;
    data.hasHumidity       = true;
    data.hasPressure       = true;
    return data;
}

namespace WX_Utils {

    void getWxModuleAddres() {
        uint8_t err, addr;
        for(addr = 1; addr < 0x7F; addr++) {
            #if defined(HELTEC_V3) || defined(HELTEC_V3_2) || defined(HELTEC_WSL_V3) || defined(HELTEC_WSL_V3_DISPLAY)
                Wire1.beginTransmission(addr);
                err = Wire1.endTransmission();
            #else
                Wire.beginTransmission(addr);
                #ifdef LIGHTGATEWAY_PLUS_1_0
                    Wire.write(0x35);
                    Wire.write(0x17);
                #endif
                err = Wire.endTransmission();
            #endif
            delay(5);
            if (err == 0) {
                //Serial.println(addr); //this shows any connected board to I2C
                if (addr == 0x76 || addr == 0x77) { // BME or BMP
                    wxModuleAddress = addr;
                    return;
                } else if (addr == 0x40) {          // Si7011
                    wxModuleAddress = addr;
                    return;
                } else if (addr == 0x70) {          // SHTC3
                    wxModuleAddress = addr;
                    return;
                }
            }
        }
    }

    void setup() {
    	Serial.println("Initializing Weather Sensor Module...");
        if (Config.wxsensor.active) {
        	Serial.println("wxsensor is active, searching for sensor...");
            getWxModuleAddres();
            if (wxModuleAddress != 0x00) {
            	Serial.print("I2C address found");
                bool wxModuleFound = false;
                if (wxModuleAddress == 0x76 || wxModuleAddress == 0x77) {
                    #if defined(HELTEC_V3) || defined(HELTEC_V3_2) || defined(HELTEC_WSL_V3) || defined(HELTEC_WSL_V3_DISPLAY)
                        if (bme280.begin(wxModuleAddress, &Wire1)) {
                            Serial.println("BME280 sensor found");
                            wxModuleType    = 1;
                            wxModuleFound   = true;
                        }
                    #else
                        if (bme280.begin(wxModuleAddress)) {
                            Serial.println("BME280 sensor found");
                            wxModuleType    = 1;
                            wxModuleFound   = true;
                        }
                        if (!wxModuleFound) {
                            if (bme680.begin(wxModuleAddress)) {
                                Serial.println("BME680 sensor found");
                                wxModuleType    = 3;
                                wxModuleFound   = true;
                            }
                        }
                    #endif
                    if (!wxModuleFound) {
                        if (bmp280.begin(wxModuleAddress)) {
                            Serial.println("BMP280 sensor found");
                            wxModuleType    = 2;
                            wxModuleFound   = true;
                            if (aht20.begin()) {
                                Serial.println("AHT20 sensor found");
                                if (wxModuleType == 2) wxModuleType = 6;
                            }
                        }
                    }
                } else if (wxModuleAddress == 0x40 && Config.battery.useExternalI2CSensor == false) {
                    if(si7021.begin()) {
                        Serial.println("Si7021 sensor found");
                        wxModuleType    = 4;
                        wxModuleFound   = true;
                    }
                }
                #ifdef LIGHTGATEWAY_PLUS_1_0
                else if (wxModuleAddress == 0x70) {
                    if (shtc3.begin()) {
                        Serial.println("SHTC3 sensor found");
                        wxModuleType    = 5;
                        wxModuleFound   = true;
                    }
                }
                #endif
                if (!wxModuleFound) {
                    displayShow("ERROR", "", "BME/BMP/Si7021/SHTC3 sensor active", "but no sensor found...", 2000);
                    Serial.println("BME/BMP/Si7021/SHTC3 sensor Active in config but not found! Check Wiring");
                } else {
                    switch (wxModuleType) {
                        case 1:
                            bme280.setSampling(Adafruit_BME280::MODE_FORCED,
                                        Adafruit_BME280::SAMPLING_X1,
                                        Adafruit_BME280::SAMPLING_X1,
                                        Adafruit_BME280::SAMPLING_X1,
                                        Adafruit_BME280::FILTER_OFF
                                        );
                            Serial.println("BME280 Module init done!");
                            break;
                        case 2:
                            bmp280.setSampling(Adafruit_BMP280::MODE_FORCED,
                                        Adafruit_BMP280::SAMPLING_X1,
                                        Adafruit_BMP280::SAMPLING_X1,
                                        Adafruit_BMP280::FILTER_OFF
                                        ); 
                            Serial.println("BMP280 Module init done!");
                            break;
                        case 3:
                            #if !defined(HELTEC_V3) && !defined(HELTEC_V3_2)
                                bme680.setTemperatureOversampling(BME680_OS_1X);
                                bme680.setHumidityOversampling(BME680_OS_1X);
                                bme680.setPressureOversampling(BME680_OS_1X);
                                bme680.setIIRFilterSize(BME680_FILTER_SIZE_0);
                                Serial.println("BME680 Module init done!");
                            #endif
                            break;
                    }
                }
            }
        } else {
			Serial.println("wxsensor is not active, skipping sensor init");
		}

    }

    // -----------------------------------------------------------------------
    // Fetch weather data from Weather Underground API
    // -----------------------------------------------------------------------
    WX_Data fetchWundergroundData() {
        WX_Data data = initWxData();

        if (!Config.wunderground.active) {
            Serial.println("Wunderground: not active in config");
            return data;
        }

        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("Wunderground: WiFi not connected");
            return data;
        }

        // Build the request URL
        String url = "https://api.weather.com/v2/pws/observations/current?format=json&units=m";
        url += "&stationId=" + Config.wunderground.stationId;
        url += "&apiKey=" + Config.wunderground.apiKey;

        HTTPClient http;
        http.setTimeout(5000);
        http.begin(url);
        Serial.printf("Wunderground HTTP GET: %s\n", url.c_str());
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            DynamicJsonDocument doc(2048);
            DeserializationError err = deserializeJson(doc, payload);
            Serial.printf("Wunderground JSON parse result: %s\n", err.c_str());
            if (!err) {
                JsonObject obs = doc["observations"][0];
                if (!obs.isNull()) {
                    // Top-level fields
                    if (obs.containsKey("humidity")) {
                        data.humidity = obs["humidity"];
                    }
                    if (obs.containsKey("winddir")) {
                        data.winddir = obs["winddir"];
                    }
                    if (obs.containsKey("solarRadiation")) {
                        data.luminosity = obs["solarRadiation"];
                    }

                    // Metric-nested fields (when using units=m)
                    JsonObject metric = obs["metric"];
                    if (!metric.isNull()) {
                        if (metric.containsKey("temp")) {
                            data.temperature = metric["temp"];
                        }
                        if (metric.containsKey("pressure")) {
                            data.pressure = metric["pressure"];
                        }
                        if (metric.containsKey("windSpeed")) {
                            data.windspeed = metric["windSpeed"].as<float>() / 3.6;  // km/h to m/s
                        }
                        if (metric.containsKey("windGust")) {
                            data.windgust = metric["windGust"].as<float>() / 3.6;    // km/h to m/s
                        }
                        if (metric.containsKey("precipRate")) {
                            data.rainLastHour = metric["precipRate"];  // mm/hr
                        }
                        if (metric.containsKey("precipTotal")) {
                            data.rainSinceMidnight = metric["precipTotal"];  // mm since midnight
                        }
                    }

                    // Mark valid if we got the core readings
                    if (!isnan(data.temperature) && !isnan(data.humidity) && !isnan(data.pressure)) {
                        data.valid = true;
                        Serial.println("Wunderground: data fetch successful");
                    }
                }
            }
        } else {
            Serial.printf("Wunderground HTTP error: %d\n", httpCode);
        }
        http.end();
        return data;
    }

    // -----------------------------------------------------------------------
    // Read weather data from local I2C sensor
    // -----------------------------------------------------------------------
    WX_Data readLocalSensor() {
        WX_Data data = initWxData();

        switch (wxModuleType) {
            case 1: // BME280
                bme280.takeForcedMeasurement();
                data.temperature = bme280.readTemperature();
                data.pressure    = bme280.readPressure() / 100.0F;
                data.humidity    = bme280.readHumidity();
                break;
            case 2: // BMP280 (no humidity)
                bmp280.takeForcedMeasurement();
                data.temperature = bmp280.readTemperature();
                data.pressure    = bmp280.readPressure() / 100.0F;
                data.humidity    = 0;
                data.hasHumidity = false;
                break;
            case 3: // BME680
                #if !defined(HELTEC_V3) && !defined(HELTEC_V3_2)
                    bme680.performReading();
                    delay(50);
                    if (bme680.endReading()) {
                        data.temperature   = bme680.temperature;
                        data.pressure      = bme680.pressure / 100.0F;
                        data.humidity      = bme680.humidity;
                        data.gasResistance = bme680.gas_resistance / 1000.0;  // kOhms
                    }
                #endif
                break;
            case 4: // Si7021 (no pressure)
                data.temperature = si7021.readTemperature();
                data.humidity    = si7021.readHumidity();
                data.pressure    = 0;
                data.hasPressure = false;
                break;
            case 5: // SHTC3 (no pressure)
                #ifdef LIGHTGATEWAY_PLUS_1_0
                {
                    sensors_event_t humidity, temp;
                    shtc3.getEvent(&humidity, &temp);
                    data.temperature = temp.temperature;
                    data.humidity    = humidity.relative_humidity;
                    data.pressure    = 0;
                    data.hasPressure = false;
                }
                #endif
                break;
            case 6: // BMP280 + AHT20
                {
                    bmp280.takeForcedMeasurement();
                    data.temperature = bmp280.readTemperature();
                    data.pressure    = bmp280.readPressure() / 100.0F;
                    sensors_event_t humidity, temp;
                    aht20.getEvent(&humidity, &temp);
                    data.humidity    = humidity.relative_humidity;
                }
                break;
            default:
                Serial.println("Local sensor: no sensor configured (wxModuleType=0)");
                return data;
        }

        // Validate readings
        if (!isnan(data.temperature)) {
            data.valid = true;
            Serial.printf("Local sensor: T=%.1fC H=%.1f%% P=%.1fhPa\n",
                          data.temperature, data.humidity, data.pressure);
        } else {
            Serial.println("Local sensor: read failed");
        }

        return data;
    }

    String generateTempString(const float sensorTemp) {
        String strTemp = String((int)sensorTemp);
        switch (strTemp.length()) {
            case 1:
                return "00" + strTemp;
            case 2:
                return "0" + strTemp;
            case 3:
                return strTemp;
            default:
                return "-999";
        }
    }

    String generateHumString(const float sensorHum) {
        String strHum = String((int)sensorHum);
        switch (strHum.length()) {
            case 1:
                return "0" + strHum;
            case 2:
                return strHum;
            case 3:
                if ((int)sensorHum == 100) {
                    return "00";
                } else {
                    return "-99";
                }
            default:
                return "-99";
        }
    }

    String generatePresString(const float sensorPres) {
        String strPress = String((int)sensorPres);
        String decPress = String(int((sensorPres - int(sensorPres)) * 10));
        switch (strPress.length()) {
            case 1:
                return "000" + strPress + decPress;
            case 2:
                return "00" + strPress + decPress;
            case 3:
                return "0" + strPress + decPress;
            case 4:
                return strPress + decPress;
            case 5:
                return strPress;
            default:
                return "-99999";
        }
    }

    // Wind direction: 3 digits (000-360 degrees)
    String generateWindDirString(const float windDir) {
        if (isnan(windDir)) return "...";
        int dir = (int)windDir % 360;
        if (dir < 10) return "00" + String(dir);
        if (dir < 100) return "0" + String(dir);
        return String(dir);
    }

    // Wind speed/gust: 3 digits in mph (input is m/s)
    String generateWindSpeedString(const float speedMs) {
        if (isnan(speedMs)) return "...";
        int mph = (int)(speedMs * 2.23694);  // m/s to mph
        if (mph < 0) return "...";
        if (mph < 10) return "00" + String(mph);
        if (mph < 100) return "0" + String(mph);
        if (mph < 1000) return String(mph);
        return "...";
    }

    // Rain: 3 digits in hundredths of inch (input is mm)
    String generateRainString(const float rainMm) {
        if (isnan(rainMm)) return "...";
        int hundredthsInch = (int)(rainMm * 3.937);  // mm to hundredths of inch
        if (hundredthsInch < 0) return "...";
        if (hundredthsInch < 10) return "00" + String(hundredthsInch);
        if (hundredthsInch < 100) return "0" + String(hundredthsInch);
        if (hundredthsInch < 1000) return String(hundredthsInch);
        return "999";  // cap at 999
    }

    // Luminosity: L for <=999, l for >=1000 W/m²
    String generateLuminosityString(const float lux) {
        if (isnan(lux)) return "";
        int val = (int)lux;
        if (val < 0) return "";
        if (val < 1000) {
            // Use 'L' prefix for values 0-999
            if (val < 10) return "L00" + String(val);
            if (val < 100) return "L0" + String(val);
            return "L" + String(val);
        } else {
            // Use 'l' prefix for values >= 1000, report value - 1000
            val = val - 1000;
            if (val > 999) val = 999;  // cap at 1999 W/m²
            if (val < 10) return "l00" + String(val);
            if (val < 100) return "l0" + String(val);
            return "l" + String(val);
        }
    }

    float getAltitudeCorrection() {
        #ifdef HAS_GPS
            return Config.beacon.gpsActive ? gps.altitude.meters() : Config.wxsensor.heightCorrection;
        #else
            return Config.wxsensor.heightCorrection;
        #endif
    }

    String readDataSensor() {
        // ---------------------------------------------------------------
        // 1) Try Weather Underground first, then fall back to local sensor
        // ---------------------------------------------------------------
        WX_Data wxData = fetchWundergroundData();
        bool usingWunderground = wxData.valid;

        if (!usingWunderground) {
            wxData = readLocalSensor();
            if (!wxData.valid) {
                Serial.println("No weather data available from any source");
                fifthLine = "";
                return ".../...g...t...";
            }
        }

        // Update global variables for compatibility with other modules
        newTemp  = wxData.temperature;
        newHum   = wxData.humidity;
        newPress = wxData.pressure;
        newGas   = wxData.gasResistance;

        // ---------------------------------------------------------------
        // 2) Build the APRS weather payload string
        // ---------------------------------------------------------------
        String tempStr = generateTempString(((wxData.temperature + Config.wxsensor.temperatureCorrection) * 1.8) + 32);
        String humStr  = wxData.hasHumidity ? generateHumString(wxData.humidity) : "..";
        String presStr = wxData.hasPressure
            ? generatePresString(wxData.pressure + getAltitudeCorrection() / CORRECTION_FACTOR)
            : ".....";

        // Wind and rain (only available from Wunderground)
        String windDirStr  = generateWindDirString(wxData.winddir);
        String windSpdStr  = generateWindSpeedString(wxData.windspeed);
        String windGustStr = generateWindSpeedString(wxData.windgust);
        String rainHrStr   = generateRainString(wxData.rainLastHour);
        String rain24Str   = generateRainString(wxData.rainLast24Hours);
        String rainMidStr  = generateRainString(wxData.rainSinceMidnight);
        String luxStr      = generateLuminosityString(wxData.luminosity);

        // Build display line
        fifthLine = usingWunderground ? "WU-> " : "BME-> ";
        fifthLine += String(int(wxData.temperature + Config.wxsensor.temperatureCorrection));
        fifthLine += "C ";
        fifthLine += humStr;
        fifthLine += "% ";
        fifthLine += presStr.substring(0, 4);
        fifthLine += "hPa";

        // Assemble APRS weather payload: ccc/sss g ggg t ttt r rrr p ppp P PPP h hh b bbbbb L lll
        String wxPayload = windDirStr + "/" + windSpdStr;
        wxPayload += "g" + windGustStr;
        wxPayload += "t" + tempStr;
        wxPayload += "r" + rainHrStr;
        wxPayload += "p" + rain24Str;
        wxPayload += "P" + rainMidStr;
        wxPayload += "h" + humStr;
        wxPayload += "b" + presStr;
        wxPayload += luxStr;

        // BME680 gas resistance (if available)
        if (!isnan(wxData.gasResistance)) {
            wxPayload += "Gas: " + String(wxData.gasResistance) + "Kohms";
        }

        return wxPayload;
    }

} // namespace WX_Utils
