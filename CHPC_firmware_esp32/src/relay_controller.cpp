/**
 * @file relay_controller.cpp
 * @brief Implementation of RelayController
 */

#include "relay_controller.h"

void RelayController::begin() {
    Serial.println(F("[Relays] Initializing relay outputs..."));
    
    // Configure pins as outputs
    pinMode(Pins::RELAY_COMPRESSOR, OUTPUT);
    pinMode(Pins::RELAY_HOT_PUMP, OUTPUT);
    pinMode(Pins::RELAY_COLD_PUMP, OUTPUT);
    pinMode(Pins::RELAY_SUMP_HEATER, OUTPUT);
    pinMode(Pins::RELAY_4WAY_VALVE, OUTPUT);
    
    // Initialize all relays to OFF
    allOff();
    
    _initialized = true;
    Serial.println(F("[Relays] Initialization complete"));
}

void RelayController::update() {
    if (!_initialized) return;
    
    // Only apply if states have changed
    if (_states != _lastAppliedStates) {
        applyStates();
        _lastAppliedStates = _states;
    }
}

void RelayController::applyStates() {
    // Apply relay states to hardware
    // Note: Adjust logic (HIGH/LOW) based on relay module (active HIGH or LOW)
    
    digitalWrite(Pins::RELAY_COMPRESSOR, _states.compressor ? HIGH : LOW);
    digitalWrite(Pins::RELAY_HOT_PUMP, _states.hotPump ? HIGH : LOW);
    digitalWrite(Pins::RELAY_COLD_PUMP, _states.coldPump ? HIGH : LOW);
    
    // Sump heater or DHW valve (depending on configuration)
    // In original code, RELAY_SUMP_HEATER is used for DHW 3-way valve
    digitalWrite(Pins::RELAY_SUMP_HEATER, _states.dhwValve ? HIGH : LOW);
    
    // 4-way valve for heating/cooling reversal
    // Note: Original code had reversed logic for different hardware
    // Adjust based on actual valve wiring
    bool valve4wayOutput = _states.coldPump && !_states.valve4way;  // Original reversed logic
    digitalWrite(Pins::RELAY_4WAY_VALVE, valve4wayOutput ? HIGH : LOW);
    
    Serial.printf("[Relays] Applied: COMP=%d, HOT=%d, COLD=%d, SUMP=%d, 4WAY=%d\n",
                  _states.compressor, _states.hotPump, _states.coldPump,
                  _states.sumpHeater, _states.valve4way);
}

void RelayController::setCompressor(bool state) {
    if (_states.compressor != state) {
        _states.compressor = state;
        Serial.printf("[Relays] Compressor: %s\n", state ? "ON" : "OFF");
    }
}

void RelayController::setHotPump(bool state) {
    if (_states.hotPump != state) {
        _states.hotPump = state;
        Serial.printf("[Relays] Hot pump: %s\n", state ? "ON" : "OFF");
    }
}

void RelayController::setColdPump(bool state) {
    if (_states.coldPump != state) {
        _states.coldPump = state;
        Serial.printf("[Relays] Cold pump: %s\n", state ? "ON" : "OFF");
    }
}

void RelayController::setSumpHeater(bool state) {
    if (_states.sumpHeater != state) {
        _states.sumpHeater = state;
        Serial.printf("[Relays] Sump heater: %s\n", state ? "ON" : "OFF");
    }
}

void RelayController::set4WayValve(bool state) {
    if (_states.valve4way != state) {
        _states.valve4way = state;
        Serial.printf("[Relays] 4-way valve: %s (cooling=%s)\n", 
                      state ? "ON" : "OFF", state ? "YES" : "NO");
    }
}

void RelayController::setDHWValve(bool state) {
    if (_states.dhwValve != state) {
        _states.dhwValve = state;
        Serial.printf("[Relays] DHW valve: %s\n", state ? "DHW" : "CH");
    }
}

void RelayController::allOff() {
    Serial.println(F("[Relays] All OFF"));
    
    _states.compressor = false;
    _states.hotPump = false;
    _states.coldPump = false;
    _states.sumpHeater = false;
    _states.valve4way = false;
    _states.dhwValve = false;
    
    // Immediately apply to hardware
    digitalWrite(Pins::RELAY_COMPRESSOR, LOW);
    digitalWrite(Pins::RELAY_HOT_PUMP, LOW);
    digitalWrite(Pins::RELAY_COLD_PUMP, LOW);
    digitalWrite(Pins::RELAY_SUMP_HEATER, LOW);
    digitalWrite(Pins::RELAY_4WAY_VALVE, LOW);
    
    _lastAppliedStates = _states;
}

bool RelayController::isAnyOn() const {
    return _states.compressor || _states.hotPump || _states.coldPump ||
           _states.sumpHeater || _states.valve4way || _states.dhwValve;
}
