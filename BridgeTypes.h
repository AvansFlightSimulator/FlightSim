#pragma once

#include <array>
#include <cstddef>

// Index 0..5 always follows the physical actuator order in MotionCalculator.cpp.
// Positions and speeds use the existing controller units; these are not angles.
constexpr std::size_t ActuatorCount = 6;
using ActuatorValues = std::array<float, ActuatorCount>;

enum class InputMode {
    Simulator,
    ManualAngles,
    ActuatorPositions
};

// Manual input uses the raw MSFS signs and degrees, including rudder (not heading).
struct SimulatorInput {
    double pitchDegrees = 0.0;
    double rollDegrees = 0.0;
    double rudderDegrees = 0.0;
};

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
