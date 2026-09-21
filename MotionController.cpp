#include "MotionController.h"

#include "ControllerProtocol.h"
#include "DashboardModel.h"
#include "MotionCalculator.h"

#include <cmath>
#include <sstream>

namespace {
constexpr double DegreesToRadians = 3.14159265358979323846 / 180.0;

PlatformAttitude MapSimulatorInput(double pitchRadians, double bankRadians, double rudderDegrees) {
    // Preserve the live path's existing extra rudder adjustment for both sources.
    return CalculatePlatformAttitude(pitchRadians, bankRadians, rudderDegrees / 1.5);
}
}

MotionController::MotionController(DashboardModel& dashboard)
    : dashboard_(dashboard) {
}

void MotionController::SetManualMode(bool manual) {
    if (manualMode_ == manual) {
        return;
    }
    manualMode_ = manual;
    manualInputActive_ = false;
    // A source change must not reuse the previous source's cached command.
    {
        std::lock_guard<std::mutex> lock(payloadMutex_);
        latestPayload_.clear();
    }
    dashboard_.SetManualMode(manual);
    dashboard_.AddEvent(manual ? "Manual input selected; enter angles and press Execute"
        : "MSFS input selected; waiting for simulator data");
}

bool MotionController::ExecuteManualInput(const SimulatorInput& input) {
    if (!manualMode_) {
        return false;
    }
    if (!std::isfinite(input.pitchDegrees) || !std::isfinite(input.rollDegrees)
        || !std::isfinite(input.rudderDegrees)) {
        return false;
    }
    const auto snapshot = dashboard_.GetSnapshot();
    if (!snapshot.clientConnected || !snapshot.positionFeedback) {
        dashboard_.AddEvent("Execute requires a controller and valid position feedback",
            DashboardEventLevel::Warning);
        return false;
    }
    manualAttitude_ = MapSimulatorInput(input.pitchDegrees * DegreesToRadians,
        input.rollDegrees * DegreesToRadians, input.rudderDegrees);
    manualInputActive_ = true;
    // Keep the existing 50 ms calculation cadence even across repeated clicks.
    dashboard_.SetManualInput(input);
    dashboard_.UpdateOrientation(manualAttitude_.pitchDegrees,
        manualAttitude_.rollDegrees, manualAttitude_.yawDegrees);
    std::ostringstream event;
    event << "Manual Execute: pitch " << input.pitchDegrees << ", roll " << input.rollDegrees
          << ", rudder " << input.rudderDegrees << " deg";
    dashboard_.AddEvent(event.str());
    return true;
}

void MotionController::TickManual(bool hasFeedback, const ActuatorValues& currentPositions,
    std::chrono::steady_clock::time_point now) {
    if (!manualMode_ || !manualInputActive_ || now < nextManualCalculation_) {
        return;
    }
    nextManualCalculation_ = now + MotionSettings::ControlInterval;
    UpdateAttitude(manualAttitude_, hasFeedback, currentPositions);
}

void MotionController::UpdateSimulatorInput(double pitchRadians, double bankRadians, double rudderDegrees,
    bool hasFeedback, const ActuatorValues& currentPositions) {
    if (manualMode_) {
        return;
    }
    dashboard_.UpdateSimulatorAttitude(RadiansToDegrees(pitchRadians), RadiansToDegrees(bankRadians));
    UpdateAttitude(MapSimulatorInput(pitchRadians, bankRadians, rudderDegrees), hasFeedback, currentPositions);
}

void MotionController::UpdateAttitude(const PlatformAttitude& attitude,
    bool hasFeedback, const ActuatorValues& currentPositions) {
    dashboard_.UpdateOrientation(attitude.pitchDegrees, attitude.rollDegrees, attitude.yawDegrees);
    // Step limits are relative to measured actuator positions, never an assumed pose.
    if (!hasFeedback) {
        return;
    }
    const auto command = CalculateMotion(attitude, currentPositions);
    dashboard_.UpdateMotion(attitude.pitchDegrees, attitude.rollDegrees, attitude.yawDegrees,
        command.positions, command.speeds);
    const auto payload = BuildCommandPayload(command);
    std::lock_guard<std::mutex> lock(payloadMutex_);
    latestPayload_ = payload;
}

bool MotionController::TryGetLatestPayload(std::string& payload) const {
    std::lock_guard<std::mutex> lock(payloadMutex_);
    if (latestPayload_.empty()) {
        return false;
    }
    payload = latestPayload_;
    return true;
}
