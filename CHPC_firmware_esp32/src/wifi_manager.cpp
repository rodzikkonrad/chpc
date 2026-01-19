/**
 * @file wifi_manager.cpp
 * @brief Implementation of WiFi Manager and Web Server
 */

#include "wifi_manager.h"
#include "settings.h"
#include "heat_pump_controller.h"
#include "temperature_sensors.h"
#include "eev_controller.h"
#include <ArduinoJson.h>

WiFiManager::WiFiManager() : _server(80) {}

void WiFiManager::begin() {
    Serial.println(F("[WiFi] Initializing..."));
    
    // Set hostname
    WiFi.setHostname(NetworkConfig::HOSTNAME);
    
    // Try to connect to configured network
    String ssid = settings.getWiFiSSID();
    String password = settings.getWiFiPassword();
    
    if (ssid.length() > 0 && ssid != NetworkConfig::DEFAULT_SSID) {
        Serial.printf("[WiFi] Connecting to: %s\n", ssid.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), password.c_str());
        
        _state = WiFiState::CONNECTING;
        _connectStartTime = millis();
    } else {
        Serial.println(F("[WiFi] No network configured, starting AP mode"));
        startAPMode();
    }
    
    // Setup web server (works in both STA and AP mode)
    setupWebServer();
}

void WiFiManager::update() {
    uint32_t now = millis();
    
    // Handle OTA updates
    ArduinoOTA.handle();
    
    switch (_state) {
        case WiFiState::CONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                _state = WiFiState::CONNECTED;
                Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            } else if (now - _connectStartTime > CONNECT_TIMEOUT_MS) {
                Serial.println(F("[WiFi] Connection timeout, starting AP mode"));
                startAPMode();
            }
            break;
            
        case WiFiState::CONNECTED:
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println(F("[WiFi] Connection lost!"));
                _state = WiFiState::DISCONNECTED;
                _lastConnectAttempt = now;
            }
            break;
            
        case WiFiState::DISCONNECTED:
            if (now - _lastConnectAttempt > RECONNECT_INTERVAL_MS) {
                reconnect();
            }
            break;
            
        case WiFiState::AP_MODE:
            // AP mode stays active until user configures network
            break;
            
        default:
            break;
    }
}

void WiFiManager::reconnect() {
    Serial.println(F("[WiFi] Reconnecting..."));
    
    String ssid = settings.getWiFiSSID();
    String password = settings.getWiFiPassword();
    
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str());
    
    _state = WiFiState::CONNECTING;
    _connectStartTime = millis();
    _lastConnectAttempt = millis();
}

void WiFiManager::startAPMode() {
    Serial.println(F("[WiFi] Starting AP mode..."));
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP("CHPC-Setup", "heatpump123");
    
    _state = WiFiState::AP_MODE;
    _apModeActive = true;
    
    Serial.printf("[WiFi] AP started, IP: %s\n", WiFi.softAPIP().toString().c_str());
}

void WiFiManager::stopAPMode() {
    if (_apModeActive) {
        WiFi.softAPdisconnect(true);
        _apModeActive = false;
    }
}

String WiFiManager::getIPAddress() const {
    if (_state == WiFiState::AP_MODE) {
        return WiFi.softAPIP().toString();
    } else if (_state == WiFiState::CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

void WiFiManager::setupWebServer() {
    // Serve static pages
    _server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", getHomePage());
    });
    
    _server.on("/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", getConfigPage());
    });
    
    _server.on("/sensors", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "text/html", getSensorsPage());
    });
    
    // API routes
    setupAPIRoutes();
    
    // OTA updates page (simple redirect to use ArduinoOTA)
    _server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = "<!DOCTYPE html><html><head><title>OTA Update</title></head>"
            "<body><h1>OTA Updates</h1>"
            "<p>Use Arduino IDE or PlatformIO with OTA upload.</p>"
            "<p>Device: ";
        html += NetworkConfig::HOSTNAME;
        html += "</p><p><a href='/'>Back to Home</a></p></body></html>";
        request->send(200, "text/html", html);
    });
    
    // Setup ArduinoOTA for IDE uploads
    ArduinoOTA.setHostname(NetworkConfig::HOSTNAME);
    ArduinoOTA.begin();
    
    _server.begin();
    Serial.println(F("[WiFi] Web server started"));
}

