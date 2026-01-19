/**
 * @file display_manager.cpp
 * @brief Implementation of DisplayManager
 */

#include "display_manager.h"
#include "heat_pump_controller.h"
#include "temperature_sensors.h"
#include "eev_controller.h"
#include "settings.h"
#include "wifi_manager.h"

DisplayManager::DisplayManager() : _lcd(0x27, 16, 2) {}

void DisplayManager::begin() {
    Serial.println(F("[Display] Initializing..."));
    
    Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL);
    
    _lcd.init();
    _lcd.backlight();
    
    // Show startup message
    _lcd.clear();
    _lcd.setCursor(0, 0);
    _lcd.print("CHPC Heat Pump");
    _lcd.setCursor(0, 1);
    _lcd.print("v" FW_VERSION);
    
    _initialized = true;
    Serial.println(F("[Display] Initialization complete"));
}

void DisplayManager::update() {
    if (!_initialized) return;
    
    uint32_t now = millis();
    if (now - _lastUpdate < UPDATE_INTERVAL_MS) return;
    _lastUpdate = now;
    
    showMainScreen();
}

void DisplayManager::showMainScreen() {
    _lcd.clear();
    
    const HeatPumpStatus& status = heatPumpController.getStatus();
    
    // Line 1: Target and actual temperature
    _lcd.setCursor(0, 0);
    
    // Show mode indicator
    switch (status.mode) {
        case SystemMode::HEATING:
            _lcd.print("H");
            break;
        case SystemMode::COOLING:
            _lcd.print("C");
            break;
        case SystemMode::DHW_HEATING:
            _lcd.print("W");
            break;
        case SystemMode::ERROR:
            _lcd.print("E");
            break;
        default:
            _lcd.print("-");
            break;
    }
    
    // Target temperature
    _lcd.print(":");
    _lcd.print(heatPumpController.getTargetTemperature(), 1);
    
    // Separator
    _lcd.print(" R:");
    
    // Actual temperature
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_TTARGET)) {
        _lcd.print(tempSensors.getTtarget(), 1);
    } else {
        _lcd.print("ERR");
    }
    
    // Line 2: Component status and EEV
    _lcd.setCursor(0, 1);
    
    // Component indicators
    _lcd.print(status.compressorRunning ? "C" : "c");
    _lcd.print(status.coldPumpRunning ? "L" : "l");
    _lcd.print(status.hotPumpRunning ? "H" : "h");
    
#if ENABLE_EEV
    // EEV position
    _lcd.print(" E:");
    _lcd.print((int)eevController.getPositionPercent());
    _lcd.print("%");
#endif
    
    // Power consumption
    _lcd.setCursor(11, 1);
    _lcd.print((int)status.currentPower);
    _lcd.print("W");
}

void DisplayManager::showMessage(const String& line1, const String& line2) {
    if (!_initialized) return;
    
    _lcd.clear();
    _lcd.setCursor(0, 0);
    _lcd.print(line1.substring(0, 16));
    
    if (line2.length() > 0) {
        _lcd.setCursor(0, 1);
        _lcd.print(line2.substring(0, 16));
    }
}

void DisplayManager::showError(uint8_t errorCode) {
    if (!_initialized) return;
    
    _lcd.clear();
    _lcd.setCursor(0, 0);
    _lcd.print("ERROR: ");
    _lcd.print(errorCode);
    
    _lcd.setCursor(0, 1);
    switch (static_cast<ErrorCode>(errorCode)) {
        case ErrorCode::SENSOR_ERROR:
            _lcd.print("Sensor fault");
            break;
        case ErrorCode::HEATPUMP_ERROR:
            _lcd.print("HP fault");
            break;
        case ErrorCode::OVERLOAD_ERROR:
            _lcd.print("Power overload");
            break;
        case ErrorCode::LACK_OF_START:
            _lcd.print("Start fail");
            break;
        default:
            _lcd.print("Unknown");
            break;
    }
}

void DisplayManager::setBacklight(bool on) {
    if (!_initialized) return;
    
    if (on) {
        _lcd.backlight();
    } else {
        _lcd.noBacklight();
    }
}
