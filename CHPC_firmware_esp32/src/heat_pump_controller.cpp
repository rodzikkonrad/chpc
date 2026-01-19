/**
 * @file heat_pump_controller.cpp
 * @brief Implementation of HeatPumpController
 */

#include "heat_pump_controller.h"

void HeatPumpController::begin() {
    Serial.println(F("[HP] Initializing Heat Pump Controller..."));
    
    // Configure current sensor ADC
    analogReadResolution(12);  // 12-bit resolution on ESP32
    analogSetAttenuation(ADC_11db);  // Full range 0-3.3V
    pinMode(Pins::CURRENT_SENSOR, INPUT);
    
    _status.mode = SystemMode::OFF;
    _status.errorCode = ErrorCode::OK;
    _startupCompleteTime = millis() + Timing::POWERON_PAUSE_MS;
    _startupComplete = false;
    
    _initialized = true;
    Serial.println(F("[HP] Initialization complete"));
    Serial.printf("[HP] Startup delay: %lu seconds\n", Timing::POWERON_PAUSE_MS / 1000);
}

void HeatPumpController::update() {
    if (!_initialized) return;
    
    uint32_t now = millis();
    
    // Check startup delay
    if (!_startupComplete) {
        if (now < _startupCompleteTime) {
            uint32_t remaining = (_startupCompleteTime - now) / 1000;
            static uint32_t lastPrint = 0;
            if (now - lastPrint > 5000) {
                Serial.printf("[HP] Startup wait: %lu seconds remaining\n", remaining);
                lastPrint = now;
            }
            return;
        }
        _startupComplete = true;
        Serial.println(F("[HP] Startup complete, entering normal operation"));
    }
    
    // Update power reading
    updatePowerReading();
    
    // Check for sensor errors first
    if (checkSensorErrors()) {
        return;  // Don't proceed with control if sensors are bad
    }
    
    // Run protection checks
    runProtectionChecks();
    
    // If in error state, don't run normal control
    if (_status.errorCode != ErrorCode::OK) {
        return;
    }
    
    // Handle sump heater (independent of other logic)
    handleSumpHeater();
    
    // Handle DHW control
    handleDHWControl();
    
    // Handle 4-way valve for cooling mode
    handle4WayValve();
    
    // Main control logic
    checkStartConditions();
    checkStopConditions();
    
    // Control pumps and compressor with proper sequencing
    controlColdPump();
    controlCompressor();
    controlHotPump();
    
    // Update timing info
    if (_status.compressorRunning) {
        _status.compressorOnTime = now - _compressorStartTime;
    } else {
        _status.compressorOffTime = now - _compressorStopTime;
    }
    
    // Apply relay states
    relayController.update();
    
#if ENABLE_EEV
    // Update EEV control
    if (tempSensors.isSensorEnabled(SensorConfig::SENSOR_TBE) && 
        tempSensors.isSensorEnabled(SensorConfig::SENSOR_TAE)) {
        eevController.updateSuperheatControl(
            tempSensors.getTbe(),
            tempSensors.getTae(),
            _status.compressorRunning
        );
    }
#endif
    
    _lastUpdateTime = now;
}

void HeatPumpController::updatePowerReading() {
    // Calculate RMS current from CT sensor
    float irms = calculateRMSCurrent();
    _status.currentAmps = irms;
    _status.currentPower = irms * 230.0f;  // Assuming 230V
}

float HeatPumpController::calculateRMSCurrent() {
    // ESP32 ADC reading for CT sensor
    // Similar algorithm to original but using ESP32 ADC
    
    float sumSq = 0.0f;
    float offset = 2048.0f;  // Mid-point for 12-bit ADC (0-4095)
    
    for (uint16_t i = 0; i < ADC_SAMPLES; i++) {
        int16_t sample = analogRead(Pins::CURRENT_SENSOR);
        float filtered = sample - offset;
        offset += (filtered / 1024.0f);  // Low-pass filter for offset
        sumSq += filtered * filtered;
    }
    
    float rms = sqrt(sumSq / ADC_SAMPLES);
    
    // Apply calibration factor
    // Calibration depends on CT ratio and burden resistor
    float calibration = PowerConfig::CURRENT_CALIBRATION;
    float voltage = 3.3f;  // ESP32 ADC reference
    float irms = calibration * (voltage / 4096.0f) * rms;
    
    return irms;
}

