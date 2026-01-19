/**
 * @file settings.cpp
 * @brief Implementation of Settings class
 */

#include "settings.h"

// NVS namespace and keys
static const char* NVS_NAMESPACE = "chpc";
static const char* KEY_MAGIC = "magic";
static const uint32_t SETTINGS_MAGIC = 0x43485043;  // "CHPC"

void Settings::begin() {
    if (_initialized) return;
    
    _prefs.begin(NVS_NAMESPACE, false);
    
    // Check if settings exist
    uint32_t magic = _prefs.getUInt(KEY_MAGIC, 0);
    if (magic == SETTINGS_MAGIC) {
        loadFromNVS();
    } else {
        loadDefaults();
        saveToNVS();
    }
    
    _initialized = true;
    _dirty = false;
}

void Settings::loadDefaults() {
    _heatingSetpoint = TempLimits::SETPOINT_DEFAULT;
    _coolingSetpoint = TempLimits::COOLING_SETPOINT_DEFAULT;
    _dhwSetpoint = TempLimits::DHW_SETPOINT_DEFAULT;
    _dhwHysteresis = TempLimits::DHW_HYSTERESIS_DEFAULT;
    _chHysteresis = TempLimits::CH_HYSTERESIS_DEFAULT;
    _eevSetpoint = EEVConfig::TARGET_TEMP_DIFF;
    _maxWatts = PowerConfig::MAX_WATTS;
    
    _dhwEnabled = false;
    _bufferEnabled = false;
    _valve4wayInstalled = false;
    _coolingModeEnabled = false;
    _season = Season::WINTER;
    
    _wifiSSID = NetworkConfig::DEFAULT_SSID;
    _wifiPassword = NetworkConfig::DEFAULT_PASSWORD;
    _mqttServer = NetworkConfig::MQTT_SERVER;
    _mqttPort = NetworkConfig::MQTT_PORT;
    _mqttUser = NetworkConfig::MQTT_USER;
    _mqttPassword = NetworkConfig::MQTT_PASSWORD;
    
    memset(_sensorAddresses, 0, sizeof(_sensorAddresses));
    _sensorsEnabled = 0;
}

void Settings::loadFromNVS() {
    _heatingSetpoint = _prefs.getFloat("heat_sp", TempLimits::SETPOINT_DEFAULT);
    _coolingSetpoint = _prefs.getFloat("cool_sp", TempLimits::COOLING_SETPOINT_DEFAULT);
    _dhwSetpoint = _prefs.getFloat("dhw_sp", TempLimits::DHW_SETPOINT_DEFAULT);
    _dhwHysteresis = _prefs.getFloat("dhw_hyst", TempLimits::DHW_HYSTERESIS_DEFAULT);
    _chHysteresis = _prefs.getFloat("ch_hyst", TempLimits::CH_HYSTERESIS_DEFAULT);
    _eevSetpoint = _prefs.getFloat("eev_sp", EEVConfig::TARGET_TEMP_DIFF);
    _maxWatts = _prefs.getFloat("max_watts", PowerConfig::MAX_WATTS);
    
    _dhwEnabled = _prefs.getBool("dhw_en", false);
    _bufferEnabled = _prefs.getBool("buffer_en", false);
    _valve4wayInstalled = _prefs.getBool("valve4w", false);
    _coolingModeEnabled = _prefs.getBool("cooling", false);
    _season = static_cast<Season>(_prefs.getUChar("season", 0));
    
    _wifiSSID = _prefs.getString("wifi_ssid", NetworkConfig::DEFAULT_SSID);
    _wifiPassword = _prefs.getString("wifi_pass", NetworkConfig::DEFAULT_PASSWORD);
    _mqttServer = _prefs.getString("mqtt_srv", NetworkConfig::MQTT_SERVER);
    _mqttPort = _prefs.getUShort("mqtt_port", NetworkConfig::MQTT_PORT);
    _mqttUser = _prefs.getString("mqtt_user", NetworkConfig::MQTT_USER);
    _mqttPassword = _prefs.getString("mqtt_pass", NetworkConfig::MQTT_PASSWORD);
    
    _sensorsEnabled = _prefs.getUShort("sens_en", 0);
    
    // Load sensor addresses
    for (uint8_t i = 0; i < SensorConfig::SENSOR_COUNT; i++) {
        String key = "sens_" + String(i);
        size_t len = _prefs.getBytes(key.c_str(), _sensorAddresses[i], 8);
        if (len != 8) {
            memset(_sensorAddresses[i], 0, 8);
        }
    }
}

void Settings::saveToNVS() {
    _prefs.putUInt(KEY_MAGIC, SETTINGS_MAGIC);
    
    _prefs.putFloat("heat_sp", _heatingSetpoint);
    _prefs.putFloat("cool_sp", _coolingSetpoint);
    _prefs.putFloat("dhw_sp", _dhwSetpoint);
    _prefs.putFloat("dhw_hyst", _dhwHysteresis);
    _prefs.putFloat("ch_hyst", _chHysteresis);
    _prefs.putFloat("eev_sp", _eevSetpoint);
    _prefs.putFloat("max_watts", _maxWatts);
    
    _prefs.putBool("dhw_en", _dhwEnabled);
    _prefs.putBool("buffer_en", _bufferEnabled);
    _prefs.putBool("valve4w", _valve4wayInstalled);
    _prefs.putBool("cooling", _coolingModeEnabled);
    _prefs.putUChar("season", static_cast<uint8_t>(_season));
    
    _prefs.putString("wifi_ssid", _wifiSSID);
    _prefs.putString("wifi_pass", _wifiPassword);
    _prefs.putString("mqtt_srv", _mqttServer);
    _prefs.putUShort("mqtt_port", _mqttPort);
    _prefs.putString("mqtt_user", _mqttUser);
    _prefs.putString("mqtt_pass", _mqttPassword);
    
    _prefs.putUShort("sens_en", _sensorsEnabled);
    
    for (uint8_t i = 0; i < SensorConfig::SENSOR_COUNT; i++) {
        String key = "sens_" + String(i);
        _prefs.putBytes(key.c_str(), _sensorAddresses[i], 8);
    }
    
    _dirty = false;
}

