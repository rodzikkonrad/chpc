/**
 * @file mqtt_manager.h
 * @brief MQTT communication for Home Assistant integration
 * 
 * Implements MQTT discovery and state publishing for Home Assistant.
 * Supports climate entity, sensors, and binary sensors.
 */

#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

/**
 * @class MQTTManager
 * @brief Handles all MQTT communications with Home Assistant
 */
class MQTTManager {
public:
    // Singleton access
    static MQTTManager& getInstance() {
        static MQTTManager instance;
        return instance;
    }
    
    /**
     * @brief Initialize MQTT connection
     */
    void begin();
    
    /**
     * @brief Main update loop - handle reconnection and publishing
     */
    void update();
    
    /**
     * @brief Check if connected to MQTT broker
     */
    bool isConnected() { return _client.connected(); }
    
    /**
     * @brief Force reconnection attempt
     */
    void reconnect();
    
    /**
     * @brief Publish all states immediately
     */
    void publishAllStates();
    
    /**
     * @brief Publish Home Assistant discovery configs
     */
    void publishDiscovery();
    
    /**
     * @brief Set callback for command received
     */
    using CommandCallback = void (*)(const String& topic, const String& payload);
    void setCommandCallback(CommandCallback callback) { _commandCallback = callback; }
    
private:
    MQTTManager();
    ~MQTTManager() = default;
    MQTTManager(const MQTTManager&) = delete;
    MQTTManager& operator=(const MQTTManager&) = delete;
    
    // MQTT message handling
    static void mqttCallback(char* topic, byte* payload, unsigned int length);
    void handleMessage(const String& topic, const String& payload);
    
    // Discovery configuration publishing
    void publishClimateDiscovery();
    void publishSensorDiscovery(const char* name, const char* id, 
                                 const char* deviceClass, const char* unit,
                                 const char* stateTopic, const char* valueTemplate);
    void publishBinarySensorDiscovery(const char* name, const char* id,
                                       const char* deviceClass,
                                       const char* stateTopic, const char* valueTemplate);
    void publishSwitchDiscovery(const char* name, const char* id,
                                 const char* stateTopic, const char* commandTopic);
    void publishNumberDiscovery(const char* name, const char* id,
                                 float min, float max, float step,
                                 const char* unit,
                                 const char* stateTopic, const char* commandTopic);
    
    // State publishing
    void publishTemperatures();
    void publishSystemState();
    void publishAvailability(bool available);
    
    // Helper functions
    String getDeviceJson();
    void subscribe(const char* topic);
    void publish(const char* topic, const String& payload, bool retain = false);
    
    WiFiClient _wifiClient;
    PubSubClient _client;
    
    uint32_t _lastConnectAttempt = 0;
    uint32_t _lastPublishTime = 0;
    bool _discoveryPublished = false;
    
    CommandCallback _commandCallback = nullptr;
    
    // Topic buffers
    static constexpr size_t TOPIC_BUFFER_SIZE = 128;
    char _topicBuffer[TOPIC_BUFFER_SIZE];
};

// Convenience macro
#define mqttManager MQTTManager::getInstance()

#endif // MQTT_MANAGER_H