void WiFiManager::setupAPIRoutes() {
    // Get current status
    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", getStatusJSON());
    });
    
    // Get sensor readings
    _server.on("/api/sensors", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", getSensorsJSON());
    });
    
    // Set target temperature
    _server.on("/api/target", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("temp", true)) {
            float temp = request->getParam("temp", true)->value().toFloat();
            heatPumpController.setTargetTemperature(temp);
            request->send(200, "application/json", "{\"success\":true}");
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing temp parameter\"}");
        }
    });
    
    // Set DHW temperature
    _server.on("/api/dhw", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("temp", true)) {
            float temp = request->getParam("temp", true)->value().toFloat();
            heatPumpController.setDHWTargetTemperature(temp);
            request->send(200, "application/json", "{\"success\":true}");
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing temp parameter\"}");
        }
    });
    
    // Set mode
    _server.on("/api/mode", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("mode", true)) {
            String mode = request->getParam("mode", true)->value();
            if (mode == "heat") {
                heatPumpController.setCoolingMode(false);
            } else if (mode == "cool") {
                heatPumpController.setCoolingMode(true);
            } else if (mode == "off") {
                heatPumpController.emergencyStop();
            }
            request->send(200, "application/json", "{\"success\":true}");
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing mode parameter\"}");
        }
    });
    
    // Configure WiFi
    _server.on("/api/wifi", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
            String ssid = request->getParam("ssid", true)->value();
            String password = request->getParam("password", true)->value();
            
            settings.setWiFiSSID(ssid);
            settings.setWiFiPassword(password);
            settings.save();
            
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Restarting...\"}");
            
            delay(1000);
            ESP.restart();
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing ssid or password\"}");
        }
    });
    
    // Configure MQTT
    _server.on("/api/mqtt", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool changed = false;
        
        if (request->hasParam("server", true)) {
            settings.setMQTTServer(request->getParam("server", true)->value());
            changed = true;
        }
        if (request->hasParam("port", true)) {
            settings.setMQTTPort(request->getParam("port", true)->value().toInt());
            changed = true;
        }
        if (request->hasParam("user", true)) {
            settings.setMQTTUser(request->getParam("user", true)->value());
            changed = true;
        }
        if (request->hasParam("password", true)) {
            settings.setMQTTPassword(request->getParam("password", true)->value());
            changed = true;
        }
        
        if (changed) {
            settings.save();
            request->send(200, "application/json", "{\"success\":true}");
        } else {
            request->send(400, "application/json", "{\"error\":\"No parameters provided\"}");
        }
    });
    
    // System commands
    _server.on("/api/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        heatPumpController.reset();
        request->send(200, "application/json", "{\"success\":true}");
    });
    
    _server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting...\"}");
        delay(500);
        ESP.restart();
    });
    
    _server.on("/api/factory-reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        settings.factoryReset();
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting...\"}");
        delay(500);
        ESP.restart();
    });
}

String WiFiManager::getStatusJSON() {
    StaticJsonDocument<512> doc;
    
    const HeatPumpStatus& status = heatPumpController.getStatus();
    
    doc["mode"] = static_cast<uint8_t>(status.mode);
    doc["error"] = static_cast<uint8_t>(status.errorCode);
    doc["compressor"] = status.compressorRunning;
    doc["hot_pump"] = status.hotPumpRunning;
    doc["cold_pump"] = status.coldPumpRunning;
    doc["power"] = status.currentPower;
    doc["target"] = heatPumpController.getTargetTemperature();
    doc["dhw_target"] = heatPumpController.getDHWTargetTemperature();
    doc["dhw_active"] = status.dhwHeatingActive;
    doc["cooling"] = heatPumpController.isCoolingMode();
    doc["summer"] = heatPumpController.isSummerMode();
    
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_TTARGET)) {
        doc["current_temp"] = tempSensors.getTtarget();
    }
    
#if ENABLE_EEV
    doc["eev_position"] = eevController.getPositionPercent();
    doc["eev_superheat"] = eevController.getSuperheat();
#endif
    
    doc["wifi_rssi"] = getRSSI();
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    
    String output;
    serializeJson(doc, output);
    return output;
}

