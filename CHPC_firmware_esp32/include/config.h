/**
 * @file config.h
 * @brief Configuration header for CHPC ESP32 Heat Pump Controller
 * 
 * This file contains all configurable parameters for the heat pump controller.
 * Migrated from Arduino Pro Mini to ESP32 with Home Assistant integration.
 * 
 * @author Original: Gonzho (gonzho@web.de)
 * @author Modified: Waldek, kondi
 * @date 2025
 * @license GPL-3.0
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// VERSION INFO
// ============================================================================
#define FW_VERSION          "3.0.0-ESP32"
#define HW_VERSION          "ESP32-CHPC"

// ============================================================================
// FEATURE FLAGS
// ============================================================================
#define ENABLE_WIFI             1       // WiFi connectivity
#define ENABLE_MQTT             1       // Home Assistant MQTT integration
#define ENABLE_OTA              1       // Over-the-air updates
#define ENABLE_WEB_SERVER       1       // Web configuration interface
#define ENABLE_DISPLAY          1       // LCD display support
#define ENABLE_EEV              1       // Electronic Expansion Valve support
#define ENABLE_DHW              1       // Domestic Hot Water support
#define ENABLE_BUFFER           1       // Buffer tank support
#define ENABLE_COOLING          1       // Cooling mode (4-way valve)
#define ENABLE_WATCHDOG         1       // Hardware watchdog

// ============================================================================
// NETWORK CONFIGURATION
// ============================================================================
namespace NetworkConfig {
    // WiFi credentials (can be overridden via web interface)
    constexpr const char* DEFAULT_SSID = "YourSSID";
    constexpr const char* DEFAULT_PASSWORD = "YourPassword";
    constexpr const char* HOSTNAME = "chpc-heatpump";
    
    // MQTT settings for Home Assistant
    constexpr const char* MQTT_SERVER = "homeassistant.local";
    constexpr uint16_t MQTT_PORT = 1883;
    constexpr const char* MQTT_USER = "mqtt_user";
    constexpr const char* MQTT_PASSWORD = "mqtt_password";
    constexpr const char* MQTT_CLIENT_ID = "chpc_heatpump";
    
    // MQTT topics (Home Assistant discovery compatible)
    constexpr const char* MQTT_BASE_TOPIC = "homeassistant";
    constexpr const char* MQTT_STATE_TOPIC = "chpc/state";
    constexpr const char* MQTT_COMMAND_TOPIC = "chpc/command";
    constexpr const char* MQTT_AVAILABILITY_TOPIC = "chpc/availability";
    
    // Update intervals
    constexpr uint32_t MQTT_PUBLISH_INTERVAL_MS = 10000;    // 10 seconds
    constexpr uint32_t MQTT_RECONNECT_INTERVAL_MS = 5000;   // 5 seconds
}

// ============================================================================
// GPIO PIN DEFINITIONS (ESP32)
// ============================================================================
namespace Pins {
    // OneWire temperature sensors
    constexpr uint8_t ONEWIRE_BUS = 4;          // GPIO4 for Dallas sensors
    
    // Relay outputs (directly connected to board)
    // Note: Choosing GPIOs that are safe for outputs (no boot conflicts)
    constexpr uint8_t RELAY_COMPRESSOR = 16;    // Heat pump compressor
    constexpr uint8_t RELAY_HOT_PUMP = 17;      // Hot side circulation pump
    constexpr uint8_t RELAY_COLD_PUMP = 18;     // Cold side circulation pump  
    constexpr uint8_t RELAY_SUMP_HEATER = 19;   // Compressor sump heater / DHW valve
    constexpr uint8_t RELAY_4WAY_VALVE = 21;    // 4-way reversing valve
    
    // EEV stepper motor control
    constexpr uint8_t EEV_COIL_1 = 25;
    constexpr uint8_t EEV_COIL_2 = 26;
    constexpr uint8_t EEV_COIL_3 = 27;
    constexpr uint8_t EEV_COIL_4 = 14;
    
    // Current sensor (CT clamp)
    constexpr uint8_t CURRENT_SENSOR = 34;      // ADC1 channel (GPIO34)
    
    // User interface
    constexpr uint8_t BUTTON_UP = 32;
    constexpr uint8_t BUTTON_DOWN = 33;
    constexpr uint8_t BUTTON_MENU = 35;
    constexpr uint8_t BUTTON_SELECT = 39;       // VN pin
    
    // I2C for display
    constexpr uint8_t I2C_SDA = 22;
    constexpr uint8_t I2C_SCL = 23;
    
    // Status LED
    constexpr uint8_t STATUS_LED = 2;           // Built-in LED on most ESP32 boards
    
    // Buzzer/Speaker
    constexpr uint8_t BUZZER = 5;
}

// ============================================================================
// TEMPERATURE LIMITS AND THRESHOLDS
// ============================================================================
namespace TempLimits {
    // User-adjustable setpoint limits
    constexpr float SETPOINT_MIN = 15.0f;
    constexpr float SETPOINT_MAX = 55.0f;
    constexpr float SETPOINT_DEFAULT = 21.5f;
    
    // Cooling mode setpoint
    constexpr float COOLING_SETPOINT_MIN = 5.0f;
    constexpr float COOLING_SETPOINT_MAX = 18.0f;
    constexpr float COOLING_SETPOINT_DEFAULT = 7.0f;
    
    // DHW (Domestic Hot Water) settings
    constexpr float DHW_SETPOINT_MIN = 30.0f;
    constexpr float DHW_SETPOINT_MAX = 55.0f;
    constexpr float DHW_SETPOINT_DEFAULT = 45.0f;
    constexpr float DHW_HYSTERESIS_DEFAULT = 2.0f;
    
    // Central heating hysteresis
    constexpr float CH_HYSTERESIS_DEFAULT = 4.0f;
    
    // Protection limits
    constexpr float SUMP_MIN = 9.0f;            // HP won't start if compressor temp lower
    constexpr float SUMP_MAX = 110.0f;          // HP stops if compressor temp higher
    constexpr float SUMP_HEATER_THRESHOLD = 16.0f;  // Enable sump heater below this
    constexpr float DISCHARGE_MAX = 108.0f;     // Max discharge temperature
    constexpr float SUCTION_MIN = -10.0f;       // Min suction temperature (anti-freeze)
    constexpr float COLD_LOOP_MIN = 1.0f;       // Cold loop anti-freeze threshold
    constexpr float HOT_OUT_MAX = 55.0f;        // Max hot side outlet temperature
    constexpr float WORKING_SUMP_MIN = 24.0f;   // Min sump temp after 5 min of operation
    
    // Sensor error value
    constexpr float SENSOR_ERROR = -127.0f;
    constexpr float SENSOR_INITIAL = 85.0f;
}

// ============================================================================
// TIMING CONSTANTS
// ============================================================================
namespace Timing {
    // Startup and cycle delays (in milliseconds)
    constexpr uint32_t POWERON_PAUSE_MS = 90000;        // 90 sec initial wait
    constexpr uint32_t COMPRESSOR_DELAY_MS = 45000;     // Cold pump runs before compressor
    constexpr uint32_t COLD_PUMP_DELAY_MS = 60000;      // Cold pump runs after compressor stops
    constexpr uint32_t MIN_CYCLE_OFF_MS = 900000;       // 15 min minimum off time
    constexpr uint32_t MIN_CYCLE_ON_MS = 600000;        // 10 min minimum on time
    constexpr uint32_t POWERON_HIGH_TIME_MS = 20000;    // Higher power allowed at startup
    constexpr uint32_t HOT_PUMP_DEFERRED_STOP_MS = 300000;  // 5 min hot pump runs after HP stops
    
    // DHW timing
    constexpr uint32_t DHW_INTERVAL_MS = 7200000;       // 2 hours between DHW cycles
    constexpr uint32_t DHW_MAX_HEATING_MS = 3600000;    // 1 hour max DHW heating
    
    // EEV timing
    constexpr uint32_t EEV_RECALIBRATE_MS = 86400000;   // 24 hours between full EEV recalibration
    constexpr uint16_t EEV_PULSE_FAST_CLOSE_MS = 20;
    constexpr uint16_t EEV_PULSE_SLOW_CLOSE_MS = 4000;
    constexpr uint16_t EEV_PULSE_WAIT_OPEN_MS = 20;
    constexpr uint16_t EEV_PULSE_FAST_OPEN_MS = 1000;
    constexpr uint16_t EEV_PULSE_SLOW_OPEN_MS = 7000;
    constexpr uint16_t EEV_HOLD_MS = 500;
    
    // Main loop timing
    constexpr uint32_t MAIN_CYCLE_MS = 1000;            // Main loop interval
    constexpr uint32_t DISPLAY_UPDATE_MS = 10000;       // Display refresh rate
    constexpr uint32_t EEPROM_SAVE_INTERVAL_MS = 900000; // 15 min between EEPROM saves
}

// ============================================================================
// EEV (Electronic Expansion Valve) CONFIGURATION
// ============================================================================
namespace EEVConfig {
    constexpr int16_t MAX_PULSES = 500;
    constexpr int16_t CLOSE_ADD_PULSES = 8;
    constexpr int16_t OPEN_AFTER_CLOSE = 100;           // Waiting position
    constexpr int16_t MIN_WORK_POSITION = 90;           // Min position during operation
    
    // Control parameters
    constexpr float TARGET_TEMP_DIFF = 4.0f;            // Target superheat
    constexpr float PRECISE_START = 8.6f;               // Threshold for slow control
    constexpr float EMERGENCY_DIFF = 3.5f;              // Emergency close threshold
    constexpr float HYSTERESIS = 0.6f;                  // Control hysteresis
}

// ============================================================================
// POWER MONITORING
// ============================================================================
namespace PowerConfig {
    constexpr float MAX_WATTS = 1800.0f;                // Maximum power consumption
    constexpr float MIN_WORKING_WATTS_RATIO = 0.3f;     // Min power = MAX_WATTS * this ratio
    constexpr float CURRENT_CALIBRATION = 62.5f;        // CT sensor calibration factor
    constexpr uint16_t CURRENT_SAMPLES = 2960;          // Samples per measurement cycle
}

// ============================================================================
// TEMPERATURE SENSOR ADDRESSES
// ============================================================================
// These will be stored in NVS (Non-Volatile Storage) after first configuration
namespace SensorConfig {
    // Sensor indices for array access
    enum SensorIndex : uint8_t {
        SENSOR_TAE = 0,         // After evaporator
        SENSOR_TBE,             // Before evaporator
        SENSOR_TTARGET,         // Target/room temperature
        SENSOR_TSUMP,           // Compressor sump
        SENSOR_TCI,             // Cold in
        SENSOR_TCO,             // Cold out
        SENSOR_THI,             // Hot in
        SENSOR_THO,             // Hot out
        SENSOR_TBC,             // Before condenser (discharge)
        SENSOR_TAC,             // After condenser
        SENSOR_TOUTER,          // Outdoor temperature
        SENSOR_TCWU,            // DHW tank temperature
        SENSOR_TS2,             // Buffer/additional sensor
        SENSOR_COUNT            // Total number of sensors
    };
    
    // Sensor names for display and MQTT
    constexpr const char* SENSOR_NAMES[SENSOR_COUNT] = {
        "Tae", "Tbe", "Ttarget", "Tsump", "Tci", "Tco",
        "Thi", "Tho", "Tbc", "Tac", "Touter", "Tcwu", "Ts2"
    };
    
    // Sensor descriptions for Home Assistant
    constexpr const char* SENSOR_DESCRIPTIONS[SENSOR_COUNT] = {
        "After Evaporator", "Before Evaporator", "Target/Room", "Compressor Sump",
        "Cold In", "Cold Out", "Hot In", "Hot Out", "Before Condenser",
        "After Condenser", "Outdoor", "DHW Tank", "Buffer/Secondary"
    };
}

// ============================================================================
// ERROR CODES
// ============================================================================
enum class ErrorCode : uint8_t {
    OK = 0,
    SENSOR_ERROR = 1,
    HOT_PUMP_ERROR = 2,
    COLD_PUMP_ERROR = 3,
    HEATPUMP_ERROR = 4,
    WATTAGE_ERROR = 5,
    OVERLOAD_ERROR = 6,
    LACK_OF_START = 7,
    COMMUNICATION_ERROR = 8
};

// ============================================================================
// SYSTEM STATES
// ============================================================================
enum class SystemMode : uint8_t {
    OFF = 0,
    STANDBY = 1,
    HEATING = 2,
    COOLING = 3,
    DHW_HEATING = 4,
    DEFROST = 5,
    ERROR = 6
};

enum class Season : uint8_t {
    WINTER = 0,     // Central heating + DHW
    SUMMER = 1      // DHW only
};

#endif // CONFIG_H