bool HeatPumpController::checkSensorErrors() {
    bool hasError = false;
    
    // Check each enabled sensor for errors
    for (uint8_t i = 0; i < SensorConfig::SENSOR_COUNT; i++) {
        if (tempSensors.isSensorEnabled(i) && !tempSensors.isSensorValid(i)) {
            hasError = true;
            break;
        }
    }
    
    if (hasError && _status.errorCode == ErrorCode::OK) {
        setError(ErrorCode::SENSOR_ERROR);
    } else if (!hasError && _status.errorCode == ErrorCode::SENSOR_ERROR) {
        // Auto-clear sensor error when sensors recover
        _status.errorCode = ErrorCode::OK;
        _status.mode = SystemMode::OFF;
        Serial.println(F("[HP] Sensor error cleared"));
    }
    
    return hasError;
}

void HeatPumpController::runProtectionChecks() {
    uint32_t now = millis();
    
    if (!_status.compressorRunning) return;
    
    // Check discharge temperature (before condenser)
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_TBC)) {
        if (tempSensors.getTbc() > TempLimits::DISCHARGE_MAX) {
            Serial.println(F("[HP] PROTECTION: Discharge temperature too high!"));
            setError(ErrorCode::HEATPUMP_ERROR);
            return;
        }
    }
    
    // Check suction temperature (after evaporator)
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_TAE)) {
        if (tempSensors.getTae() < TempLimits::SUCTION_MIN) {
            Serial.println(F("[HP] PROTECTION: Suction temperature too low!"));
            setError(ErrorCode::HEATPUMP_ERROR);
            return;
        }
    }
    
    // Check sump/compressor temperature
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_TSUMP)) {
        if (tempSensors.getTsump() > TempLimits::SUMP_MAX) {
            Serial.println(F("[HP] PROTECTION: Compressor overheating!"));
            setError(ErrorCode::HEATPUMP_ERROR);
            return;
        }
    }
    
    // Check hot outlet temperature
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_THO)) {
        if (tempSensors.getTho() > TempLimits::HOT_OUT_MAX) {
            Serial.println(F("[HP] PROTECTION: Hot outlet temperature too high!"));
            setError(ErrorCode::HEATPUMP_ERROR);
            return;
        }
    }
    
    // Check cold loop for anti-freeze
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_TCI)) {
        if (tempSensors.getTci() < TempLimits::COLD_LOOP_MIN) {
            Serial.println(F("[HP] PROTECTION: Cold inlet too low (freeze risk)!"));
            setError(ErrorCode::HEATPUMP_ERROR);
            return;
        }
    }
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_TCO)) {
        if (tempSensors.getTco() < TempLimits::COLD_LOOP_MIN) {
            Serial.println(F("[HP] PROTECTION: Cold outlet too low (freeze risk)!"));
            setError(ErrorCode::HEATPUMP_ERROR);
            return;
        }
    }
    
    // Check power consumption
    if (_status.currentPower > settings.getMaxWatts()) {
        // Allow higher power briefly at startup
        if (now - _compressorStartTime > Timing::POWERON_HIGH_TIME_MS) {
            Serial.printf("[HP] PROTECTION: Power overload! %.0fW > %.0fW\n", 
                         _status.currentPower, settings.getMaxWatts());
            setError(ErrorCode::OVERLOAD_ERROR);
            return;
        }
    }
    
    // After 5 minutes, check for proper operation
    if (now - _compressorStartTime > 300000) {
        // Check sump temperature is reasonable
        if (tempSensors.isSensorValid(SensorConfig::SENSOR_TSUMP)) {
            if (tempSensors.getTsump() < TempLimits::WORKING_SUMP_MIN) {
                Serial.println(F("[HP] PROTECTION: Compressor not heating (fault)"));
                setError(ErrorCode::HEATPUMP_ERROR);
                return;
            }
        }
        
        // Check minimum power consumption (compressor should be drawing power)
        float minWatts = settings.getMaxWatts() * PowerConfig::MIN_WORKING_WATTS_RATIO;
        if (_status.currentPower < minWatts) {
            Serial.printf("[HP] PROTECTION: Power too low! %.0fW < %.0fW\n", 
                         _status.currentPower, minWatts);
            setError(ErrorCode::LACK_OF_START);
            return;
        }
    }
}

