/**
 * @file mqtt_manager.cpp
 * @brief Implementation of MQTT Manager for Home Assistant
 */

#include "mqtt_manager.h"
#include "settings.h"
#include "heat_pump_controller.h"
#include "temperature_sensors.h"
#include "eev_controller.h"

// Static instance for callback
static MQTTManager* _instance = nullptr;

MQTTManager::MQTTManager() : _client(_wifiClient) {
    _instance = this;
}

void MQTTManager::begin() {
    Serial.println(F("[MQTT] Initializing..."));
    
    _client.setServer(settings.getMQTTServer().c_str(), settings.getMQTTPort());
    _client.setCallback(mqttCallback);
    _client.setBufferSize(1024);  // Larger buffer for discovery messages
    
    Serial.printf("[MQTT] Server: %s:%d\n", 
                  settings.getMQTTServer().c_str(), settings.getMQTTPort());
}

void MQTTManager::update() {
    uint32_t now = millis();
    
    if (!_client.connected()) {
        if (now - _lastConnectAttempt > NetworkConfig::MQTT_RECONNECT_INTERVAL_MS) {
            reconnect();
            _lastConnectAttempt = now;
        }
    } else {
        _client.loop();
        
        // Publish states periodically
        if (now - _lastPublishTime > NetworkConfig::MQTT_PUBLISH_INTERVAL_MS) {
            publishAllStates();
            _lastPublishTime = now;
        }
    }
}

void MQTTManager::reconnect() {
    if (_client.connected()) return;
    
    Serial.println(F("[MQTT] Connecting..."));
    
    String clientId = NetworkConfig::MQTT_CLIENT_ID;
    clientId += "_" + String(random(0xffff), HEX);
    
    // Last will for availability
    String availTopic = String(NetworkConfig::MQTT_AVAILABILITY_TOPIC);
    
    bool connected = _client.connect(
        clientId.c_str(),
        settings.getMQTTUser().c_str(),
        settings.getMQTTPassword().c_str(),
        availTopic.c_str(),
        0,      // QoS
        true,   // Retain
        "offline"
    );
    
    if (connected) {
        Serial.println(F("[MQTT] Connected!"));
        
        // Publish availability
        publishAvailability(true);
        
        // Publish discovery if not done
        if (!_discoveryPublished) {
            publishDiscovery();
            _discoveryPublished = true;
        }
        
        // Subscribe to command topics
        subscribe("chpc/command/#");
        subscribe("chpc/set/#");
        
        // Publish initial states
        publishAllStates();
    } else {
        Serial.printf("[MQTT] Connection failed, rc=%d\n", _client.state());
    }
}

void MQTTManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance) {
        String topicStr(topic);
        String payloadStr;
        payloadStr.reserve(length);
        for (unsigned int i = 0; i < length; i++) {
            payloadStr += (char)payload[i];
        }
        _instance->handleMessage(topicStr, payloadStr);
    }
}

void MQTTManager::handleMessage(const String& topic, const String& payload) {
    Serial.printf("[MQTT] Received: %s = %s\n", topic.c_str(), payload.c_str());
    
    // Parse command topics
    if (topic == "chpc/set/target_temp") {
        float temp = payload.toFloat();
        heatPumpController.setTargetTemperature(temp);
        Serial.printf("[MQTT] Set target temp: %.1f\n", temp);
    }
    else if (topic == "chpc/set/dhw_temp") {
        float temp = payload.toFloat();
        heatPumpController.setDHWTargetTemperature(temp);
        Serial.printf("[MQTT] Set DHW temp: %.1f\n", temp);
    }
    else if (topic == "chpc/set/mode") {
        if (payload == "heat") {
            heatPumpController.setCoolingMode(false);
        } else if (payload == "cool") {
            heatPumpController.setCoolingMode(true);
        } else if (payload == "off") {
            heatPumpController.emergencyStop();
        }
        Serial.printf("[MQTT] Set mode: %s\n", payload.c_str());
    }
    else if (topic == "chpc/set/dhw_enabled") {
        bool enabled = (payload == "ON" || payload == "true" || payload == "1");
        settings.setDHWEnabled(enabled);
        Serial.printf("[MQTT] Set DHW enabled: %d\n", enabled);
    }
    else if (topic == "chpc/set/summer_mode") {
        bool enabled = (payload == "ON" || payload == "true" || payload == "1");
        heatPumpController.setSummerMode(enabled);
        Serial.printf("[MQTT] Set summer mode: %d\n", enabled);
    }
    else if (topic == "chpc/command/force_dhw") {
        heatPumpController.forceDHWHeating();
    }
    else if (topic == "chpc/command/reset") {
        heatPumpController.reset();
    }
#if ENABLE_EEV
    else if (topic == "chpc/set/eev_target") {
        float target = payload.toFloat();
        eevController.setTargetSuperheat(target);
        Serial.printf("[MQTT] Set EEV target: %.1f\n", target);
    }
#endif
    
    // Notify callback if set
    if (_commandCallback) {
        _commandCallback(topic, payload);
    }
    
    // Publish updated state
    publishAllStates();
}

