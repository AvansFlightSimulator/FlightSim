#pragma once

#include "BridgeTypes.h"

#include <chrono>

namespace MotionSettings {
// Calculation and output share a nominal 20 Hz schedule. This is not real time.
constexpr int ControlRateHz = 20;
constexpr auto ControlInterval = std::chrono::milliseconds(1000 / ControlRateHz);
// Accepted direct input matches the PLC's three-digit position representation.
// These are protocol bounds, not a claim about physical actuator travel.
constexpr float MinimumActuatorInput = 0.0f;
constexpr float MaximumActuatorInput = 999.0f;
}

double RadiansToDegrees(double radians);

// MSFS pitch/bank arrive in radians; rudder deflection arrives in degrees.
// Preserves the existing sign changes, attitude scaling and +/-30-degree limits.
PlatformAttitude CalculatePlatformAttitude(
    double pitchRadians, double bankRadians, double rudderDegrees);

// Pure calculation: no sockets, simulator SDK, dashboard state, or side effects.
// PP/Unity use the most recent feedback for step limits. CSP returns final
// fractional endpoints and leaves trajectory generation to the PLC.
MotionCommand CalculateMotion(
    const PlatformAttitude& attitude, const ActuatorValues& currentPositions);

// PP/Unity apply feedback-relative step/speed limits to explicit leg targets.
// CSP passes the endpoints through with unused speeds set to zero.
// This does not calculate platform orientation or validate physical reachability.
MotionCommand CalculateActuatorMotion(
    const ActuatorValues& desiredPositions, const ActuatorValues& currentPositions);
