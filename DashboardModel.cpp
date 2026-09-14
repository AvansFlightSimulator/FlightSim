#include "DashboardModel.h"

#include <algorithm>

namespace {
constexpr std::size_t MaximumDashboardEvents = 80;
}

DashboardModel::DashboardModel()
    : startTime_(std::chrono::steady_clock::now()) {
}

void DashboardModel::SetSimulatorConnected(bool connected) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.simulatorConnected = connected;
}

void DashboardModel::SetTcpListening(bool listening) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.tcpListening = listening;
}

void DashboardModel::SetClientConnected(bool connected) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.clientConnected = connected;
    if (!connected) {
        state_.positionFeedback = false;
    }
}

void DashboardModel::UpdateFeedback(const std::array<float, 6>& positions) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.currentPositions = positions;
    state_.positionFeedback = true;
    ++state_.receivedMessages;
    state_.lastFeedbackMilliseconds = ElapsedMilliseconds();
}

void DashboardModel::UpdateOrientation(
    double pitchDegrees,
    double rollDegrees,
    double yawDegrees) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.pitchDegrees = pitchDegrees;
    state_.rollDegrees = rollDegrees;
    state_.yawDegrees = yawDegrees;
    state_.motionAvailable = true;
}

void DashboardModel::UpdateMotion(
    double pitchDegrees,
    double rollDegrees,
    double yawDegrees,
    const std::array<float, 6>& targetPositions,
    const std::array<float, 6>& speeds) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.pitchDegrees = pitchDegrees;
    state_.rollDegrees = rollDegrees;
    state_.yawDegrees = yawDegrees;
    state_.targetPositions = targetPositions;
    state_.speeds = speeds;
    state_.motionAvailable = true;
}

void DashboardModel::RecordCommandSent() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++state_.sentMessages;
    state_.lastSendMilliseconds = ElapsedMilliseconds();
}

void DashboardModel::AddEvent(const std::string& message, DashboardEventLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back({ ElapsedMilliseconds(), level, message });
    while (events_.size() > MaximumDashboardEvents) {
        events_.pop_front();
    }
}

DashboardSnapshot DashboardModel::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    DashboardSnapshot snapshot = state_;
    snapshot.elapsedMilliseconds = ElapsedMilliseconds();
    snapshot.events.assign(events_.begin(), events_.end());
    return snapshot;
}

long long DashboardModel::ElapsedMilliseconds() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime_).count();
}
