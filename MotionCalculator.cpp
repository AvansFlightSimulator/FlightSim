#include "MotionCalculator.h"

#include "calculate_legs.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double Pi = 3.14159265358979323846;

// Existing platform calibration. Confirm physical units and hardware limits with
// the owner before tuning these values; this refactor does not recalibrate them.
constexpr double MaximumPitchDegrees = 30.0;
constexpr double MaximumRollDegrees = 30.0;
constexpr double MaximumYawDegrees = 30.0;
constexpr float SpeedLimit = 500.0f;
constexpr float MinimumSpeed = 2.0f;
constexpr float MaximumStepPerSecond = 400.0f;
constexpr float BaseLegLength = 1156.372420286821f;
constexpr float NeutralActuatorPosition = 200.0f;

// Physical mounting coordinates for the six-actuator Stewart platform.
const vec baseLegs[ActuatorCount] = {
    {177.53f, 723.37f, 0.0f},
    {-177.53f, 723.37f, 0.0f},
    {-715.23f, -207.94f, 0.0f},
    {-537.69f, -515.43f, 0.0f},
    {537.69f, -515.43f, 0.0f},
    {715.23f, -207.94f, 0.0f}
};

const vec platformLegs[ActuatorCount] = {
    {360.59f, 346.0f, 0.0f},
    {-360.59f, 346.0f, 0.0f},
    {-480.12f, 139.59f, 0.0f},
    {-119.17f, -485.59f, 0.0f},
    {119.17f, -485.59f, 0.0f},
    {480.12f, 139.59f, 0.0f}
};

const vec startHeight{ 0.0f, 0.0f, 1079.0f };

// Scaling and limiting are separate steps: halving is normal motion mapping.
double ScaleAndLimitAngle(double degrees, double limit) {
    const double scaledDegrees = degrees / 2.0;
    return (std::max)(-limit, (std::min)(scaledDegrees, limit));
}
}

double RadiansToDegrees(double radians) {
    return radians * (180.0 / Pi);
}

PlatformAttitude CalculatePlatformAttitude(
    double pitchRadians, double bankRadians, double rudderDegrees) {
    PlatformAttitude attitude;
    attitude.pitchDegrees = ScaleAndLimitAngle(-RadiansToDegrees(pitchRadians), MaximumPitchDegrees);
    attitude.rollDegrees = ScaleAndLimitAngle(RadiansToDegrees(bankRadians), MaximumRollDegrees);
    // Platform yaw comes from rudder deflection, not aircraft heading.
    attitude.yawDegrees = ScaleAndLimitAngle(-rudderDegrees, MaximumYawDegrees);
    return attitude;
}

MotionCommand CalculateMotion(
    const PlatformAttitude& attitude, const ActuatorValues& currentPositions) {
    constexpr float controlStepSeconds = 1.0f / MotionSettings::ControlRateHz;
    constexpr float maximumStep = MaximumStepPerSecond * controlStepSeconds;
    MotionCommand command;
    command.attitude = attitude;

    // Preserve the established geometry mapping: yaw about Z, negative platform
    // roll about Y, and platform pitch about X. Every leg shares this rotation.
    const auto rotation = rotation_matrix(
        static_cast<float>(attitude.yawDegrees),
        static_cast<float>(-attitude.rollDegrees),
        static_cast<float>(attitude.pitchDegrees));

    for (std::size_t index = 0; index < ActuatorCount; ++index) {
        const vec leg = startHeight + dot_product(rotation, platformLegs[index]) - baseLegs[index];
        const float geometricLength = leg.magnitude();
        // Convert geometric length to the controller's existing position reference.
        const float desiredLength = std::round(
            geometricLength - BaseLegLength + NeutralActuatorPosition);

        const float requestedDelta = desiredLength - currentPositions[index];
        const float limitedDelta = (std::max)(-maximumStep, (std::min)(requestedDelta, maximumStep));
        command.positions[index] = currentPositions[index] + limitedDelta;

        const float distance = std::fabs(limitedDelta);
        // Even a stationary actuator retains the existing minimum speed command.
        command.speeds[index] = (std::max)(
            MinimumSpeed,
            (std::min)(distance / controlStepSeconds, SpeedLimit));
    }

    return command;
}