void MQTTManager::publishDiscovery() {
    Serial.println(F("[MQTT] Publishing Home Assistant discovery..."));
    
    // Climate entity (main thermostat)
    publishClimateDiscovery();
    
    // Temperature sensors
    for (uint8_t i = 0; i < SensorConfig::SENSOR_COUNT; i++) {
        if (tempSensors.isSensorEnabled(i)) {
            String id = String("temp_") + SensorConfig::SENSOR_NAMES[i];
            String stateTopic = String(NetworkConfig::MQTT_STATE_TOPIC) + "/temperatures";
            String valueTemplate = String("{{ value_json.") + SensorConfig::SENSOR_NAMES[i] + " }}";
            
            publishSensorDiscovery(
                SensorConfig::SENSOR_DESCRIPTIONS[i],
                id.c_str(),
                "temperature",
                "°C",
                stateTopic.c_str(),
                valueTemplate.c_str()
            );
        }
    }
    
    // Power sensor
    publishSensorDiscovery("Power Consumption", "power", "power", "W",
                           "chpc/state/system", "{{ value_json.power }}");
    
    // Binary sensors for component states
    publishBinarySensorDiscovery("Compressor", "compressor", "running",
                                  "chpc/state/system", "{{ value_json.compressor }}");
    publishBinarySensorDiscovery("Hot Pump", "hot_pump", "running",
                                  "chpc/state/system", "{{ value_json.hot_pump }}");
    publishBinarySensorDiscovery("Cold Pump", "cold_pump", "running",
                                  "chpc/state/system", "{{ value_json.cold_pump }}");
    publishBinarySensorDiscovery("Error State", "error", "problem",
                                  "chpc/state/system", "{{ value_json.error }}");
    
    // DHW switch
    publishSwitchDiscovery("DHW Enabled", "dhw_enabled",
                           "chpc/state/dhw", "chpc/set/dhw_enabled");
    
    // Summer mode switch
    publishSwitchDiscovery("Summer Mode", "summer_mode",
                           "chpc/state/season", "chpc/set/summer_mode");
    
#if ENABLE_EEV
    // EEV superheat target
    publishNumberDiscovery("EEV Superheat Target", "eev_target",
                           1.0, 15.0, 0.25, "°C",
                           "chpc/state/eev", "chpc/set/eev_target");
    
    // EEV position sensor
    publishSensorDiscovery("EEV Position", "eev_position", nullptr, "%",
                           "chpc/state/eev", "{{ value_json.position }}");
#endif
    
    Serial.println(F("[MQTT] Discovery published"));
}

