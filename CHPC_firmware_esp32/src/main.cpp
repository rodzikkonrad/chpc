/**
 * @file main.cpp
 * @brief CHPC Heat Pump Controller - Main Entry Point
 * @version 2.0-ESP32
 * 
 * Cheap Heat Pump Controller (CHPC) firmware for ESP32
 * Migrated from Arduino Pro Mini version
 * 
 * Features:
 * - Temperature monitoring (13 DS18B20 sensors)
 * - Electronic Expansion Valve (EEV) control
 * - Heat pump start/stop logic with protections
 * - DHW (Domestic Hot Water) support
 * - WiFi connectivity
 * - Home Assistant integration via MQTT
 * - Web-based configuration interface
 * - OTA updates
 * 
 * @author Original: WaldemarPachol, ESP32 port: AI-assisted
 * @license GPL-3.0
 */

#include <Arduino.h>

// Watchdog disabled for now - ESP32 Arduino core uses different API
// #if ENABLE_WATCHDOG
// #include <esp_task_wdt.h>
// #endif

#include "config.h"
#include "settings.h"
#include "temperature_sensors.h"
#include "eev_controller.h"
#include "relay_controller.h"
#include "heat_pump_controller.h"
#include "mqtt_manager.h"
#include "wifi_manager.h"
#include "display_manager.h"

// Status LED control
static uint32_t lastLedToggle = 0;
static bool ledState = false;

// Timing for periodic tasks
static uint32_t lastSerialPrint = 0;
static uint32_t lastStatusCheck = 0;

// Forward declarations
void printSystemStatus();
void blinkStatusLed();

/**
 * @brief Arduino setup function
 */
void setup() {
    // Initialize serial for debugging
    Serial.begin(115200);
    delay(1000); // Wait for serial monitor
    
    Serial.println();
    Serial.println(F("================================================"));
    Serial.println(F("  CHPC Heat Pump Controller v" FW_VERSION));
    Serial.println(F("  ESP32 Edition"));
    Serial.println(F("================================================"));
    Serial.println();
    
    // Initialize status LED
    pinMode(Pins::STATUS_LED, OUTPUT);
    digitalWrite(Pins::STATUS_LED, LOW);
    
    // Initialize modules in dependency order
    Serial.println(F("[Main] Initializing modules..."));
    
    // 1. Settings (load from NVS)
    settings.begin();
    
    // 2. Display (for visual feedback during startup)
    displayManager.begin();
    displayManager.showMessage("Starting...", "Loading config");
    
    // 3. WiFi (connect to network)
    displayManager.showMessage("Starting...", "Connecting WiFi");
    wifiManager.begin();
    
    // 4. MQTT (connect to broker)
    displayManager.showMessage("Starting...", "Connecting MQTT");
    mqttManager.begin();
    
    // 5. Temperature sensors
    displayManager.showMessage("Starting...", "Init sensors");
    tempSensors.begin();
    
    // 6. EEV controller
    displayManager.showMessage("Starting...", "Init EEV");
    eevController.begin();
    
    // 7. Relay controller
    displayManager.showMessage("Starting...", "Init relays");
    relayController.begin();
    
    // 8. Heat pump controller (main logic)
    displayManager.showMessage("Starting...", "Init HP logic");
    heatPumpController.begin();
    
    // Give sensors time to take first reading
    Serial.println(F("[Main] Waiting for initial sensor readings..."));
    displayManager.showMessage("Starting...", "Reading sensors");
    delay(2000);
    
    // Indicate startup complete
    Serial.println();
    Serial.println(F("================================================"));
    Serial.println(F("  CHPC Ready!"));
    Serial.println(F("================================================"));
    Serial.println();
    
    // Print initial configuration
    printSystemStatus();
    
    // Show ready on display
    displayManager.showMessage("CHPC Ready", wifiManager.getIPAddress());
    delay(2000);
}

/**
 * @brief Arduino main loop
 */
void loop() {
    uint32_t now = millis();
    
    // Update status LED
    blinkStatusLed();
    
    // Update all modules
    
    // WiFi and web server (handles client requests)
    wifiManager.update();
    
    // MQTT connection and message handling
    mqttManager.update();
    
    // Temperature sensor reading (async)
    tempSensors.update();
    
    // EEV control (stepper motor and superheat calculation)
    eevController.update();
    
    // Relay state management
    relayController.update();
    
    // Main heat pump control logic
    heatPumpController.update();
    
    // LCD display update
    displayManager.update();
    
    // Periodic status print to serial
    if (now - lastSerialPrint >= 30000) { // Every 30 seconds
        lastSerialPrint = now;
        printSystemStatus();
    }
    
    // Check for error conditions
    if (now - lastStatusCheck >= 5000) { // Every 5 seconds
        lastStatusCheck = now;
        
        const HeatPumpStatus& status = heatPumpController.getStatus();
        
        if (status.mode == SystemMode::ERROR) {
            // Show error on display
            displayManager.showError(status.lastError);
        }
    }
    
    // Small yield to prevent watchdog issues
    yield();
}

/**
 * @brief Blink status LED based on system state
 */
void blinkStatusLed() {
    uint32_t now = millis();
    uint32_t interval;
    
    const HeatPumpStatus& status = heatPumpController.getStatus();
    
    // Determine blink interval based on system state
    if (status.mode == SystemMode::ERROR) {
        interval = 100;  // Fast blink for error
    } else if (wifiManager.getState() != WiFiState::CONNECTED) {
        interval = 500;  // Medium blink for no WiFi
    } else if (status.compressorRunning) {
        interval = 2000; // Slow blink when running normally
    } else {
        interval = 1000; // Normal blink when idle
    }
    
    if (now - lastLedToggle >= interval) {
        lastLedToggle = now;
        ledState = !ledState;
        digitalWrite(Pins::STATUS_LED, ledState ? HIGH : LOW);
    }
}