void HeatPumpController::handleSumpHeater() {
    if (!tempSensors.isSensorValid(SensorConfig::SENSOR_TSUMP)) return;
    
    float sumpTemp = tempSensors.getTsump();
    
    if (sumpTemp < TempLimits::SUMP_HEATER_THRESHOLD && !_status.sumpHeaterOn) {
        _status.sumpHeaterOn = true;
        relayController.setSumpHeater(true);
        Serial.println(F("[HP] Sump heater ON"));
    } else if (sumpTemp >= TempLimits::SUMP_HEATER_THRESHOLD && _status.sumpHeaterOn) {
        _status.sumpHeaterOn = false;
        relayController.setSumpHeater(false);
        Serial.println(F("[HP] Sump heater OFF"));
    }
}

void HeatPumpController::handleDHWControl() {
    if (!settings.isDHWEnabled()) return;
    if (!tempSensors.isSensorValid(SensorConfig::SENSOR_TCWU)) return;
    
    uint32_t now = millis();
    float dhwTemp = tempSensors.getTcwu();
    float dhwTarget = settings.getDHWSetpoint();
    float dhwHyst = settings.getDHWHysteresis();
    
    // Check if we need to start DHW heating
    if (!_status.dhwHeatingActive) {
        bool shouldStart = false;
        
        // Emergency: DHW tank too cold
        if (dhwTemp < 32.0f) {
            Serial.println(F("[HP] DHW emergency: tank temp < 32°C"));
            shouldStart = true;
        }
        // Normal: scheduled heating
        else if ((now - _status.lastDHWHeating > Timing::DHW_INTERVAL_MS || _status.lastDHWHeating == 0) &&
                 dhwTemp < dhwTarget - dhwHyst) {
            Serial.println(F("[HP] DHW scheduled heating start"));
            shouldStart = true;
        }
        
        if (shouldStart) {
            _status.dhwHeatingActive = true;
            _status.dhwValveActive = true;
            _dhwStartTime = now;
            relayController.setDHWValve(true);
            _status.mode = SystemMode::DHW_HEATING;
            Serial.printf("[HP] DHW heating started, target: %.1f°C, current: %.1f°C\n",
                         dhwTarget, dhwTemp);
        }
    }
    
    // Check if we need to stop DHW heating
    if (_status.dhwHeatingActive) {
        bool shouldStop = false;
        
        // DHW disabled by user
        if (!settings.isDHWEnabled()) {
            Serial.println(F("[HP] DHW stopped: disabled by user"));
            shouldStop = true;
        }
        // Max heating time exceeded
        else if (now - _dhwStartTime > Timing::DHW_MAX_HEATING_MS) {
            Serial.println(F("[HP] DHW stopped: max time exceeded"));
            shouldStop = true;
        }
        // Target reached
        else if (dhwTemp >= dhwTarget + dhwHyst) {
            Serial.println(F("[HP] DHW stopped: target reached"));
            shouldStop = true;
        }
        
        if (shouldStop) {
            _status.dhwHeatingActive = false;
            _status.dhwValveActive = false;
            _status.lastDHWHeating = now;
            relayController.setDHWValve(false);
            
            if (_status.mode == SystemMode::DHW_HEATING) {
                _status.mode = SystemMode::OFF;
            }
        }
    }
}

