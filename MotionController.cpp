#include "MotionController.h"

#include "BuildMode.h"
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

void MotionController::SetInputMode(InputMode mode) {
    if (mode == InputMode::ActuatorPositions && !SupportsActuatorPositions()) {
        return;
    }
    if (inputMode_ == mode) {
        return;
    }
    inputMode_ = mode;
    manualInputActive_ = false;
    // A source change must not reuse the previous source's cached command.
    {
        std::lock_guard<std::mutex> lock(payloadMutex_);
        latestPayload_.clear();
    }
    dashboard_.SetInputMode(mode);
    dashboard_.AddEvent(mode == InputMode::ActuatorPositions ? "Actuator positions selected; enter A1-A6 and press Execute"
        : mode == InputMode::ManualAngles ? "Manual input selected; enter angles and press Execute"
        : "MSFS input selected; waiting for simulator data");
}

bool MotionController::SupportsActuatorPositions() noexcept {
#ifdef TARGET_PLC
    return true;
#else
    // Unity's existing contract requires orientation; no forward kinematics
    // are implemented to derive it from independent actuator positions.
    return false;
#endif
}

bool MotionController::ExecuteActuatorInput(const ActuatorValues& positions) {
    if (inputMode_ != InputMode::ActuatorPositions || !SupportsActuatorPositions()) {
        return false;
    }
    ActuatorValues rounded{};
    for (std::size_t index = 0; index < ActuatorCount; ++index) {
        const float value = positions[index];
        if (!std::isfinite(value) || value < MotionSettings::MinimumActuatorInput
            || value > MotionSettings::MaximumActuatorInput) {
            return false;
        }
        rounded[index] = std::round(value);
    }
    const auto snapshot = dashboard_.GetSnapshot();
    if (!snapshot.clientConnected || !snapshot.positionFeedback) {
        dashboard_.AddEvent("Execute requires a controller and valid position feedback",
            DashboardEventLevel::Warning);
        return false;
    }
    actuatorPositions_ = rounded;
    manualInputActive_ = true;
    dashboard_.SetActuatorInput(rounded);
    std::ostringstream event;
    event << "Actuator Execute:";
    for (std::size_t index = 0; index < ActuatorCount; ++index) {
        event << " A" << index + 1 << '=' << rounded[index];
    }
    dashboard_.AddEvent(event.str());
    return true;
}

bool MotionController::ExecuteManualInput(const SimulatorInput& input) {
    if (inputMode_ != InputMode::ManualAngles) {
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
    if (inputMode_ == InputMode::Simulator || !manualInputActive_ || now < nextManualCalculation_) {
        return;
    }
    nextManualCalculation_ = now + MotionSettings::ControlInterval;
    if (inputMode_ == InputMode::ActuatorPositions) {
        if (hasFeedback) {
            const auto command = CalculateActuatorMotion(actuatorPositions_, currentPositions);
            dashboard_.UpdateActuatorMotion(command.positions, command.speeds);
            PublishCommand(command);
        }
        return;
    }
    UpdateAttitude(manualAttitude_, hasFeedback, currentPositions);
}

void MotionController::UpdateSimulatorInput(double pitchRadians, double bankRadians, double rudderDegrees,
    bool hasFeedback, const ActuatorValues& currentPositions) {
    if (inputMode_ != InputMode::Simulator) {
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
    PublishCommand(command);
}

void MotionController::PublishCommand(const MotionCommand& command) {
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
