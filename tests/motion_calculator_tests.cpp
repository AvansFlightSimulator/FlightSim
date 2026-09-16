#include "MotionCalculator.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
constexpr double Pi = 3.14159265358979323846;

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool Near(double actual, double expected) {
    return std::fabs(actual - expected) < 0.00001;
}

void CheckAttitudeMapping() {
    const auto mapped = CalculatePlatformAttitude(20 * Pi / 180, -40 * Pi / 180, 12);
    Require(Near(mapped.pitchDegrees, -10), "Pitch must be negated and halved");
    Require(Near(mapped.rollDegrees, -20), "Bank must retain its sign and be halved");
    Require(Near(mapped.yawDegrees, -6), "Yaw must use negated, halved rudder");

    // These preserve CURRENT behavior, including the lack of an inverted-return
    // curve. Update only when a separately agreed motion change is implemented.
    for (double degrees : {60.0, 80.0, 180.0}) {
        const auto positive = CalculatePlatformAttitude(degrees * Pi / 180, degrees * Pi / 180, degrees);
        const auto negative = CalculatePlatformAttitude(-degrees * Pi / 180, -degrees * Pi / 180, -degrees);
        Require(Near(positive.pitchDegrees, -30) && Near(positive.rollDegrees, 30)
            && Near(positive.yawDegrees, -30), "Positive inputs must saturate at current limits");
        Require(Near(negative.pitchDegrees, 30) && Near(negative.rollDegrees, -30)
            && Near(negative.yawDegrees, 30), "Negative inputs must saturate at current limits");
    }
}

void CheckActuatorCommands() {
    const ActuatorValues neutral{{200, 200, 200, 200, 200, 200}};
    const auto stationary = CalculateMotion({}, neutral);
    Require(stationary.positions == neutral, "Neutral platform must remain at neutral positions");
    Require(stationary.speeds == ActuatorValues{{2, 2, 2, 2, 2, 2}}, "Stationary speed must retain minimum 2");

    // Captured from the pre-refactor implementation. An asymmetric pose detects
    // changed signs, swapped axes, or reordered actuators even without hardware.
    const auto pose = CalculatePlatformAttitude(0.01, -0.02, 0.5);
    const auto smallMove = CalculateMotion(pose, neutral);
    Require(smallMove.positions == ActuatorValues{{197, 202, 206, 204, 203, 195}}, "Small-pose positions changed");
    Require(smallMove.speeds == ActuatorValues{{60, 40, 120, 80, 60, 100}}, "Small-pose speeds changed");

    const ActuatorValues displaced{{0, 400, 199.75f, 200.25f, 100, 300}};
    const auto returnToNeutral = CalculateMotion({}, displaced);
    Require(returnToNeutral.positions == ActuatorValues{{20, 380, 200, 200, 120, 280}}, "Step limit must be relative to feedback");
    Require(returnToNeutral.speeds == ActuatorValues{{400, 400, 5, 5, 400, 400}}, "Speed must match the limited step over 50 ms");
}
}

int main() {
    try {
        CheckAttitudeMapping();
        CheckActuatorCommands();
        std::cout << "Motion mapping and actuator regression checks passed.\n";
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