void HeatPumpController::handle4WayValve() {
    if (!settings.is4WayValveInstalled()) return;
    
    bool shouldBeCooling = settings.isCoolingModeEnabled();
    
    // Don't switch while DHW heating (need heat, not cooling)
    if (_status.dhwHeatingActive) {
        shouldBeCooling = false;
    }
    
    if (shouldBeCooling != _status.valve4wayActive) {
        // Need to switch - stop compressor first
        if (_status.compressorRunning) {
            Serial.println(F("[HP] Stopping compressor for 4-way valve switch"));
            stopCompressor();
            // Schedule valve switch and restart
            _delayedCompressorStart = millis() + 180000;  // 3 minute delay
            _pendingCompressorStart = true;
        }
        
        _status.valve4wayActive = shouldBeCooling;
        relayController.set4WayValve(shouldBeCooling);
        
        _status.mode = shouldBeCooling ? SystemMode::COOLING : SystemMode::HEATING;
        Serial.printf("[HP] 4-way valve switched to %s mode\n", 
                     shouldBeCooling ? "COOLING" : "HEATING");
    }
}

void HeatPumpController::checkStartConditions() {
    if (_status.compressorRunning || _status.coldPumpRunning) return;
    if (_status.errorCode != ErrorCode::OK) return;
    
    uint32_t now = millis();
    
    // Check minimum off time
    if (_compressorStopTime > 0 && 
        now - _compressorStopTime < Timing::MIN_CYCLE_OFF_MS) {
        return;
    }
    
    // Check pending start delay (e.g., after error recovery or valve switch)
    if (_pendingCompressorStart && now < _delayedCompressorStart) {
        return;
    }
    _pendingCompressorStart = false;
    
    // Check sump temperature minimum
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_TSUMP)) {
        if (tempSensors.getTsump() < TempLimits::SUMP_MIN) {
            return;  // Too cold to start
        }
        if (tempSensors.getTsump() > TempLimits::SUMP_MAX) {
            return;  // Too hot
        }
    }
    
    // Determine if heating/cooling is needed
    bool needsOperation = false;
    
    // Check if in summer mode (DHW only)
    if (settings.getSeason() == Season::SUMMER) {
        needsOperation = _status.dhwHeatingActive;
    } else {
        // Winter mode - check CH or DHW
        if (_status.dhwHeatingActive) {
            needsOperation = true;
        } else if (tempSensors.isSensorValid(SensorConfig::SENSOR_TTARGET)) {
            float currentTemp = tempSensors.getTtarget();
            float hysteresis = settings.getCHHysteresis();
            
            if (settings.isCoolingModeEnabled()) {
                // Cooling mode - start if too hot
                float target = settings.getCoolingSetpoint();
                needsOperation = currentTemp > target;
            } else {
                // Heating mode - start if too cold
                float target = settings.getHeatingSetpoint();
                needsOperation = currentTemp < target - hysteresis;
                
                // Also check buffer sensor if enabled
                if (settings.isBufferEnabled() && 
                    tempSensors.isSensorValid(SensorConfig::SENSOR_TS2)) {
                    float bufferTemp = tempSensors.getTs2();
                    needsOperation = needsOperation || (bufferTemp < target - hysteresis);
                }
            }
        }
    }
    
    if (needsOperation) {
        // Start cold pump first
        startColdPump();
        _status.mode = _status.dhwHeatingActive ? SystemMode::DHW_HEATING :
                       (settings.isCoolingModeEnabled() ? SystemMode::COOLING : SystemMode::HEATING);
    }
}

void HeatPumpController::checkStopConditions() {
    if (!_status.compressorRunning) return;
    
    uint32_t now = millis();
    
    // Check minimum on time
    if (now - _compressorStartTime < Timing::MIN_CYCLE_ON_MS) {
        return;
    }
    
    bool shouldStop = false;
    
    // Check if target is reached
    if (tempSensors.isSensorValid(SensorConfig::SENSOR_TTARGET)) {
        float currentTemp = tempSensors.getTtarget();
        
        if (_status.dhwHeatingActive) {
            // DHW stop is handled in handleDHWControl()
        } else if (settings.isCoolingModeEnabled()) {
            // Cooling mode - stop when cool enough
            shouldStop = currentTemp < settings.getCoolingSetpoint();
        } else {
            // Heating mode - stop when warm enough
            float target = settings.getHeatingSetpoint();
            
            if (settings.isBufferEnabled() && 
                tempSensors.isSensorValid(SensorConfig::SENSOR_TS2)) {
                // Use buffer bottom sensor for stop
                shouldStop = tempSensors.getTs2() > target;
            } else {
                shouldStop = currentTemp > target;
            }
        }
    }
    
    if (shouldStop && !_status.dhwHeatingActive) {
        Serial.println(F("[HP] Target reached, stopping"));
        stopCompressor();
    }
}

