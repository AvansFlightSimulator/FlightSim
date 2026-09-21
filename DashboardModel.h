#pragma once

#include "BridgeTypes.h"
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

enum class DashboardEventLevel {
    Info,
    Warning,
    Error
};

struct DashboardEvent {
    long long elapsedMilliseconds = 0;
    DashboardEventLevel level = DashboardEventLevel::Info;
    std::string message;
};

struct DashboardSnapshot {
    bool simulatorConnected = false;
    bool tcpListening = false;
    bool clientConnected = false;
    bool positionFeedback = false;
    bool motionAvailable = false;
    bool manualMode = false;
    bool manualInputAvailable = false;
    SimulatorInput manualInput;

    ActuatorValues currentPositions{};
    ActuatorValues targetPositions{};
    ActuatorValues speeds{};

    double pitchDegrees = 0.0;
    double rollDegrees = 0.0;
    double yawDegrees = 0.0;

    // Simulator values in degrees, before motion sign changes, scaling or limits.
    bool simulatorAttitudeAvailable = false;
    bool simulatorRudderAvailable = false;
    double simulatorPitchDegrees = 0.0;
    double simulatorRollDegrees = 0.0;
    double simulatorRudderDegrees = 0.0;

    std::uint64_t sentMessages = 0;
    std::uint64_t receivedMessages = 0;
    long long elapsedMilliseconds = 0;
    long long lastSendMilliseconds = -1;
    long long lastFeedbackMilliseconds = -1;
    std::vector<DashboardEvent> events;
};

// Thread-safe presentation model shared by the simulator, TCP workers and HMI.
class DashboardModel {
public:
    DashboardModel();

    void SetSimulatorConnected(bool connected);
    void SetManualMode(bool manual);
    void SetManualInput(const SimulatorInput& input);
    void SetTcpListening(bool listening);
    void SetClientConnected(bool connected);
    void UpdateFeedback(const ActuatorValues& positions);
    void UpdateSimulatorAttitude(double pitchDegrees, double rollDegrees);
    void UpdateSimulatorRudder(double rudderDegrees);
    void UpdateOrientation(double pitchDegrees, double rollDegrees, double yawDegrees);
    void UpdateMotion(
        double pitchDegrees,
        double rollDegrees,
        double yawDegrees,
        const ActuatorValues& targetPositions,
        const ActuatorValues& speeds);
    void RecordCommandSent();
    void AddEvent(const std::string& message, DashboardEventLevel level = DashboardEventLevel::Info);

    DashboardSnapshot GetSnapshot() const;

private:
    long long ElapsedMilliseconds() const;

    const std::chrono::steady_clock::time_point startTime_;
    mutable std::mutex mutex_;
    DashboardSnapshot state_;
    std::deque<DashboardEvent> events_;
};