String WiFiManager::getSensorsJSON() {
    StaticJsonDocument<512> doc;
    JsonArray sensors = doc.createNestedArray("sensors");
    
    for (uint8_t i = 0; i < SensorConfig::SENSOR_COUNT; i++) {
        if (tempSensors.isSensorEnabled(i)) {
            JsonObject sensor = sensors.createNestedObject();
            sensor["id"] = i;
            sensor["name"] = SensorConfig::SENSOR_NAMES[i];
            sensor["desc"] = SensorConfig::SENSOR_DESCRIPTIONS[i];
            sensor["temp"] = tempSensors.getTemperature(i);
            sensor["valid"] = tempSensors.isSensorValid(i);
        }
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

String WiFiManager::getHomePage() {
    String html = R"html(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>CHPC Heat Pump</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; 
               background: #1a1a2e; color: #eee; padding: 20px; }
        .container { max-width: 800px; margin: 0 auto; }
        h1 { color: #00d9ff; margin-bottom: 20px; }
        .card { background: #16213e; border-radius: 12px; padding: 20px; margin-bottom: 20px; }
        .card h2 { color: #00d9ff; font-size: 1.2em; margin-bottom: 15px; }
        .status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(120px, 1fr)); gap: 10px; }
        .status-item { background: #0f3460; padding: 15px; border-radius: 8px; text-align: center; }
        .status-item .value { font-size: 1.8em; font-weight: bold; color: #00d9ff; }
        .status-item .label { font-size: 0.8em; color: #888; margin-top: 5px; }
        .status-on { background: #1e5631; }
        .status-off { background: #5c1e1e; }
        .controls { display: flex; gap: 10px; flex-wrap: wrap; }
        button { background: #00d9ff; color: #000; border: none; padding: 12px 24px; 
                 border-radius: 8px; cursor: pointer; font-size: 1em; }
        button:hover { background: #00b8d4; }
        button.secondary { background: #0f3460; color: #fff; }
        input[type="range"] { width: 100%; margin: 10px 0; }
        .nav { display: flex; gap: 10px; margin-bottom: 20px; }
        .nav a { color: #00d9ff; text-decoration: none; padding: 10px 15px; 
                 background: #16213e; border-radius: 8px; }
        .nav a:hover { background: #0f3460; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🌡️ CHPC Heat Pump</h1>
        
        <div class="nav">
            <a href="/">Dashboard</a>
            <a href="/config">Settings</a>
            <a href="/sensors">Sensors</a>
            <a href="/update">OTA Update</a>
        </div>
        
        <div class="card">
            <h2>Current Status</h2>
            <div class="status-grid" id="status">
                <div class="status-item">
                    <div class="value" id="current-temp">--</div>
                    <div class="label">Current °C</div>
                </div>
                <div class="status-item">
                    <div class="value" id="target-temp">--</div>
                    <div class="label">Target °C</div>
                </div>
                <div class="status-item">
                    <div class="value" id="power">--</div>
                    <div class="label">Power W</div>
                </div>
                <div class="status-item" id="compressor-status">
                    <div class="value" id="compressor">--</div>
                    <div class="label">Compressor</div>
                </div>
            </div>
        </div>
        
        <div class="card">
            <h2>Temperature Control</h2>
            <div>
                <label>Target Temperature: <span id="target-display">21.5</span>°C</label>
                <input type="range" id="target-slider" min="15" max="55" step="0.5" value="21.5">
            </div>
            <div class="controls" style="margin-top: 15px;">
                <button onclick="setMode('heat')">🔥 Heat</button>
                <button onclick="setMode('cool')">❄️ Cool</button>
                <button onclick="setMode('off')" class="secondary">⏹️ Off</button>
            </div>
        </div>
        
        <div class="card">
            <h2>System</h2>
            <div class="controls">
                <button onclick="resetSystem()" class="secondary">Reset</button>
                <button onclick="rebootSystem()" class="secondary">Reboot</button>
            </div>
        </div>
    </div>
    
    <script>
        function updateStatus() {
            fetch('/api/status')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('current-temp').textContent = 
                        data.current_temp ? data.current_temp.toFixed(1) : '--';
                    document.getElementById('target-temp').textContent = data.target.toFixed(1);
                    document.getElementById('power').textContent = Math.round(data.power);
                    document.getElementById('compressor').textContent = data.compressor ? 'ON' : 'OFF';
                    document.getElementById('compressor-status').className = 
                        'status-item ' + (data.compressor ? 'status-on' : 'status-off');
                });
        }
        
        document.getElementById('target-slider').addEventListener('input', function() {
            document.getElementById('target-display').textContent = this.value;
        });
        
        document.getElementById('target-slider').addEventListener('change', function() {
            fetch('/api/target', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: 'temp=' + this.value
            });
        });
        
        function setMode(mode) {
            fetch('/api/mode', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: 'mode=' + mode
            }).then(() => updateStatus());
        }
        
        function resetSystem() {
            if (confirm('Reset heat pump controller?')) {
                fetch('/api/reset', {method: 'POST'}).then(() => updateStatus());
            }
        }
        
        function rebootSystem() {
            if (confirm('Reboot the device?')) {
                fetch('/api/reboot', {method: 'POST'});
            }
        }
        
        updateStatus();
        setInterval(updateStatus, 5000);
    </script>
</body>
</html>
)html";
    return html;
}

String WiFiManager::getConfigPage() {
    String html = R"html(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>CHPC Configuration</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, sans-serif; 
               background: #1a1a2e; color: #eee; padding: 20px; }
        .container { max-width: 600px; margin: 0 auto; }
        h1 { color: #00d9ff; margin-bottom: 20px; }
        .card { background: #16213e; border-radius: 12px; padding: 20px; margin-bottom: 20px; }
        .card h2 { color: #00d9ff; font-size: 1.2em; margin-bottom: 15px; }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 5px; color: #888; }
        input[type="text"], input[type="password"], input[type="number"] {
            width: 100%; padding: 10px; border: 1px solid #0f3460; 
            background: #0f3460; color: #fff; border-radius: 6px;
        }
        button { background: #00d9ff; color: #000; border: none; padding: 12px 24px; 
                 border-radius: 8px; cursor: pointer; font-size: 1em; width: 100%; }
        button:hover { background: #00b8d4; }
        .nav { display: flex; gap: 10px; margin-bottom: 20px; }
        .nav a { color: #00d9ff; text-decoration: none; padding: 10px 15px; 
                 background: #16213e; border-radius: 8px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>⚙️ Configuration</h1>
        
        <div class="nav">
            <a href="/">Dashboard</a>
            <a href="/config">Settings</a>
            <a href="/sensors">Sensors</a>
        </div>
        
        <div class="card">
            <h2>WiFi Settings</h2>
            <form id="wifi-form">
                <div class="form-group">
                    <label>SSID</label>
                    <input type="text" name="ssid" placeholder="Network name">
                </div>
                <div class="form-group">
                    <label>Password</label>
                    <input type="password" name="password" placeholder="Password">
                </div>
                <button type="submit">Save & Restart</button>
            </form>
        </div>
        
        <div class="card">
            <h2>MQTT Settings (Home Assistant)</h2>
            <form id="mqtt-form">
                <div class="form-group">
                    <label>MQTT Server</label>
                    <input type="text" name="server" placeholder="homeassistant.local">
                </div>
                <div class="form-group">
                    <label>Port</label>
                    <input type="number" name="port" value="1883">
                </div>
                <div class="form-group">
                    <label>Username</label>
                    <input type="text" name="user" placeholder="mqtt_user">
                </div>
                <div class="form-group">
                    <label>Password</label>
                    <input type="password" name="password" placeholder="mqtt_password">
                </div>
                <button type="submit">Save MQTT Settings</button>
            </form>
        </div>
    </div>
    
    <script>
        document.getElementById('wifi-form').addEventListener('submit', function(e) {
            e.preventDefault();
            const data = new FormData(this);
            fetch('/api/wifi', {
                method: 'POST',
                body: new URLSearchParams(data)
            }).then(r => r.json()).then(d => {
                alert(d.message || 'Saved!');
            });
        });
        
        document.getElementById('mqtt-form').addEventListener('submit', function(e) {
            e.preventDefault();
            const data = new FormData(this);
            fetch('/api/mqtt', {
                method: 'POST',
                body: new URLSearchParams(data)
            }).then(r => r.json()).then(d => {
                alert(d.success ? 'Saved!' : d.error);
            });
        });
    </script>
</body>
</html>
)html";
    return html;
}

String WiFiManager::getSensorsPage() {
    String html = R"html(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>CHPC Sensors</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, sans-serif; 
               background: #1a1a2e; color: #eee; padding: 20px; }
        .container { max-width: 800px; margin: 0 auto; }
        h1 { color: #00d9ff; margin-bottom: 20px; }
        .card { background: #16213e; border-radius: 12px; padding: 20px; margin-bottom: 20px; }
        .sensor-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; }
        .sensor-item { background: #0f3460; padding: 15px; border-radius: 8px; }
        .sensor-item .name { font-weight: bold; color: #00d9ff; }
        .sensor-item .temp { font-size: 2em; margin: 10px 0; }
        .sensor-item .desc { font-size: 0.8em; color: #888; }
        .sensor-item.invalid { opacity: 0.5; }
        .nav { display: flex; gap: 10px; margin-bottom: 20px; }
        .nav a { color: #00d9ff; text-decoration: none; padding: 10px 15px; 
                 background: #16213e; border-radius: 8px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🌡️ Temperature Sensors</h1>
        
        <div class="nav">
            <a href="/">Dashboard</a>
            <a href="/config">Settings</a>
            <a href="/sensors">Sensors</a>
        </div>
        
        <div class="card">
            <div class="sensor-grid" id="sensors">
                <div class="sensor-item">Loading...</div>
            </div>
        </div>
    </div>
    
    <script>
        function updateSensors() {
            fetch('/api/sensors')
                .then(r => r.json())
                .then(data => {
                    const container = document.getElementById('sensors');
                    container.innerHTML = data.sensors.map(s => `
                        <div class="sensor-item ${s.valid ? '' : 'invalid'}">
                            <div class="name">${s.name}</div>
                            <div class="temp">${s.valid ? s.temp.toFixed(1) + '°C' : 'Error'}</div>
                            <div class="desc">${s.desc}</div>
                        </div>
                    `).join('');
                });
        }
        
        updateSensors();
        setInterval(updateSensors, 5000);
    </script>
</body>
</html>
)html";
    return html;
}
