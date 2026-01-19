/**
 * @file display_manager.h
 * @brief LCD display management
 * 
 * Handles 16x2 or 20x4 LCD display output.
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"

/**
 * @class DisplayManager
 * @brief Manages LCD display output
 */
class DisplayManager {
public:
    // Singleton access
    static DisplayManager& getInstance() {
        static DisplayManager instance;
        return instance;
    }
    
    /**
     * @brief Initialize display
     */
    void begin();
    
    /**
     * @brief Update display content
     */
    void update();
    
    /**
     * @brief Show message on display
     */
    void showMessage(const String& line1, const String& line2 = "");
    
    /**
     * @brief Show error message
     */
    void showError(uint8_t errorCode);
    
    /**
     * @brief Toggle backlight
     */
    void setBacklight(bool on);
    
private:
    DisplayManager();
    ~DisplayManager() = default;
    DisplayManager(const DisplayManager&) = delete;
    DisplayManager& operator=(const DisplayManager&) = delete;
    
    void showMainScreen();
    void showMenuScreen();
    
    LiquidCrystal_I2C _lcd;
    
    uint32_t _lastUpdate = 0;
    uint8_t _currentScreen = 0;
    bool _initialized = false;
    
    static constexpr uint32_t UPDATE_INTERVAL_MS = 1000;
};

// Convenience macro
#define displayManager DisplayManager::getInstance()

#endif // DISPLAY_MANAGER_H
