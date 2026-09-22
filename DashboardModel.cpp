#include "DashboardModel.h"

namespace {
constexpr std::size_t MaximumDashboardEvents = 80;
}

DashboardModel::DashboardModel()
    : startTime_(std::chrono::steady_clock::now()) {
}

void DashboardModel::SetInputMode(InputMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.inputMode = mode;
    state_.manualInputAvailable = false;
    state_.actuatorInputAvailable = false;
    state_.actuatorCommandAvailable = false;
    state_.motionAvailable = false;
    state_.targetPositions = {};
    state_.speeds = {};
}

void DashboardModel::SetActuatorInput(const ActuatorValues& positions) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.requestedPositions = positions;
    state_.actuatorInputAvailable = true;
}

void DashboardModel::UpdateActuatorMotion(const ActuatorValues& positions, const ActuatorValues& speeds) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.targetPositions = positions;
    state_.speeds = speeds;
    state_.actuatorCommandAvailable = true;
    state_.motionAvailable = false;
}

void DashboardModel::SetManualInput(const SimulatorInput& input) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.manualInput = input;
    state_.manualInputAvailable = true;
}

void DashboardModel::SetSimulatorConnected(bool connected) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.simulatorConnected = connected;
    if (!connected) {
        state_.simulatorAttitudeAvailable = false;
        state_.simulatorRudderAvailable = false;
    }
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

void DashboardModel::UpdateFeedback(const ActuatorValues& positions) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.currentPositions = positions;
    state_.positionFeedback = true;
    ++state_.receivedMessages;
    state_.lastFeedbackMilliseconds = ElapsedMilliseconds();
}

void DashboardModel::UpdateSimulatorAttitude(double pitchDegrees, double rollDegrees) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.simulatorPitchDegrees = pitchDegrees;
    state_.simulatorRollDegrees = rollDegrees;
    state_.simulatorAttitudeAvailable = true;
}

void DashboardModel::UpdateSimulatorRudder(double rudderDegrees) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.simulatorRudderDegrees = rudderDegrees;
    state_.simulatorRudderAvailable = true;
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
    const ActuatorValues& targetPositions,
    const ActuatorValues& speeds) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.pitchDegrees = pitchDegrees;
    state_.rollDegrees = rollDegrees;
    state_.yawDegrees = yawDegrees;
    state_.targetPositions = targetPositions;
    state_.speeds = speeds;
    state_.actuatorCommandAvailable = true;
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
