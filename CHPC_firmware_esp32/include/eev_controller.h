/**
 * @file eev_controller.h
 * @brief Electronic Expansion Valve (EEV) controller
 * 
 * Controls a stepper motor driven EEV for refrigerant flow regulation.
 * Implements superheat control algorithm.
 */

#ifndef EEV_CONTROLLER_H
#define EEV_CONTROLLER_H

#include <Arduino.h>
#include "config.h"

/**
 * @enum EEVState
 * @brief Current state of the EEV controller
 */
enum class EEVState : uint8_t {
    IDLE,           // Not operating, at waiting position
    CALIBRATING,    // Full close calibration in progress
    OPENING,        // Opening valve
    CLOSING,        // Closing valve
    HOLDING,        // At target position
    ERROR           // Fault condition
};

/**
 * @class EEVController
 * @brief Manages Electronic Expansion Valve operation
 * 
 * The EEV controls refrigerant flow to maintain proper superheat
 * at the evaporator outlet. This implementation uses a unipolar
 * stepper motor with 8 half-steps for smooth operation.
 */
class EEVController {
public:
    // Singleton access
    static EEVController& getInstance() {
        static EEVController instance;
        return instance;
    }
    
    /**
     * @brief Initialize EEV hardware
     */
    void begin();
    
    /**
     * @brief Main update function - call frequently from loop
     */
    void update();
    
    /**
     * @brief Start full close calibration
     */
    void calibrate();
    
    /**
     * @brief Get current valve position
     * @return Position in pulses (0 = fully closed)
     */
    int16_t getPosition() const { return _currentPosition; }
    
    /**
     * @brief Get position as percentage
     * @return Percentage (0-100)
     */
    float getPositionPercent() const {
        return (_currentPosition * 100.0f) / EEVConfig::MAX_PULSES;
    }
    
    /**
     * @brief Get current state
     */
    EEVState getState() const { return _state; }
    
    /**
     * @brief Check if EEV is actively moving
     */
    bool isMoving() const { return _pendingPulses != 0; }
    
    /**
     * @brief Get current superheat (delta T)
     */
    float getSuperheat() const { return _currentSuperheat; }
    
    /**
     * @brief Set target superheat
     * @param target Target temperature difference in °C
     */
    void setTargetSuperheat(float target);
    
    /**
     * @brief Get target superheat
     */
    float getTargetSuperheat() const { return _targetSuperheat; }
    
    /**
     * @brief Enable/disable automatic control
     */
    void setAutoControl(bool enabled) { _autoControl = enabled; }
    bool isAutoControlEnabled() const { return _autoControl; }
    
    /**
     * @brief Manual position control
     * @param pulses Number of pulses to move (positive = open, negative = close)
     */
    void move(int16_t pulses);
    
    /**
     * @brief Move to specific position
     * @param position Target position in pulses
     */
    void moveTo(int16_t position);
    
    /**
     * @brief Emergency close (liquid protection)
     */
    void emergencyClose();
    
    /**
     * @brief Set to waiting position (HP not running)
     */
    void setWaitingPosition();
    
    /**
     * @brief Called when compressor starts
     */
    void onCompressorStart();
    
    /**
     * @brief Called when compressor stops
     */
    void onCompressorStop();
    
    /**
     * @brief Update superheat control based on temperatures
     * @param tBefore Temperature before evaporator
     * @param tAfter Temperature after evaporator
     * @param compressorRunning Is compressor currently running
     */
    void updateSuperheatControl(float tBefore, float tAfter, bool compressorRunning);
    
private:
    EEVController();
    ~EEVController() = default;
    EEVController(const EEVController&) = delete;
    EEVController& operator=(const EEVController&) = delete;
    
    // Stepper motor control
    void stepMotor();
    void energizeCoils();
    void deenergizeCoils();
    bool isTimeToStep() const;
    
    // Control algorithm
    void runControlAlgorithm();
    
    // Hardware state
    EEVState _state = EEVState::IDLE;
    int16_t _currentPosition = 0;       // Current position in pulses
    int16_t _pendingPulses = 0;         // Pulses remaining to move
    int8_t _currentStep = 0;            // Current half-step (0-7)
    bool _fastMode = false;             // Fast stepping mode
    bool _ignoreMinPosition = false;    // Allow closing below min work position
    
    // Control state
    float _targetSuperheat = EEVConfig::TARGET_TEMP_DIFF;
    float _currentSuperheat = 0.0f;
    bool _autoControl = true;
    bool _compressorRunning = false;
    
    // Timing
    uint32_t _lastStepTime = 0;
    uint32_t _lastControlUpdate = 0;
    uint32_t _lastCalibration = 0;
    uint32_t _lastEnergize = 0;
    
    // Half-step sequence for unipolar stepper
    static constexpr uint8_t STEP_SEQUENCE[8] = {
        0b1000, 0b1010, 0b0010, 0b0110,
        0b0100, 0b0101, 0b0001, 0b1001
    };
};

// Convenience macro
#define eevController EEVController::getInstance()

#endif // EEV_CONTROLLER_H
