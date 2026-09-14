#include "calculate_legs.h"

#include <array>
#include <cmath>
#include <iostream>

namespace {
constexpr std::size_t ActuatorCount = 6;

std::array<vec, ActuatorCount> BaseLegs = {{
    {177.53f, 723.37f, 0.0f},
    {-177.53f, 723.37f, 0.0f},
    {-715.23f, -207.94f, 0.0f},
    {-537.69f, -515.43f, 0.0f},
    {537.69f, -515.43f, 0.0f},
    {715.23f, -207.94f, 0.0f}
}};

std::array<vec, ActuatorCount> PlatformLegs = {{
    {360.59f, 346.0f, 0.0f},
    {-360.59f, 346.0f, 0.0f},
    {-480.12f, 139.59f, 0.0f},
    {-119.17f, -485.59f, 0.0f},
    {119.17f, -485.59f, 0.0f},
    {480.12f, 139.59f, 0.0f}
}};

bool IsApproximately(float left, float right, float tolerance = 0.01f) {
    return std::fabs(left - right) <= tolerance;
}

bool ValidatePose(float yaw, float roll, float pitch) {
    vec startHeight{ 0.0f, 0.0f, 1079.0f };
    for (std::size_t index = 0; index < ActuatorCount; ++index) {
        const float length = compute_li_length(
            startHeight,
            yaw,
            roll,
            pitch,
            PlatformLegs[index],
            BaseLegs[index]);
        if (!std::isfinite(length) || length <= 0.0f) {
            std::cerr << "Invalid length for actuator " << index
                      << " at pose " << yaw << ", " << roll << ", " << pitch << '\n';
            return false;
        }
    }
    return true;
}
}

int main() {
    vec startHeight{ 0.0f, 0.0f, 1079.0f };
    std::array<float, ActuatorCount> neutralLengths{};
    for (std::size_t index = 0; index < ActuatorCount; ++index) {
        neutralLengths[index] = compute_li_length(
            startHeight,
            0.0f,
            0.0f,
            0.0f,
            PlatformLegs[index],
            BaseLegs[index]);
    }

    if (!IsApproximately(neutralLengths[0], neutralLengths[1])
        || !IsApproximately(neutralLengths[2], neutralLengths[3])
        || !IsApproximately(neutralLengths[3], neutralLengths[4])
        || !IsApproximately(neutralLengths[4], neutralLengths[5])) {
        std::cerr << "Mirrored actuator pairs are not symmetric at the neutral pose.\n";
        return 1;
    }

    const std::array<std::array<float, 3>, 9> boundaryPoses = {{
        {{0.0f, 0.0f, 0.0f}},
        {{30.0f, 0.0f, 0.0f}},
        {{-30.0f, 0.0f, 0.0f}},
        {{0.0f, 30.0f, 0.0f}},
        {{0.0f, -30.0f, 0.0f}},
        {{0.0f, 0.0f, 30.0f}},
        {{0.0f, 0.0f, -30.0f}},
        {{30.0f, 30.0f, 30.0f}},
        {{-30.0f, -30.0f, -30.0f}}
    }};

    for (const auto& pose : boundaryPoses) {
        if (!ValidatePose(pose[0], pose[1], pose[2])) {
            return 1;
        }
    }

    std::cout << "All six actuators passed neutral and boundary-pose checks.\n";
    return 0;
}
