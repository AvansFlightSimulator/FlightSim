#pragma once

#include "BridgeTypes.h"

#include <chrono>

namespace MotionSettings {
// Calculation and output share a nominal 20 Hz schedule. This is not real time.
constexpr int ControlRateHz = 20;
constexpr auto ControlInterval = std::chrono::milliseconds(1000 / ControlRateHz);
// Final targets and accepted direct input match the PLC's three-digit position
// representation. These are protocol bounds, not physical actuator travel claims.
constexpr float MinimumActuatorInput = 0.0f;
constexpr float MaximumActuatorInput = 999.0f;
// Velocity limit passed to the drive's Point-to-Point command. The drive owns
// acceleration, deceleration, jerk, and trajectory generation.
constexpr float PointToPointVelocityLimit = 400.0f;
}

double RadiansToDegrees(double radians);

// MSFS pitch/bank arrive in radians; rudder deflection arrives in degrees.
// Preserves the existing sign changes, halving, and +/-30-degree limits.
PlatformAttitude CalculatePlatformAttitude(
    double pitchRadians, double bankRadians, double rudderDegrees);

// Pure calculation: no sockets, simulator SDK, dashboard state, or side effects.
// Produces final position targets for the drive's Point-to-Point trajectory generator.
MotionCommand CalculateMotion(const PlatformAttitude& attitude);

// Clamp explicit leg targets to the controller range and apply the configured
// Point-to-Point velocity limit. This does not calculate platform orientation
// or validate whether an arbitrary combination is mechanically reachable.
MotionCommand CalculateActuatorMotion(const ActuatorValues& desiredPositions);
