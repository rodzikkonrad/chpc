/**
 * @file temperature_sensors.cpp
 * @brief Implementation of TemperatureSensors class
 */

#include "temperature_sensors.h"
#include "settings.h"

TemperatureSensors::TemperatureSensors() 
    : _oneWire(Pins::ONEWIRE_BUS)
    , _dallas(&_oneWire) {
}

void TemperatureSensors::begin() {
    Serial.println(F("[Sensors] Initializing temperature sensors..."));
    
    _dallas.begin();
    _dallas.setWaitForConversion(false);  // Async mode
    _dallas.setResolution(12);            // 12-bit resolution (0.0625°C)
    
    _deviceCount = _dallas.getDeviceCount();
    Serial.printf("[Sensors] Found %d devices on OneWire bus\n", _deviceCount);
    
    // Load sensor configuration from settings
    loadSensorConfig();
    
    // Request initial temperature conversion
    requestTemperatures();
    delay(CONVERSION_DELAY_MS);
    readTemperatures();
    
    Serial.println(F("[Sensors] Initialization complete"));
}

void TemperatureSensors::loadSensorConfig() {
    for (uint8_t i = 0; i < SensorConfig::SENSOR_COUNT; i++) {
        settings.getSensorAddress(i, _sensors[i].address);
        _sensors[i].enabled = settings.isSensorEnabled(i);
        
        if (_sensors[i].enabled) {
            Serial.printf("[Sensors] %s configured: ", SensorConfig::SENSOR_NAMES[i]);
            for (uint8_t j = 0; j < 8; j++) {
                Serial.printf("%02X", _sensors[i].address[j]);
            }
            Serial.println();
        }
    }
}

void TemperatureSensors::requestTemperatures() {
    _dallas.requestTemperatures();
    _conversionRequested = true;
    _conversionRequestTime = millis();
}

void TemperatureSensors::readTemperatures() {
    if (!_conversionRequested) return;
    
    // Wait for conversion if needed
    uint32_t elapsed = millis() - _conversionRequestTime;
    if (elapsed < CONVERSION_DELAY_MS) {
        delay(CONVERSION_DELAY_MS - elapsed);
    }
    
    for (uint8_t i = 0; i < SensorConfig::SENSOR_COUNT; i++) {
        if (_sensors[i].enabled) {
            float temp = readSensor(i);
            
            if (validateReading(temp)) {
                _sensors[i].temperature = temp;
                _sensors[i].valid = true;
                _sensors[i].lastReadTime = millis();
                _sensors[i].errorCount = 0;
            } else {
                _sensors[i].errorCount++;
                if (_sensors[i].errorCount >= MAX_ERROR_COUNT) {
                    _sensors[i].valid = false;
                    _sensors[i].temperature = TempLimits::SENSOR_ERROR;
                }
                Serial.printf("[Sensors] %s read error (count: %d)\n", 
                             SensorConfig::SENSOR_NAMES[i], _sensors[i].errorCount);
            }
        }
    }
    
    _conversionRequested = false;
}

float TemperatureSensors::readSensor(uint8_t index) {
    if (index >= SensorConfig::SENSOR_COUNT) return TempLimits::SENSOR_ERROR;
    if (!_sensors[index].enabled) return TempLimits::SENSOR_ERROR;
    
    // Try multiple times for reliable reading
    for (uint8_t attempt = 0; attempt < 3; attempt++) {
        float temp = _dallas.getTempC(_sensors[index].address);
        
        // Check for valid reading (not 85.0 or -127.0)
        if (validateReading(temp)) {
            return temp;
        }
        
        // If 85.0, wait and retry (power-on reset value)
        if (temp == TempLimits::SENSOR_INITIAL) {
            delay(100);
        }
    }
    
    return TempLimits::SENSOR_ERROR;
}

bool TemperatureSensors::validateReading(float temp) const {
    // Check for error values
    if (temp == TempLimits::SENSOR_ERROR) return false;
    if (temp == TempLimits::SENSOR_INITIAL) return false;
    
    // Check reasonable temperature range (-50 to +125°C for DS18B20)
    if (temp < -50.0f || temp > 125.0f) return false;
    
    return true;
}