void HeatPumpController::startColdPump() {
    if (_status.coldPumpRunning) return;
    
    Serial.println(F("[HP] Starting cold pump"));
    _status.coldPumpRunning = true;
    relayController.setColdPump(true);
    _coldPumpStartTime = millis();
    
    // Schedule compressor start after delay
    _delayedCompressorStart = millis() + Timing::COMPRESSOR_DELAY_MS;
    _pendingCompressorStart = true;
    
    Serial.printf("[HP] Compressor will start in %lu seconds\n", 
                 Timing::COMPRESSOR_DELAY_MS / 1000);
}

void HeatPumpController::stopCompressor() {
    if (!_status.compressorRunning) return;
    
    Serial.println(F("[HP] Stopping compressor"));
    _status.compressorRunning = true;
    relayController.setCompressor(false);
    _compressorStopTime = millis();
    
#if ENABLE_EEV
    eevController.onCompressorStop();
#endif
    
    // Schedule cold pump stop after delay
    _delayedColdPumpStop = millis() + Timing::COLD_PUMP_DELAY_MS;
    _pendingColdPumpStop = true;
    
    Serial.printf("[HP] Cold pump will stop in %lu seconds\n",
                 Timing::COLD_PUMP_DELAY_MS / 1000);
}

void HeatPumpController::controlColdPump() {
    uint32_t now = millis();
    
    // Check for pending stop
    if (_pendingColdPumpStop && now >= _delayedColdPumpStop) {
        if (!_status.compressorRunning && _status.coldPumpRunning) {
            Serial.println(F("[HP] Stopping cold pump"));
            _status.coldPumpRunning = false;
            relayController.setColdPump(false);
            _coldPumpStopTime = now;
        }
        _pendingColdPumpStop = false;
    }
}

void HeatPumpController::controlCompressor() {
    uint32_t now = millis();
    
    // Check for pending start
    if (_pendingCompressorStart && now >= _delayedCompressorStart) {
        if (_status.coldPumpRunning && !_status.compressorRunning &&
            _delayedCompressorStart > _coldPumpStopTime) {
            Serial.println(F("[HP] Starting compressor"));
            _status.compressorRunning = true;
            relayController.setCompressor(true);
            _compressorStartTime = now;
            
#if ENABLE_EEV
            eevController.onCompressorStart();
#endif
        }
        _pendingCompressorStart = false;
    }
}

void HeatPumpController::controlHotPump() {
    uint32_t now = millis();
    
    // Hot pump runs when compressor is on
    if (_status.compressorRunning && !_status.hotPumpRunning) {
        Serial.println(F("[HP] Starting hot pump"));
        _status.hotPumpRunning = true;
        relayController.setHotPump(true);
    }
    
    // Hot pump stops after deferred delay when compressor stops
    if (!_status.compressorRunning && _status.hotPumpRunning) {
        if (now - _compressorStopTime > Timing::HOT_PUMP_DEFERRED_STOP_MS) {
            // Also check if hot side still has useful heat
            bool stillHot = false;
            float targetTemp = tempSensors.getTtarget();
            
            if (tempSensors.isSensorValid(SensorConfig::SENSOR_THO)) {
                stillHot = tempSensors.getTho() > targetTemp + TempLimits::SETPOINT_MIN;
            }
            if (tempSensors.isSensorValid(SensorConfig::SENSOR_THI)) {
                stillHot = stillHot || 
                          (tempSensors.getThi() > targetTemp + TempLimits::SETPOINT_MIN);
            }
            
            if (!stillHot) {
                Serial.println(F("[HP] Stopping hot pump"));
                _status.hotPumpRunning = false;
                relayController.setHotPump(false);
            }
        }
    }
    
    // Also start hot pump if there's heat available (power recovery scenario)
    if (!_status.hotPumpRunning && !_status.compressorRunning) {
        if (tempSensors.isSensorValid(SensorConfig::SENSOR_THO) &&
            tempSensors.isSensorValid(SensorConfig::SENSOR_TTARGET)) {
            float targetTemp = tempSensors.getTtarget();
            if (tempSensors.getTho() > targetTemp + TempLimits::SETPOINT_MIN) {
                Serial.println(F("[HP] Starting hot pump (residual heat)"));
                _status.hotPumpRunning = true;
                relayController.setHotPump(true);
            }
        }
    }
}