void Settings::save() {
    if (_dirty) {
        saveToNVS();
    }
}

void Settings::factoryReset() {
    _prefs.clear();
    loadDefaults();
    saveToNVS();
}

// ========== Setters with validation ==========

void Settings::setHeatingSetpoint(float value) {
    value = constrain(value, TempLimits::SETPOINT_MIN, TempLimits::SETPOINT_MAX);
    if (_heatingSetpoint != value) {
        _heatingSetpoint = value;
        _dirty = true;
    }
}

void Settings::setCoolingSetpoint(float value) {
    value = constrain(value, TempLimits::COOLING_SETPOINT_MIN, TempLimits::COOLING_SETPOINT_MAX);
    if (_coolingSetpoint != value) {
        _coolingSetpoint = value;
        _dirty = true;
    }
}

void Settings::setDHWSetpoint(float value) {
    value = constrain(value, TempLimits::DHW_SETPOINT_MIN, TempLimits::DHW_SETPOINT_MAX);
    if (_dhwSetpoint != value) {
        _dhwSetpoint = value;
        _dirty = true;
    }
}

void Settings::setDHWHysteresis(float value) {
    value = constrain(value, 0.0f, 10.0f);
    if (_dhwHysteresis != value) {
        _dhwHysteresis = value;
        _dirty = true;
    }
}

void Settings::setCHHysteresis(float value) {
    value = constrain(value, 0.0f, 10.0f);
    if (_chHysteresis != value) {
        _chHysteresis = value;
        _dirty = true;
    }
}

void Settings::setEEVSetpoint(float value) {
    value = constrain(value, 0.5f, 15.0f);
    if (_eevSetpoint != value) {
        _eevSetpoint = value;
        _dirty = true;
    }
}

void Settings::setMaxWatts(float value) {
    value = constrain(value, 500.0f, 5000.0f);
    if (_maxWatts != value) {
        _maxWatts = value;
        _dirty = true;
    }
}

void Settings::setDHWEnabled(bool value) {
    if (_dhwEnabled != value) {
        _dhwEnabled = value;
        _dirty = true;
    }
}

void Settings::setBufferEnabled(bool value) {
    if (_bufferEnabled != value) {
        _bufferEnabled = value;
        _dirty = true;
    }
}

void Settings::set4WayValveInstalled(bool value) {
    if (_valve4wayInstalled != value) {
        _valve4wayInstalled = value;
        _dirty = true;
    }
}

void Settings::setCoolingModeEnabled(bool value) {
    if (_coolingModeEnabled != value) {
        _coolingModeEnabled = value;
        _dirty = true;
    }
}

void Settings::setSeason(Season value) {
    if (_season != value) {
        _season = value;
        _dirty = true;
    }
}

void Settings::setWiFiSSID(const String& value) {
    if (_wifiSSID != value) {
        _wifiSSID = value;
        _dirty = true;
    }
}

void Settings::setWiFiPassword(const String& value) {
    if (_wifiPassword != value) {
        _wifiPassword = value;
        _dirty = true;
    }
}

void Settings::setMQTTServer(const String& value) {
    if (_mqttServer != value) {
        _mqttServer = value;
        _dirty = true;
    }
}

void Settings::setMQTTPort(uint16_t value) {
    if (_mqttPort != value) {
        _mqttPort = value;
        _dirty = true;
    }
}

void Settings::setMQTTUser(const String& value) {
    if (_mqttUser != value) {
        _mqttUser = value;
        _dirty = true;
    }
}

void Settings::setMQTTPassword(const String& value) {
    if (_mqttPassword != value) {
        _mqttPassword = value;
        _dirty = true;
    }
}

void Settings::setSensorAddress(uint8_t index, const uint8_t* address) {
    if (index < SensorConfig::SENSOR_COUNT) {
        memcpy(_sensorAddresses[index], address, 8);
        _dirty = true;
    }
}

void Settings::getSensorAddress(uint8_t index, uint8_t* address) const {
    if (index < SensorConfig::SENSOR_COUNT) {
        memcpy(address, _sensorAddresses[index], 8);
    }
}

bool Settings::isSensorEnabled(uint8_t index) const {
    if (index < SensorConfig::SENSOR_COUNT) {
        return (_sensorsEnabled & (1 << index)) != 0;
    }
    return false;
}

void Settings::setSensorEnabled(uint8_t index, bool enabled) {
    if (index < SensorConfig::SENSOR_COUNT) {
        if (enabled) {
            _sensorsEnabled |= (1 << index);
        } else {
            _sensorsEnabled &= ~(1 << index);
        }
        _dirty = true;
    }
}
