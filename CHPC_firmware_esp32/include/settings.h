/**
 * @file settings.h
 * @brief Runtime settings management with NVS persistence
 * 
 * Manages all runtime-configurable settings with automatic persistence
 * to ESP32's Non-Volatile Storage (NVS).
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

/**
 * @class Settings
 * @brief Manages persistent settings stored in NVS
 */
class Settings {
public:
    // Singleton access
    static Settings& getInstance() {
        static Settings instance;
        return instance;
    }
    
    // Initialize settings from NVS
    void begin();
    
    // Save all settings to NVS
    void save();
    
    // Reset to factory defaults
    void factoryReset();
    
    // ========== Temperature Setpoints ==========
    float getHeatingSetpoint() const { return _heatingSetpoint; }
    void setHeatingSetpoint(float value);
    
    float getCoolingSetpoint() const { return _coolingSetpoint; }
    void setCoolingSetpoint(float value);
    
    float getDHWSetpoint() const { return _dhwSetpoint; }
    void setDHWSetpoint(float value);
    
    float getDHWHysteresis() const { return _dhwHysteresis; }
    void setDHWHysteresis(float value);
    
    float getCHHysteresis() const { return _chHysteresis; }
    void setCHHysteresis(float value);
    
    float getEEVSetpoint() const { return _eevSetpoint; }
    void setEEVSetpoint(float value);
    
    float getMaxWatts() const { return _maxWatts; }
    void setMaxWatts(float value);
    
    // ========== Feature Flags ==========
    bool isDHWEnabled() const { return _dhwEnabled; }
    void setDHWEnabled(bool value);
    
    bool isBufferEnabled() const { return _bufferEnabled; }
    void setBufferEnabled(bool value);
    
    bool is4WayValveInstalled() const { return _valve4wayInstalled; }
    void set4WayValveInstalled(bool value);
    
    bool isCoolingModeEnabled() const { return _coolingModeEnabled; }
    void setCoolingModeEnabled(bool value);
    
    Season getSeason() const { return _season; }
    void setSeason(Season value);
    
    // ========== Network Settings ==========
    const String& getWiFiSSID() const { return _wifiSSID; }
    void setWiFiSSID(const String& value);
    
    const String& getWiFiPassword() const { return _wifiPassword; }
    void setWiFiPassword(const String& value);
    
    const String& getMQTTServer() const { return _mqttServer; }
    void setMQTTServer(const String& value);
    
    uint16_t getMQTTPort() const { return _mqttPort; }
    void setMQTTPort(uint16_t value);
    
    const String& getMQTTUser() const { return _mqttUser; }
    void setMQTTUser(const String& value);
    
    const String& getMQTTPassword() const { return _mqttPassword; }
    void setMQTTPassword(const String& value);
    
    // ========== Sensor Addresses ==========
    void setSensorAddress(uint8_t index, const uint8_t* address);
    void getSensorAddress(uint8_t index, uint8_t* address) const;
    bool isSensorEnabled(uint8_t index) const;
    void setSensorEnabled(uint8_t index, bool enabled);
    
    // Check if settings have been modified
    bool isDirty() const { return _dirty; }
    
private:
    Settings() = default;
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;
    
    void loadDefaults();
    void loadFromNVS();
    void saveToNVS();
    
    Preferences _prefs;
    bool _dirty = false;
    bool _initialized = false;
    
    // Temperature settings
    float _heatingSetpoint = TempLimits::SETPOINT_DEFAULT;
    float _coolingSetpoint = TempLimits::COOLING_SETPOINT_DEFAULT;
    float _dhwSetpoint = TempLimits::DHW_SETPOINT_DEFAULT;
    float _dhwHysteresis = TempLimits::DHW_HYSTERESIS_DEFAULT;
    float _chHysteresis = TempLimits::CH_HYSTERESIS_DEFAULT;
    float _eevSetpoint = EEVConfig::TARGET_TEMP_DIFF;
    float _maxWatts = PowerConfig::MAX_WATTS;
    
    // Feature flags
    bool _dhwEnabled = false;
    bool _bufferEnabled = false;
    bool _valve4wayInstalled = false;
    bool _coolingModeEnabled = false;
    Season _season = Season::WINTER;
    
    // Network settings
    String _wifiSSID = NetworkConfig::DEFAULT_SSID;
    String _wifiPassword = NetworkConfig::DEFAULT_PASSWORD;
    String _mqttServer = NetworkConfig::MQTT_SERVER;
    uint16_t _mqttPort = NetworkConfig::MQTT_PORT;
    String _mqttUser = NetworkConfig::MQTT_USER;
    String _mqttPassword = NetworkConfig::MQTT_PASSWORD;
    
    // Sensor configuration
    uint8_t _sensorAddresses[SensorConfig::SENSOR_COUNT][8] = {0};
    uint16_t _sensorsEnabled = 0;  // Bitmask of enabled sensors
};

// Convenience macro for accessing settings
#define settings Settings::getInstance()

#endif // SETTINGS_H