void HeatPumpController::setError(ErrorCode code) {
    _status.errorCode = code;
    _status.mode = SystemMode::ERROR;
    
    Serial.printf("[HP] ERROR: %d\n", static_cast<uint8_t>(code));
    
    // Stop compressor immediately
    if (_status.compressorRunning) {
        _status.compressorRunning = false;
        relayController.setCompressor(false);
        _compressorStopTime = millis();
        
        // Schedule cold pump stop
        _delayedColdPumpStop = millis() + Timing::COLD_PUMP_DELAY_MS;
        _pendingColdPumpStop = true;
        
#if ENABLE_EEV
        eevController.onCompressorStop();
#endif
    }
    
    relayController.update();
}

void HeatPumpController::clearError() {
    Serial.println(F("[HP] Clearing error"));
    _status.errorCode = ErrorCode::OK;
    _status.mode = SystemMode::OFF;
    _status.dhwHeatingActive = false;
    _status.dhwValveActive = false;
    relayController.setDHWValve(false);
}

void HeatPumpController::emergencyStop() {
    Serial.println(F("[HP] EMERGENCY STOP!"));
    relayController.allOff();
    _status.compressorRunning = false;
    _status.coldPumpRunning = false;
    _status.hotPumpRunning = false;
    _status.mode = SystemMode::OFF;
    _compressorStopTime = millis();
    
#if ENABLE_EEV
    eevController.emergencyClose();
#endif
}

void HeatPumpController::reset() {
    Serial.println(F("[HP] System reset"));
    clearError();
    _status = HeatPumpStatus();
    _compressorStopTime = millis();
    _pendingCompressorStart = false;
    _pendingColdPumpStop = false;
    relayController.allOff();
}

void HeatPumpController::setTargetTemperature(float temp) {
    if (settings.isCoolingModeEnabled()) {
        settings.setCoolingSetpoint(temp);
    } else {
        settings.setHeatingSetpoint(temp);
    }
}

float HeatPumpController::getTargetTemperature() const {
    if (settings.isCoolingModeEnabled()) {
        return settings.getCoolingSetpoint();
    } else {
        return settings.getHeatingSetpoint();
    }
}

void HeatPumpController::setDHWTargetTemperature(float temp) {
    settings.setDHWSetpoint(temp);
}

float HeatPumpController::getDHWTargetTemperature() const {
    return settings.getDHWSetpoint();
}

void HeatPumpController::setCoolingMode(bool enabled) {
    settings.setCoolingModeEnabled(enabled);
}

bool HeatPumpController::isCoolingMode() const {
    return settings.isCoolingModeEnabled();
}

void HeatPumpController::setSummerMode(bool enabled) {
    settings.setSeason(enabled ? Season::SUMMER : Season::WINTER);
}

bool HeatPumpController::isSummerMode() const {
    return settings.getSeason() == Season::SUMMER;
}

void HeatPumpController::forceDHWHeating() {
    if (!settings.isDHWEnabled()) {
        Serial.println(F("[HP] Cannot force DHW: not enabled"));
        return;
    }
    Serial.println(F("[HP] Forcing DHW heating cycle"));
    _status.lastDHWHeating = 0;  // Reset timer to trigger start
}
