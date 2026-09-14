#pragma once

#include <array>
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

    std::array<float, 6> currentPositions{};
    std::array<float, 6> targetPositions{};
    std::array<float, 6> speeds{};

    double pitchDegrees = 0.0;
    double rollDegrees = 0.0;
    double yawDegrees = 0.0;

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
    void SetTcpListening(bool listening);
    void SetClientConnected(bool connected);
    void UpdateFeedback(const std::array<float, 6>& positions);
    void UpdateOrientation(double pitchDegrees, double rollDegrees, double yawDegrees);
    void UpdateMotion(
        double pitchDegrees,
        double rollDegrees,
        double yawDegrees,
        const std::array<float, 6>& targetPositions,
        const std::array<float, 6>& speeds);
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
