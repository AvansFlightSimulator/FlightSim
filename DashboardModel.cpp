#include "DashboardModel.h"

namespace {
    // Max number of events kept in the dashboard log.
    constexpr std::size_t MaximumDashboardEvents = 80;
}

// Initialize startTime_ to now (steady clock). 
DashboardModel::DashboardModel()
    : startTime_(std::chrono::steady_clock::now()) {}

// Switch input mode and reset all input/motion availability and targets.
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

// Store requested actuator positions and mark them available.
void DashboardModel::SetActuatorInput(const ActuatorValues& positions) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.requestedPositions = positions;
    state_.actuatorInputAvailable = true;
}

// Update actuator targets and speeds (no orientation data).
void DashboardModel::UpdateActuatorMotion(const ActuatorValues& positions, const ActuatorValues& speeds) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.targetPositions = positions;
    state_.speeds = speeds;
    state_.actuatorCommandAvailable = true;
    state_.motionAvailable = false;
}

// Store manual simulator input and mark it available.
void DashboardModel::SetManualInput(const SimulatorInput& input) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.manualInput = input;
    state_.manualInputAvailable = true;
}

// Set simulator connection; clear simulator data on disconnect.
void DashboardModel::SetSimulatorConnected(bool connected) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.simulatorConnected = connected;
    if (!connected) {
        state_.simulatorAttitudeAvailable = false;
        state_.simulatorRudderAvailable = false;
    }
}

// Set whether the TCP server is listening.
void DashboardModel::SetTcpListening(bool listening) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.tcpListening = listening;
}

// Set client connection; clear position feedback on disconnect.
void DashboardModel::SetClientConnected(bool connected) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.clientConnected = connected;
    if (!connected) {
        state_.positionFeedback = false;
    }
}

// Store actuator feedback, count the message, and timestamp it.
void DashboardModel::UpdateFeedback(const ActuatorValues& positions) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.currentPositions = positions;
    state_.positionFeedback = true;
    ++state_.receivedMessages;
    state_.lastFeedbackMilliseconds = ElapsedMilliseconds();
}

// Store simulator pitch/roll and mark attitude available.
void DashboardModel::UpdateSimulatorAttitude(double pitchDegrees, double rollDegrees) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.simulatorPitchDegrees = pitchDegrees;
    state_.simulatorRollDegrees = rollDegrees;
    state_.simulatorAttitudeAvailable = true;
}

// Store simulator rudder angle and mark it available.
void DashboardModel::UpdateSimulatorRudder(double rudderDegrees) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.simulatorRudderDegrees = rudderDegrees;
    state_.simulatorRudderAvailable = true;
}

// Update platform orientation (pitch/roll/yaw) only.
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

// Update orientation plus actuator targets and speeds together.
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

// Count a sent command and timestamp it.
void DashboardModel::RecordCommandSent() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++state_.sentMessages;
    state_.lastSendMilliseconds = ElapsedMilliseconds();
}

// Append a timestamped event; drop oldest beyond the limit.
void DashboardModel::AddEvent(const std::string& message, DashboardEventLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back({ ElapsedMilliseconds(), level, message });
    while (events_.size() > MaximumDashboardEvents) {
        events_.pop_front();
    }
}

// Return a thread-safe copy of the current state and events.
DashboardSnapshot DashboardModel::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    DashboardSnapshot snapshot = state_;
    snapshot.elapsedMilliseconds = ElapsedMilliseconds();
    snapshot.events.assign(events_.begin(), events_.end());
    return snapshot;
}

// Milliseconds elapsed since construction.
long long DashboardModel::ElapsedMilliseconds() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime_).count();
}