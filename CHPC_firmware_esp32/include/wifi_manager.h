/**
 * @file wifi_manager.h
 * @brief WiFi connection and web server management
 * 
 * Handles WiFi connectivity, AP mode for configuration,
 * and web-based configuration interface.
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include "config.h"

/**
 * @enum WiFiState
 * @brief Current WiFi connection state
 */
enum class WiFiState : uint8_t {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    AP_MODE,
    ERROR
};

/**
 * @class WiFiManager
 * @brief Manages WiFi connectivity and web server
 */
class WiFiManager {
public:
    // Singleton access
    static WiFiManager& getInstance() {
        static WiFiManager instance;
        return instance;
    }
    
    /**
     * @brief Initialize WiFi and web server
     */
    void begin();
    
    /**
     * @brief Main update loop
     */
    void update();
    
    /**
     * @brief Get current WiFi state
     */
    WiFiState getState() const { return _state; }
    
    /**
     * @brief Check if connected to WiFi
     */
    bool isConnected() const { return _state == WiFiState::CONNECTED; }
    
    /**
     * @brief Get IP address as string
     */
    String getIPAddress() const;
    
    /**
     * @brief Get signal strength (RSSI)
     */
    int32_t getRSSI() const { return WiFi.RSSI(); }
    
    /**
     * @brief Force reconnection
     */
    void reconnect();
    
    /**
     * @brief Start AP mode for configuration
     */
    void startAPMode();
    
    /**
     * @brief Stop AP mode
     */
    void stopAPMode();
    
private:
    WiFiManager();
    ~WiFiManager() = default;
    WiFiManager(const WiFiManager&) = delete;
    WiFiManager& operator=(const WiFiManager&) = delete;
    
    // Web server setup
    void setupWebServer();
    void setupAPIRoutes();
    
    // HTML page generators
    String getHomePage();
    String getConfigPage();
    String getSensorsPage();
    String getStatusJSON();
    String getSensorsJSON();
    
    AsyncWebServer _server;
    WiFiState _state = WiFiState::DISCONNECTED;
    
    uint32_t _connectStartTime = 0;
    uint32_t _lastConnectAttempt = 0;
    bool _apModeActive = false;
    
    static constexpr uint32_t CONNECT_TIMEOUT_MS = 30000;
    static constexpr uint32_t RECONNECT_INTERVAL_MS = 30000;
};

// Convenience macro
#define wifiManager WiFiManager::getInstance()

#endif // WIFI_MANAGER_H
