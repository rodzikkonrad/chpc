/**
 * @file eev_controller.cpp
 * @brief Implementation of EEV Controller
 */

#include "eev_controller.h"
#include "settings.h"

// Define static constexpr
constexpr uint8_t EEVController::STEP_SEQUENCE[8];

EEVController::EEVController() {
    _targetSuperheat = EEVConfig::TARGET_TEMP_DIFF;
}

void EEVController::begin() {
    Serial.println(F("[EEV] Initializing Electronic Expansion Valve..."));
    
    // Configure GPIO pins
    pinMode(Pins::EEV_COIL_1, OUTPUT);
    pinMode(Pins::EEV_COIL_2, OUTPUT);
    pinMode(Pins::EEV_COIL_3, OUTPUT);
    pinMode(Pins::EEV_COIL_4, OUTPUT);
    
    deenergizeCoils();
    
    // Load target from settings
    _targetSuperheat = settings.getEEVSetpoint();
    
    // Perform initial calibration
    Serial.println(F("[EEV] Starting initial calibration..."));
    calibrate();
    
    Serial.println(F("[EEV] Initialization complete"));
}

void EEVController::update() {
    uint32_t now = millis();
    
    // Process pending movement
    if (_pendingPulses != 0 && isTimeToStep()) {
        stepMotor();
        _lastStepTime = now;
    }
    
    // Periodic coil energize to maintain position (prevents drift)
    if (_pendingPulses == 0 && (now - _lastEnergize > 10000)) {
        energizeCoils();
        delay(30);
        deenergizeCoils();
        _lastEnergize = now;
    }
    
    // Check for periodic recalibration
    if (!_compressorRunning && 
        (now - _lastCalibration > Timing::EEV_RECALIBRATE_MS || _lastCalibration == 0)) {
        calibrate();
    }
}

void EEVController::calibrate() {
    Serial.println(F("[EEV] Calibrating (full close)..."));
    
    _state = EEVState::CALIBRATING;
    _ignoreMinPosition = true;
    _fastMode = true;
    
    // Close fully plus extra pulses for certainty
    int16_t closeAmount = _lastCalibration == 0 ? 
        EEVConfig::MAX_PULSES + EEVConfig::CLOSE_ADD_PULSES :
        _currentPosition + EEVConfig::CLOSE_ADD_PULSES;
    
    _pendingPulses = -closeAmount;
    
    // Wait for closing to complete
    while (_pendingPulses != 0) {
        if (isTimeToStep()) {
            stepMotor();
            _lastStepTime = millis();
        }
        yield();
    }
    
    _currentPosition = 0;
    _lastCalibration = millis();
    
    // Move to waiting position if configured
    if (EEVConfig::OPEN_AFTER_CLOSE > 0) {
        _fastMode = true;
        _ignoreMinPosition = false;
        _pendingPulses = EEVConfig::OPEN_AFTER_CLOSE;
        
        while (_pendingPulses != 0) {
            if (isTimeToStep()) {
                stepMotor();
                _lastStepTime = millis();
            }
            yield();
        }
    }
    
    _state = EEVState::IDLE;
    Serial.printf("[EEV] Calibration complete, position: %d\n", _currentPosition);
}

bool EEVController::isTimeToStep() const {
    uint32_t elapsed = millis() - _lastStepTime;
    if (_lastStepTime == 0) return true;
    
    if (_pendingPulses < 0) {
        // Closing
        return elapsed > (_fastMode ? Timing::EEV_PULSE_FAST_CLOSE_MS : Timing::EEV_PULSE_SLOW_CLOSE_MS);
    } else if (_pendingPulses > 0) {
        // Opening
        if (_currentPosition < EEVConfig::MIN_WORK_POSITION) {
            return elapsed > Timing::EEV_PULSE_WAIT_OPEN_MS;
        }
        return elapsed > (_fastMode ? Timing::EEV_PULSE_FAST_OPEN_MS : Timing::EEV_PULSE_SLOW_OPEN_MS);
    }
    
    return false;
}

void EEVController::stepMotor() {
    if (_pendingPulses > 0 && _currentPosition < EEVConfig::MAX_PULSES) {
        // Opening
        _currentPosition++;
        _currentStep = (_currentStep + 1) & 0x07;  // Wrap 0-7
        _pendingPulses--;
        _state = EEVState::OPENING;
    } else if (_pendingPulses < 0) {
        // Closing
        if (_currentPosition > EEVConfig::MIN_WORK_POSITION || _ignoreMinPosition) {
            _currentPosition--;
            if (_currentPosition < 0) _currentPosition = 0;
            _currentStep = (_currentStep - 1 + 8) & 0x07;  // Wrap 0-7
            _pendingPulses++;
            _state = EEVState::CLOSING;
        } else {
            _pendingPulses = 0;
        }
    }
    
    if (_pendingPulses == 0) {
        _state = EEVState::HOLDING;
        _ignoreMinPosition = false;
    }
    
    energizeCoils();
    delayMicroseconds(50);
    deenergizeCoils();
}