void MQTTManager::publishClimateDiscovery() {
    StaticJsonDocument<1024> doc;
    
    doc["name"] = "Heat Pump";
    doc["unique_id"] = "chpc_climate";
    doc["object_id"] = "chpc_climate";
    
    // Add device info
    JsonObject device = doc.createNestedObject("device");
    device["identifiers"][0] = "chpc_heatpump";
    device["name"] = "CHPC Heat Pump";
    device["model"] = HW_VERSION;
    device["sw_version"] = FW_VERSION;
    device["manufacturer"] = "DIY";
    
    // Climate entity configuration
    doc["modes"][0] = "off";
    doc["modes"][1] = "heat";
    doc["modes"][2] = "cool";
    
    doc["min_temp"] = TempLimits::SETPOINT_MIN;
    doc["max_temp"] = TempLimits::SETPOINT_MAX;
    doc["temp_step"] = 0.5;
    doc["temperature_unit"] = "C";
    
    doc["mode_state_topic"] = "chpc/state/mode";
    doc["mode_command_topic"] = "chpc/set/mode";
    doc["temperature_state_topic"] = "chpc/state/target_temp";
    doc["temperature_command_topic"] = "chpc/set/target_temp";
    doc["current_temperature_topic"] = "chpc/state/current_temp";
    doc["availability_topic"] = NetworkConfig::MQTT_AVAILABILITY_TOPIC;
    
    String payload;
    serializeJson(doc, payload);
    
    String topic = String(NetworkConfig::MQTT_BASE_TOPIC) + "/climate/chpc/climate/config";
    publish(topic.c_str(), payload, true);
}

void MQTTManager::publishSensorDiscovery(const char* name, const char* id,
                                          const char* deviceClass, const char* unit,
                                          const char* stateTopic, const char* valueTemplate) {
    StaticJsonDocument<512> doc;
    
    doc["name"] = name;
    doc["unique_id"] = String("chpc_") + id;
    doc["object_id"] = String("chpc_") + id;
    doc["state_topic"] = stateTopic;
    doc["value_template"] = valueTemplate;
    
    if (deviceClass) doc["device_class"] = deviceClass;
    if (unit) doc["unit_of_measurement"] = unit;
    
    doc["availability_topic"] = NetworkConfig::MQTT_AVAILABILITY_TOPIC;
    
    // Device reference
    JsonObject device = doc.createNestedObject("device");
    device["identifiers"][0] = "chpc_heatpump";
    
    String payload;
    serializeJson(doc, payload);
    
    String topic = String(NetworkConfig::MQTT_BASE_TOPIC) + "/sensor/chpc/" + id + "/config";
    publish(topic.c_str(), payload, true);
}

void MQTTManager::publishBinarySensorDiscovery(const char* name, const char* id,
                                                const char* deviceClass,
                                                const char* stateTopic, const char* valueTemplate) {
    StaticJsonDocument<512> doc;
    
    doc["name"] = name;
    doc["unique_id"] = String("chpc_") + id;
    doc["object_id"] = String("chpc_") + id;
    doc["state_topic"] = stateTopic;
    doc["value_template"] = valueTemplate;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    
    if (deviceClass) doc["device_class"] = deviceClass;
    
    doc["availability_topic"] = NetworkConfig::MQTT_AVAILABILITY_TOPIC;
    
    JsonObject device = doc.createNestedObject("device");
    device["identifiers"][0] = "chpc_heatpump";
    
    String payload;
    serializeJson(doc, payload);
    
    String topic = String(NetworkConfig::MQTT_BASE_TOPIC) + "/binary_sensor/chpc/" + id + "/config";
    publish(topic.c_str(), payload, true);
}

void MQTTManager::publishSwitchDiscovery(const char* name, const char* id,
                                          const char* stateTopic, const char* commandTopic) {
    StaticJsonDocument<512> doc;
    
    doc["name"] = name;
    doc["unique_id"] = String("chpc_") + id;
    doc["object_id"] = String("chpc_") + id;
    doc["state_topic"] = stateTopic;
    doc["command_topic"] = commandTopic;
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    
    doc["availability_topic"] = NetworkConfig::MQTT_AVAILABILITY_TOPIC;
    
    JsonObject device = doc.createNestedObject("device");
    device["identifiers"][0] = "chpc_heatpump";
    
    String payload;
    serializeJson(doc, payload);
    
    String topic = String(NetworkConfig::MQTT_BASE_TOPIC) + "/switch/chpc/" + id + "/config";
    publish(topic.c_str(), payload, true);
}

