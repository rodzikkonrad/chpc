/**
 * @file temperature_sensors.h
 * @brief Dallas DS18B20 temperature sensor management
 * 
 * Handles reading and management of all temperature sensors
 * on the OneWire bus.
 */

#ifndef TEMPERATURE_SENSORS_H
#define TEMPERATURE_SENSORS_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"

/**
 * @struct TemperatureSensor
 * @brief Represents a single temperature sensor
 */
struct TemperatureSensor {
    uint8_t address[8];         // OneWire address
    float temperature;          // Current temperature reading
    bool enabled;               // Whether sensor is configured
    bool valid;                 // Whether last reading was valid
    uint32_t lastReadTime;      // Last successful read timestamp
    uint8_t errorCount;         // Consecutive read errors
    
    TemperatureSensor() : temperature(TempLimits::SENSOR_ERROR), 
                          enabled(false), valid(false), 
                          lastReadTime(0), errorCount(0) {
        memset(address, 0, 8);
    }
};

/**
 * @class TemperatureSensors
 * @brief Manages all Dallas temperature sensors
 */
class TemperatureSensors {
public:
    // Singleton access
    static TemperatureSensors& getInstance() {
        static TemperatureSensors instance;
        return instance;
    }
    
    /**
     * @brief Initialize the OneWire bus and sensors
     */
    void begin();
    
    /**
     * @brief Request temperature conversion from all sensors (async)
     */
    void requestTemperatures();
    
    /**
     * @brief Read temperatures from all sensors
     * @note Call after requestTemperatures() with appropriate delay
     */
    void readTemperatures();
    
    /**
     * @brief Perform full update cycle (request + read)
     */
    void update();
    
    /**
     * @brief Get temperature for a specific sensor
     * @param index Sensor index from SensorConfig::SensorIndex
     * @return Temperature in Celsius, or SENSOR_ERROR if invalid
     */
    float getTemperature(uint8_t index) const;
    
    /**
     * @brief Check if a sensor is enabled and has valid readings
     * @param index Sensor index
     * @return true if sensor is usable
     */
    bool isSensorValid(uint8_t index) const;
    
    /**
     * @brief Check if sensor is enabled (configured)
     * @param index Sensor index
     * @return true if sensor is configured
     */
    bool isSensorEnabled(uint8_t index) const;
    
    /**
     * @brief Get sensor reference for direct access
     * @param index Sensor index
     * @return Reference to sensor struct
     */
    const TemperatureSensor& getSensor(uint8_t index) const;
    
    /**
     * @brief Scan bus for new sensors
     * @return Number of devices found
     */
    uint8_t scanBus();
    
    /**
     * @brief Get number of sensors found on bus
     * @return Sensor count
     */
    uint8_t getDeviceCount() const { return _deviceCount; }
    
    /**
     * @brief Configure a sensor slot with an address
     * @param index Sensor index to configure
     * @param address 8-byte OneWire address
     */
    void configureSensor(uint8_t index, const uint8_t* address);
    
    /**
     * @brief Print all sensor addresses to Serial
     */
    void printAddresses();
    
    /**
     * @brief Get overall sensor system status
     * @return true if all required sensors are working
     */
    bool isSystemHealthy() const;
    
    /**
     * @brief Check if any sensor has an error
     * @return true if any configured sensor is in error state
     */
    bool hasErrors() const;
    
    // Convenience accessors for common sensors
    float getTae() const { return getTemperature(SensorConfig::SENSOR_TAE); }
    float getTbe() const { return getTemperature(SensorConfig::SENSOR_TBE); }
    float getTtarget() const { return getTemperature(SensorConfig::SENSOR_TTARGET); }
    float getTsump() const { return getTemperature(SensorConfig::SENSOR_TSUMP); }
    float getTci() const { return getTemperature(SensorConfig::SENSOR_TCI); }
    float getTco() const { return getTemperature(SensorConfig::SENSOR_TCO); }
    float getThi() const { return getTemperature(SensorConfig::SENSOR_THI); }
    float getTho() const { return getTemperature(SensorConfig::SENSOR_THO); }
    float getTbc() const { return getTemperature(SensorConfig::SENSOR_TBC); }
    float getTac() const { return getTemperature(SensorConfig::SENSOR_TAC); }
    float getTouter() const { return getTemperature(SensorConfig::SENSOR_TOUTER); }
    float getTcwu() const { return getTemperature(SensorConfig::SENSOR_TCWU); }
    float getTs2() const { return getTemperature(SensorConfig::SENSOR_TS2); }
    
private:
    TemperatureSensors();
    ~TemperatureSensors() = default;
    TemperatureSensors(const TemperatureSensors&) = delete;
    TemperatureSensors& operator=(const TemperatureSensors&) = delete;
    
    float readSensor(uint8_t index);
    bool validateReading(float temp) const;
    void loadSensorConfig();
    
    OneWire _oneWire;
    DallasTemperature _dallas;
    TemperatureSensor _sensors[SensorConfig::SENSOR_COUNT];
    uint8_t _deviceCount = 0;
    bool _conversionRequested = false;
    uint32_t _conversionRequestTime = 0;
    
    static constexpr uint16_t CONVERSION_DELAY_MS = 750;  // 12-bit resolution
    static constexpr uint8_t MAX_ERROR_COUNT = 5;
};

// Convenience macro
#define tempSensors TemperatureSensors::getInstance()

#endif // TEMPERATURE_SENSORS_H