/**
 * @brief Print system status to serial
 */
void printSystemStatus() {
    const HeatPumpStatus& status = heatPumpController.getStatus();
    
    Serial.println(F("============ SYSTEM STATUS ============"));
    
    // WiFi status
    Serial.print(F("WiFi: "));
    if (wifiManager.getState() == WiFiState::CONNECTED) {
        Serial.print(F("Connected, IP: "));
        Serial.println(wifiManager.getIPAddress());
    } else {
        Serial.println(F("Disconnected"));
    }
    
    // MQTT status
    Serial.print(F("MQTT: "));
    Serial.println(mqttManager.isConnected() ? F("Connected") : F("Disconnected"));
    
    // System mode
    Serial.print(F("Mode: "));
    switch (status.mode) {
        case SystemMode::OFF:
            Serial.println(F("OFF"));
            break;
        case SystemMode::STANDBY:
            Serial.println(F("STANDBY"));
            break;
        case SystemMode::HEATING:
            Serial.println(F("HEATING"));
            break;
        case SystemMode::COOLING:
            Serial.println(F("COOLING"));
            break;
        case SystemMode::DHW_HEATING:
            Serial.println(F("DHW HEATING"));
            break;
        case SystemMode::ERROR:
            Serial.print(F("ERROR #"));
            Serial.println(status.lastError);
            break;
    }
    
    // Setpoints
    Serial.print(F("Target: "));
    Serial.print(heatPumpController.getTargetTemperature(), 1);
    Serial.println(F("°C"));
    
#if ENABLE_DHW
    Serial.print(F("DHW Target: "));
    Serial.print(settings.getDHWSetpoint(), 1);
    Serial.println(F("°C"));
#endif
    
    // Key temperatures
    Serial.println(F("--- Temperatures ---"));
    
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_TTARGET)) {
        Serial.print(F("  Ttarget: "));
        Serial.print(tempSensors.getTtarget(), 2);
        Serial.println(F("°C"));
    }
    
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_TAE)) {
        Serial.print(F("  Tae (evap after): "));
        Serial.print(tempSensors.getTae(), 2);
        Serial.println(F("°C"));
    }
    
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_TBE)) {
        Serial.print(F("  Tbe (evap before): "));
        Serial.print(tempSensors.getTbe(), 2);
        Serial.println(F("°C"));
    }
    
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_TSUMP)) {
        Serial.print(F("  Tsump: "));
        Serial.print(tempSensors.getTsump(), 2);
        Serial.println(F("°C"));
    }
    
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_THI)) {
        Serial.print(F("  Thi (hot in): "));
        Serial.print(tempSensors.getThi(), 2);
        Serial.println(F("°C"));
    }
    
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_THO)) {
        Serial.print(F("  Tho (hot out): "));
        Serial.print(tempSensors.getTho(), 2);
        Serial.println(F("°C"));
    }
    
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_TOUTER)) {
        Serial.print(F("  Touter: "));
        Serial.print(tempSensors.getTouter(), 2);
        Serial.println(F("°C"));
    }
    
#if ENABLE_DHW
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_TCWU)) {
        Serial.print(F("  Tcwu (DHW): "));
        Serial.print(tempSensors.getTcwu(), 2);
        Serial.println(F("°C"));
    }
#endif
    
    // Components
    Serial.println(F("--- Components ---"));
    Serial.print(F("  Compressor: "));
    Serial.println(status.compressorRunning ? F("ON") : F("OFF"));
    Serial.print(F("  Cold Pump: "));
    Serial.println(status.coldPumpRunning ? F("ON") : F("OFF"));
    Serial.print(F("  Hot Pump: "));
    Serial.println(status.hotPumpRunning ? F("ON") : F("OFF"));
    
#if ENABLE_COOLING
    Serial.print(F("  4-Way Valve: "));
    Serial.println(status.valve4wayActive ? F("COOLING") : F("HEATING"));
#endif
    
#if ENABLE_EEV
    Serial.println(F("--- EEV ---"));
    Serial.print(F("  Position: "));
    Serial.print(eevController.getPosition());
    Serial.print(F(" / "));
    Serial.print(EEVConfig::MAX_PULSES);
    Serial.print(F(" ("));
    Serial.print(eevController.getPositionPercent(), 1);
    Serial.println(F("%)"));
    Serial.print(F("  Target superheat: "));
    Serial.print(EEVConfig::TARGET_TEMP_DIFF, 1);
    Serial.println(F("K"));
#endif
    
    // Power
    Serial.println(F("--- Power ---"));
    Serial.print(F("  Current: "));
    Serial.print(status.currentPower, 0);
    Serial.println(F("W"));
    
    // Runtime
    Serial.println(F("--- Runtime ---"));
    Serial.print(F("  Uptime: "));
    uint32_t uptime = millis() / 1000;
    Serial.print(uptime / 3600);
    Serial.print(F("h "));
    Serial.print((uptime % 3600) / 60);
    Serial.print(F("m "));
    Serial.print(uptime % 60);
    Serial.println(F("s"));
    
    Serial.print(F("  Free heap: "));
    Serial.print(ESP.getFreeHeap() / 1024);
    Serial.println(F(" KB"));
    
    Serial.println(F("======================================="));
    Serial.println();
}
