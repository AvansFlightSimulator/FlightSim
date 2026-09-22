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
    const ActuatorValues pointToPointVelocity{{400, 400, 400, 400, 400, 400}};
    // The existing geometric calibration rounds a level pose to 201, even though
    // the controller reference constant is 200. Preserve that observed behavior.
    const ActuatorValues levelTargets{{201, 201, 201, 201, 201, 201}};
    const auto levelMove = CalculateMotion({});
    Require(levelMove.positions == levelTargets, "Level-pose calibration changed");
    Require(levelMove.speeds == pointToPointVelocity,
        "Geometry commands must use the Point-to-Point velocity limit");

    // Captured from the pre-refactor implementation. An asymmetric pose detects
    // changed signs, swapped axes, or reordered actuators even without hardware.
    const auto pose = CalculatePlatformAttitude(0.01, -0.02, 0.5);
    const auto smallMove = CalculateMotion(pose);
    Require(smallMove.positions == ActuatorValues{{197, 202, 206, 204, 203, 195}}, "Small-pose positions changed");
    Require(smallMove.speeds == pointToPointVelocity, "Pose commands must use the velocity limit");

    // Final targets must be sent directly in both directions, including no-op
    // targets and the two controller boundaries.
    const auto direct = CalculateActuatorMotion({{300, 100, 200, 0, 999, 250}});
    Require(direct.positions == ActuatorValues{{300, 100, 200, 0, 999, 250}},
        "Actuator commands must contain final targets instead of intermediate steps");
    Require(direct.speeds == pointToPointVelocity,
        "Direct targets must use the Point-to-Point velocity limit");

    const auto outsideRange = CalculateActuatorMotion({{-1, 1000, -100, 1100, 0, 999}});
    Require(outsideRange.positions == ActuatorValues{{0, 999, 0, 999, 0, 999}},
        "Actuator targets must retain the existing 0..999 controller limits");

    const auto firstTarget = CalculateActuatorMotion({{300, 300, 300, 300, 300, 300}});
    const auto changedTarget = CalculateActuatorMotion({{350, 350, 350, 350, 350, 350}});
    Require(firstTarget.positions == ActuatorValues{{300, 300, 300, 300, 300, 300}}
        && changedTarget.positions == ActuatorValues{{350, 350, 350, 350, 350, 350}},
        "A changed target must replace the previous target without trajectory interpolation");
}
}

int main() {
    try {
        CheckAttitudeMapping();
        CheckActuatorCommands();
        std::cout << "Motion mapping and Point-to-Point target checks passed.\n";
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