void TemperatureSensors::update() {
    static uint32_t lastUpdate = 0;
    uint32_t now = millis();
    
    if (_conversionRequested) {
        // Check if conversion is complete
        if (now - _conversionRequestTime >= CONVERSION_DELAY_MS) {
            readTemperatures();
            requestTemperatures();  // Start next conversion
        }
    } else {
        // Start new conversion cycle
        requestTemperatures();
    }
}

float TemperatureSensors::getTemperature(uint8_t index) const {
    if (index >= SensorConfig::SENSOR_COUNT) return TempLimits::SENSOR_ERROR;
    return _sensors[index].temperature;
}

bool TemperatureSensors::isSensorValid(uint8_t index) const {
    if (index >= SensorConfig::SENSOR_COUNT) return false;
    return _sensors[index].enabled && _sensors[index].valid;
}

bool TemperatureSensors::isSensorEnabled(uint8_t index) const {
    if (index >= SensorConfig::SENSOR_COUNT) return false;
    return _sensors[index].enabled;
}

const TemperatureSensor& TemperatureSensors::getSensor(uint8_t index) const {
    static TemperatureSensor dummy;
    if (index >= SensorConfig::SENSOR_COUNT) return dummy;
    return _sensors[index];
}

uint8_t TemperatureSensors::scanBus() {
    Serial.println(F("[Sensors] Scanning OneWire bus..."));
    
    DeviceAddress addr;
    uint8_t count = 0;
    
    _oneWire.reset_search();
    while (_oneWire.search(addr)) {
        if (OneWire::crc8(addr, 7) == addr[7]) {
            Serial.printf("[Sensors] Device %d: ", count);
            for (uint8_t i = 0; i < 8; i++) {
                Serial.printf("%02X", addr[i]);
            }
            Serial.println();
            count++;
        }
    }
    
    _deviceCount = count;
    Serial.printf("[Sensors] Found %d devices\n", count);
    return count;
}

void TemperatureSensors::configureSensor(uint8_t index, const uint8_t* address) {
    if (index >= SensorConfig::SENSOR_COUNT) return;
    
    memcpy(_sensors[index].address, address, 8);
    _sensors[index].enabled = true;
    _sensors[index].valid = false;
    _sensors[index].errorCount = 0;
    
    // Save to settings
    settings.setSensorAddress(index, address);
    settings.setSensorEnabled(index, true);
    
    Serial.printf("[Sensors] Configured %s\n", SensorConfig::SENSOR_NAMES[index]);
}

void TemperatureSensors::printAddresses() {
    Serial.println(F("\n=== Temperature Sensor Addresses ==="));
    for (uint8_t i = 0; i < SensorConfig::SENSOR_COUNT; i++) {
        Serial.printf("%-10s: ", SensorConfig::SENSOR_NAMES[i]);
        if (_sensors[i].enabled) {
            for (uint8_t j = 0; j < 8; j++) {
                Serial.printf("%02X", _sensors[i].address[j]);
            }
            Serial.printf(" = %.2f°C %s\n", 
                         _sensors[i].temperature,
                         _sensors[i].valid ? "(OK)" : "(ERROR)");
        } else {
            Serial.println("Not configured");
        }
    }
    Serial.println(F("====================================\n"));
}

bool TemperatureSensors::isSystemHealthy() const {
    // Check critical sensors: Tae, Tbe for EEV, Ttarget for control
    bool critical = true;
    
#if ENABLE_EEV
    if (_sensors[SensorConfig::SENSOR_TAE].enabled && !_sensors[SensorConfig::SENSOR_TAE].valid) {
        critical = false;
    }
    if (_sensors[SensorConfig::SENSOR_TBE].enabled && !_sensors[SensorConfig::SENSOR_TBE].valid) {
        critical = false;
    }
#endif
    
    if (_sensors[SensorConfig::SENSOR_TTARGET].enabled && !_sensors[SensorConfig::SENSOR_TTARGET].valid) {
        critical = false;
    }
    
    return critical;
}

bool TemperatureSensors::hasErrors() const {
    for (uint8_t i = 0; i < SensorConfig::SENSOR_COUNT; i++) {
        if (_sensors[i].enabled && !_sensors[i].valid) {
            return true;
        }
    }
    return false;
}