void MQTTManager::publishNumberDiscovery(const char* name, const char* id,
                                          float min, float max, float step,
                                          const char* unit,
                                          const char* stateTopic, const char* commandTopic) {
    StaticJsonDocument<512> doc;
    
    doc["name"] = name;
    doc["unique_id"] = String("chpc_") + id;
    doc["object_id"] = String("chpc_") + id;
    doc["state_topic"] = stateTopic;
    doc["command_topic"] = commandTopic;
    doc["min"] = min;
    doc["max"] = max;
    doc["step"] = step;
    if (unit) doc["unit_of_measurement"] = unit;
    
    doc["availability_topic"] = NetworkConfig::MQTT_AVAILABILITY_TOPIC;
    
    JsonObject device = doc.createNestedObject("device");
    device["identifiers"][0] = "chpc_heatpump";
    
    String payload;
    serializeJson(doc, payload);
    
    String topic = String(NetworkConfig::MQTT_BASE_TOPIC) + "/number/chpc/" + id + "/config";
    publish(topic.c_str(), payload, true);
}

void MQTTManager::publishAllStates() {
    publishTemperatures();
    publishSystemState();
    
    // Publish individual state topics
    const HeatPumpStatus& status = heatPumpController.getStatus();
    
    // Mode
    String mode;
    switch (status.mode) {
        case SystemMode::HEATING:
        case SystemMode::DHW_HEATING:
            mode = "heat";
            break;
        case SystemMode::COOLING:
            mode = "cool";
            break;
        default:
            mode = "off";
            break;
    }
    publish("chpc/state/mode", mode, true);
    
    // Target temperature
    publish("chpc/state/target_temp", 
            String(heatPumpController.getTargetTemperature(), 1), true);
    
    // Current temperature
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_TTARGET)) {
        publish("chpc/state/current_temp",
                String(tempSensors.getTtarget(), 1), true);
    }
    
    // DHW state
    publish("chpc/state/dhw", settings.isDHWEnabled() ? "ON" : "OFF", true);
    
    // Season
    publish("chpc/state/season", 
            settings.getSeason() == Season::SUMMER ? "ON" : "OFF", true);
    
#if ENABLE_EEV
    // EEV state
    StaticJsonDocument<128> eevDoc;
    eevDoc["position"] = eevController.getPositionPercent();
    eevDoc["superheat"] = eevController.getSuperheat();
    eevDoc["target"] = eevController.getTargetSuperheat();
    String eevPayload;
    serializeJson(eevDoc, eevPayload);
    publish("chpc/state/eev", eevPayload, true);
#endif
}

void MQTTManager::publishTemperatures() {
    StaticJsonDocument<512> doc;
    
    for (uint8_t i = 0; i < SensorConfig::SENSOR_COUNT; i++) {
        if (tempSensors.isSensorEnabled(i)) {
            float temp = tempSensors.getTemperature(i);
            if (temp != TempLimits::SENSOR_ERROR) {
                doc[SensorConfig::SENSOR_NAMES[i]] = serialized(String(temp, 1));
            }
        }
    }
    
    String payload;
    serializeJson(doc, payload);
    publish("chpc/state/temperatures", payload, false);
}

void MQTTManager::publishSystemState() {
    const HeatPumpStatus& status = heatPumpController.getStatus();
    
    StaticJsonDocument<256> doc;
    doc["compressor"] = status.compressorRunning ? "ON" : "OFF";
    doc["hot_pump"] = status.hotPumpRunning ? "ON" : "OFF";
    doc["cold_pump"] = status.coldPumpRunning ? "ON" : "OFF";
    doc["power"] = serialized(String(status.currentPower, 0));
    doc["error"] = status.errorCode != ErrorCode::OK ? "ON" : "OFF";
    doc["error_code"] = static_cast<uint8_t>(status.errorCode);
    doc["mode"] = static_cast<uint8_t>(status.mode);
    doc["dhw_active"] = status.dhwHeatingActive ? "ON" : "OFF";
    
    String payload;
    serializeJson(doc, payload);
    publish("chpc/state/system", payload, false);
}

void MQTTManager::publishAvailability(bool available) {
    publish(NetworkConfig::MQTT_AVAILABILITY_TOPIC, 
            available ? "online" : "offline", true);
}

void MQTTManager::subscribe(const char* topic) {
    _client.subscribe(topic);
    Serial.printf("[MQTT] Subscribed: %s\n", topic);
}

void MQTTManager::publish(const char* topic, const String& payload, bool retain) {
    _client.publish(topic, payload.c_str(), retain);
}
