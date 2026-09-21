#pragma once

#include "BridgeTypes.h"

#include <chrono>
#include <mutex>
#include <string>

class DashboardModel;

// Shared command pipeline, independent of the simulator SDK and sockets.
// Updates run on the main thread; the output worker only copies the payload.
class MotionController {
public:
    explicit MotionController(DashboardModel& dashboard);

    bool IsManualMode() const noexcept { return manualMode_; }
    void SetManualMode(bool manual);
    bool ExecuteManualInput(const SimulatorInput& input);
    void TickManual(bool hasFeedback, const ActuatorValues& currentPositions,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    void UpdateSimulatorInput(double pitchRadians, double bankRadians, double rudderDegrees,
        bool hasFeedback, const ActuatorValues& currentPositions);
    bool TryGetLatestPayload(std::string& payload) const;

private:
    void UpdateAttitude(const PlatformAttitude& attitude,
        bool hasFeedback, const ActuatorValues& currentPositions);

    DashboardModel& dashboard_;
    bool manualMode_ = false;
    bool manualInputActive_ = false;
    PlatformAttitude manualAttitude_;
    std::chrono::steady_clock::time_point nextManualCalculation_{};
    mutable std::mutex payloadMutex_;
    std::string latestPayload_;
};
