/**
 * @file heat_pump_controller.h
 * @brief Main heat pump control logic
 * 
 * Implements the core heat pump control algorithm including:
 * - Thermostat control
 * - Compressor cycling
 * - Protection logic
 * - DHW (Domestic Hot Water) support
 * - Cooling mode support
 */

#ifndef HEAT_PUMP_CONTROLLER_H
#define HEAT_PUMP_CONTROLLER_H

#include <Arduino.h>
#include "config.h"
#include "temperature_sensors.h"
#include "relay_controller.h"
#include "eev_controller.h"
#include "settings.h"

/**
 * @struct HeatPumpStatus
 * @brief Current status of the heat pump system
 */
struct HeatPumpStatus {
    SystemMode mode = SystemMode::OFF;
    ErrorCode errorCode = ErrorCode::OK;
    uint8_t lastError = 0;                  // Last error code as uint8_t
    
    // Component states
    bool compressorRunning = false;
    bool hotPumpRunning = false;
    bool coldPumpRunning = false;
    bool sumpHeaterOn = false;
    bool valve4wayActive = false;       // false = heating, true = cooling
    bool dhwValveActive = false;        // false = CH, true = DHW
    bool dhwHeatingActive = false;
    
    // Timing
    uint32_t compressorOnTime = 0;      // How long compressor has been on
    uint32_t compressorOffTime = 0;     // How long compressor has been off
    uint32_t lastDHWHeating = 0;        // Last DHW heating cycle end time
    
    // Power monitoring
    float currentPower = 0.0f;          // Current power consumption in watts
    float currentAmps = 0.0f;           // Current draw in amps
};

/**
 * @class HeatPumpController
 * @brief Main heat pump control logic
 */
class HeatPumpController {
public:
    // Singleton access
    static HeatPumpController& getInstance() {
        static HeatPumpController instance;
        return instance;
    }
    
    /**
     * @brief Initialize the heat pump controller
     */
    void begin();
    
    /**
     * @brief Main control loop - call frequently
     */
    void update();
    
    /**
     * @brief Get current status
     */
    const HeatPumpStatus& getStatus() const { return _status; }
    
    /**
     * @brief Get current error code
     */
    ErrorCode getErrorCode() const { return _status.errorCode; }
    
    /**
     * @brief Clear error and attempt restart
     */
    void clearError();
    
    /**
     * @brief Emergency stop
     */
    void emergencyStop();
    
    /**
     * @brief Manual system reset
     */
    void reset();
    
    /**
     * @brief Set target temperature
     */
    void setTargetTemperature(float temp);
    float getTargetTemperature() const;
    
    /**
     * @brief Set DHW target temperature
     */
    void setDHWTargetTemperature(float temp);
    float getDHWTargetTemperature() const;
    
    /**
     * @brief Set cooling mode
     */
    void setCoolingMode(bool enabled);
    bool isCoolingMode() const;
    
    /**
     * @brief Set summer mode (DHW only)
     */
    void setSummerMode(bool enabled);
    bool isSummerMode() const;
    
    /**
     * @brief Force DHW heating cycle
     */
    void forceDHWHeating();
    
    /**
     * @brief Get power consumption
     */
    float getPowerConsumption() const { return _status.currentPower; }
    
private:
    HeatPumpController() = default;
    ~HeatPumpController() = default;
    HeatPumpController(const HeatPumpController&) = delete;
    HeatPumpController& operator=(const HeatPumpController&) = delete;
    
    // Control logic functions
    void checkStartConditions();
    void checkStopConditions();
    void runProtectionChecks();
    void handleDHWControl();
    void handleSumpHeater();
    void controlColdPump();
    void controlHotPump();
    void controlCompressor();
    void handle4WayValve();
    
    // Pump and compressor control
    void startColdPump();
    void stopColdPump();
    void startHotPump();
    void stopHotPump();
    void startCompressor();
    void stopCompressor();
    
    // Power monitoring
    void updatePowerReading();
    
    // Error handling
    void setError(ErrorCode code);
    bool checkSensorErrors();
    
    // State
    HeatPumpStatus _status;
    
    // Timing variables
    uint32_t _lastUpdateTime = 0;
    uint32_t _compressorStartTime = 0;
    uint32_t _compressorStopTime = 0;
    uint32_t _coldPumpStartTime = 0;
    uint32_t _coldPumpStopTime = 0;
    uint32_t _dhwStartTime = 0;
    uint32_t _delayedCompressorStart = 0;   // Scheduled compressor start time
    uint32_t _delayedColdPumpStop = 0;      // Scheduled cold pump stop time
    uint32_t _startupCompleteTime = 0;
    
    // Flags
    bool _initialized = false;
    bool _startupComplete = false;
    bool _pendingCompressorStart = false;
    bool _pendingColdPumpStop = false;
    
    // Power monitoring
    float _powerReadingSum = 0.0f;
    uint32_t _powerReadingCount = 0;
    uint32_t _lastPowerReadTime = 0;
    
    // ADC for current sensing
    static constexpr uint16_t ADC_SAMPLES = 1000;
    float calculateRMSCurrent();
};

// Convenience macro
#define heatPumpController HeatPumpController::getInstance()

#endif // HEAT_PUMP_CONTROLLER_H
