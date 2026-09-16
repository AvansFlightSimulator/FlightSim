#pragma once

#include <array>
#include <cstddef>

// Index 0..5 always follows the physical actuator order in MotionCalculator.cpp.
// Positions and speeds use the existing controller units; these are not angles.
constexpr std::size_t ActuatorCount = 6;
using ActuatorValues = std::array<float, ActuatorCount>;

// Calculated platform targets in degrees, not measured platform orientation.
struct PlatformAttitude {
    double pitchDegrees = 0.0;
    double rollDegrees = 0.0;
    double yawDegrees = 0.0;
};

struct MotionCommand {
    PlatformAttitude attitude;
    ActuatorValues positions{};
    ActuatorValues speeds{};
};