void EEVController::energizeCoils() {
    uint8_t pattern = STEP_SEQUENCE[_currentStep];
    digitalWrite(Pins::EEV_COIL_1, (pattern >> 0) & 0x01);
    digitalWrite(Pins::EEV_COIL_2, (pattern >> 1) & 0x01);
    digitalWrite(Pins::EEV_COIL_3, (pattern >> 2) & 0x01);
    digitalWrite(Pins::EEV_COIL_4, (pattern >> 3) & 0x01);
}

void EEVController::deenergizeCoils() {
    digitalWrite(Pins::EEV_COIL_1, LOW);
    digitalWrite(Pins::EEV_COIL_2, LOW);
    digitalWrite(Pins::EEV_COIL_3, LOW);
    digitalWrite(Pins::EEV_COIL_4, LOW);
}

void EEVController::setTargetSuperheat(float target) {
    target = constrain(target, 0.5f, 15.0f);
    if (_targetSuperheat != target) {
        _targetSuperheat = target;
        settings.setEEVSetpoint(target);
    }
}

void EEVController::move(int16_t pulses) {
    _pendingPulses = pulses;
    _fastMode = false;
}

void EEVController::moveTo(int16_t position) {
    position = constrain(position, 0, EEVConfig::MAX_PULSES);
    int16_t delta = position - _currentPosition;
    if (delta != 0) {
        _pendingPulses = delta;
        _fastMode = abs(delta) > 20;
    }
}

void EEVController::emergencyClose() {
    Serial.println(F("[EEV] EMERGENCY CLOSE!"));
    _state = EEVState::CLOSING;
    _ignoreMinPosition = false;  // Close to min work position only
    _fastMode = true;
    _pendingPulses = -(EEVConfig::MAX_PULSES);  // Will stop at MIN_WORK_POSITION
}

void EEVController::setWaitingPosition() {
    if (_currentPosition != EEVConfig::OPEN_AFTER_CLOSE) {
        moveTo(EEVConfig::OPEN_AFTER_CLOSE);
    }
}

void EEVController::onCompressorStart() {
    _compressorRunning = true;
    
    // Ensure valve is at minimum working position
    if (_currentPosition < EEVConfig::MIN_WORK_POSITION) {
        _fastMode = true;
        _pendingPulses = EEVConfig::MIN_WORK_POSITION - _currentPosition;
        Serial.printf("[EEV] Opening to min work position: %d\n", EEVConfig::MIN_WORK_POSITION);
    }
}

void EEVController::onCompressorStop() {
    _compressorRunning = false;
    setWaitingPosition();
}

void EEVController::updateSuperheatControl(float tBefore, float tAfter, bool compressorRunning) {
    if (!_autoControl) return;
    
    _compressorRunning = compressorRunning;
    
    // Calculate current superheat (after - before for evaporator)
    _currentSuperheat = tAfter - tBefore;
    
    if (!compressorRunning) {
        // Compressor not running - no active control
        return;
    }
    
    // Don't control while moving
    if (_pendingPulses != 0) return;
    
    // Get hot outlet temp modifier (reduce target at high temps)
    float modifier = 0.0f;
    // Could add modifier based on Tho if available
    
    float adjustedTarget = _targetSuperheat + modifier;
    
    bool shouldClose = _currentSuperheat < adjustedTarget;
    bool shouldOpen = _currentSuperheat > adjustedTarget;
    bool emergencyClose = _currentSuperheat < (adjustedTarget - EEVConfig::EMERGENCY_DIFF);
    bool fastOpen = _currentSuperheat > (adjustedTarget + EEVConfig::HYSTERESIS + EEVConfig::PRECISE_START);
    
    // Control logic
    if (_currentPosition >= EEVConfig::MIN_WORK_POSITION) {
        if (emergencyClose) {
            // Emergency - superheat too low (risk of liquid)
            _pendingPulses = -1;
            _fastMode = true;
            Serial.printf("[EEV] Emergency close! SH=%.1f, target=%.1f\n", _currentSuperheat, adjustedTarget);
        } else if (shouldClose) {
            // Superheat below target - close slowly
            _pendingPulses = -1;
            _fastMode = false;
        } else if (fastOpen) {
            // Superheat well above target - open fast
            _pendingPulses = 1;
            _fastMode = true;
        } else if (shouldOpen && _currentSuperheat > adjustedTarget + EEVConfig::HYSTERESIS) {
            // Superheat above target + hysteresis - open slowly
            _pendingPulses = 1;
            _fastMode = false;
        }
        // Within hysteresis - no action needed
    }
}
