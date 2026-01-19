/**
 * @file relay_controller.h
 * @brief Relay/output management for heat pump components
 * 
 * Controls all relay outputs for compressor, pumps, valves, etc.
 */

#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include <Arduino.h>
#include "config.h"

/**
 * @struct RelayStates
 * @brief Current state of all relays
 */
struct RelayStates {
    bool compressor = false;
    bool hotPump = false;
    bool coldPump = false;
    bool sumpHeater = false;     // Also used for DHW 3-way valve in some configs
    bool valve4way = false;
    
    // Extended state (for when sump heater is separate from DHW valve)
    bool dhwValve = false;
    
    bool operator==(const RelayStates& other) const {
        return compressor == other.compressor &&
               hotPump == other.hotPump &&
               coldPump == other.coldPump &&
               sumpHeater == other.sumpHeater &&
               valve4way == other.valve4way &&
               dhwValve == other.dhwValve;
    }
    
    bool operator!=(const RelayStates& other) const {
        return !(*this == other);
    }
};

/**
 * @class RelayController
 * @brief Manages all relay outputs
 */
class RelayController {
public:
    // Singleton access
    static RelayController& getInstance() {
        static RelayController instance;
        return instance;
    }
    
    /**
     * @brief Initialize relay pins
     */
    void begin();
    
    /**
     * @brief Apply current relay states to hardware
     */
    void update();
    
    /**
     * @brief Get current relay states
     */
    const RelayStates& getStates() const { return _states; }
    
    // Individual relay control
    void setCompressor(bool state);
    void setHotPump(bool state);
    void setColdPump(bool state);
    void setSumpHeater(bool state);
    void set4WayValve(bool state);
    void setDHWValve(bool state);
    
    // Convenience getters
    bool isCompressorOn() const { return _states.compressor; }
    bool isHotPumpOn() const { return _states.hotPump; }
    bool isColdPumpOn() const { return _states.coldPump; }
    bool isSumpHeaterOn() const { return _states.sumpHeater; }
    bool is4WayValveOn() const { return _states.valve4way; }
    bool isDHWValveOn() const { return _states.dhwValve; }
    
    /**
     * @brief Turn off all relays (emergency stop)
     */
    void allOff();
    
    /**
     * @brief Check if any relay is on
     */
    bool isAnyOn() const;
    
private:
    RelayController() = default;
    ~RelayController() = default;
    RelayController(const RelayController&) = delete;
    RelayController& operator=(const RelayController&) = delete;
    
    void applyStates();
    
    RelayStates _states;
    RelayStates _lastAppliedStates;
    bool _initialized = false;
};

// Convenience macro
#define relayController RelayController::getInstance()

#endif // RELAY_CONTROLLER_H
