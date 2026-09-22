#pragma once

#include "BridgeTypes.h"
#include "SimulatorInputFilter.h"

#include <chrono>
#include <mutex>
#include <string>

class DashboardModel;

// Shared command pipeline, independent of the simulator SDK and sockets.
// Updates run on the main thread; the output worker only copies the payload.
class MotionController {
public:
    explicit MotionController(DashboardModel& dashboard);

    InputMode GetInputMode() const noexcept { return inputMode_; }
    void SetInputMode(InputMode mode);
    bool ExecuteManualInput(const SimulatorInput& input);
    static bool SupportsActuatorPositions() noexcept;
    bool ExecuteActuatorInput(const ActuatorValues& positions);
    void TickManual(bool hasFeedback,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    void UpdateSimulatorInput(double pitchRadians, double bankRadians, double rudderDegrees,
        bool hasFeedback,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    void ResetSimulatorInputFilter() noexcept;
    bool TryGetLatestPayload(std::string& payload) const;

private:
    void PublishCommand(const MotionCommand& command);
    void UpdateAttitude(const PlatformAttitude& attitude, bool hasFeedback);

    DashboardModel& dashboard_;
    InputMode inputMode_ = InputMode::Simulator;
    bool manualInputActive_ = false;
    PlatformAttitude manualAttitude_;
    ActuatorValues actuatorPositions_{};
    std::chrono::steady_clock::time_point nextManualCalculation_{};
    SimulatorInputFilter simulatorInputFilter_;
    mutable std::mutex payloadMutex_;
    std::string latestPayload_;
};
