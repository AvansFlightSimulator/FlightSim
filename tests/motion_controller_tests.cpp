#include "MotionController.h"
#include "MotionCalculator.h"
#include "ControllerProtocol.h"
#include "DashboardModel.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace {
constexpr double Pi = 3.14159265358979323846;

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void CheckSimulatorInputFiltering() {
    DashboardModel dashboard;
    MotionController motion(dashboard);
    const auto start = std::chrono::steady_clock::time_point{};
    const double degreesToRadians = Pi / 180.0;

    motion.UpdateSimulatorInput(2.0 * degreesToRadians, 2.0 * degreesToRadians, 2.0,
        false, start);
    auto snapshot = dashboard.GetSnapshot();
    Require(std::fabs(snapshot.simulatorPitchDegrees - 2.0) < 1e-9
        && std::fabs(snapshot.simulatorRollDegrees - 2.0) < 1e-9,
        "Raw simulator diagnostics must remain unfiltered");
    Require(std::fabs(snapshot.pitchDegrees + 1.0) < 1e-9
        && std::fabs(snapshot.rollDegrees - 1.0) < 1e-9
        && std::fabs(snapshot.yawDegrees + 2.0 / 3.0) < 1e-9,
        "First simulator sample must initialize without easing from zero");

    motion.UpdateSimulatorInput(2.04 * degreesToRadians, 2.04 * degreesToRadians, 2.04,
        false, start + std::chrono::milliseconds(50));
    snapshot = dashboard.GetSnapshot();
    const double timeConstantSeconds =
        std::chrono::duration<double>(SimulatorFilterSettings::TimeConstant).count();
    const double alpha = 1.0 - std::exp(-0.05 / timeConstantSeconds);
    const double expectedFiltered = 2.0 + alpha * 0.04;
    Require(std::fabs(snapshot.simulatorPitchDegrees - 2.04) < 1e-9,
        "Raw simulator display must show the latest noisy sample");
    Require(std::fabs(snapshot.pitchDegrees + expectedFiltered / 2.0) < 1e-9
        && std::fabs(snapshot.rollDegrees - expectedFiltered / 2.0) < 1e-9
        && std::fabs(snapshot.yawDegrees + expectedFiltered / 3.0) < 1e-9,
        "Filtered live values must feed the existing mapping before geometry");

    const auto beforeInvalid = snapshot;
    motion.UpdateSimulatorInput(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0,
        false, start + std::chrono::milliseconds(100));
    snapshot = dashboard.GetSnapshot();
    Require(snapshot.simulatorPitchDegrees == beforeInvalid.simulatorPitchDegrees
        && snapshot.pitchDegrees == beforeInvalid.pitchDegrees,
        "Nonfinite live input must leave raw and filtered state unchanged");

    motion.ResetSimulatorInputFilter();
    motion.UpdateSimulatorInput(10.0 * degreesToRadians, -8.0 * degreesToRadians, 6.0,
        false, start + std::chrono::seconds(1));
    snapshot = dashboard.GetSnapshot();
    Require(std::fabs(snapshot.pitchDegrees + 5.0) < 1e-9
        && std::fabs(snapshot.rollDegrees + 4.0) < 1e-9
        && std::fabs(snapshot.yawDegrees + 2.0) < 1e-9,
        "Reset must make the next valid live sample initialize immediately");
}

void CheckManualControl() {
    DashboardModel dashboard;
    MotionController motion(dashboard);
    const ActuatorValues neutral{{200, 200, 200, 200, 200, 200}};
    std::string payload;
    const auto start = std::chrono::steady_clock::now();
    Require(!motion.TryGetLatestPayload(payload), "Startup must have no command");
    Require(!motion.ExecuteManualInput({20, -40, 12}), "Live mode must reject manual execution");
    motion.SetInputMode(InputMode::ManualAngles);
    Require(!motion.ExecuteManualInput({20, -40, 12}), "No client must reject execution");
    dashboard.SetClientConnected(true);
    Require(!motion.ExecuteManualInput({20, -40, 12}), "Missing feedback must reject execution");
    dashboard.UpdateFeedback(neutral);
    motion.TickManual(true, start);
    Require(!motion.TryGetLatestPayload(payload), "Selecting manual must not command a move");
    Require(motion.ExecuteManualInput({20, -40, 12}), "Valid manual inputs must execute");
    auto snapshot = dashboard.GetSnapshot();
    Require(!snapshot.simulatorConnected && !snapshot.simulatorAttitudeAvailable,
        "Manual execution must not fabricate simulator telemetry");
    Require(snapshot.manualInputAvailable && snapshot.manualInput.rudderDegrees == 12,
        "Applied raw manual inputs must remain visible");
    Require(std::fabs(snapshot.pitchDegrees + 10) < 1e-9
        && std::fabs(snapshot.rollDegrees + 20) < 1e-9
        && std::fabs(snapshot.yawDegrees + 4) < 1e-9,
        "Manual input must use live signs, halving, and extra rudder adjustment");
    motion.TickManual(false, start);
    Require(!motion.TryGetLatestPayload(payload), "Missing feedback must not generate a command");
    auto now = start + MotionSettings::ControlInterval;
    motion.TickManual(true, now);
    Require(motion.TryGetLatestPayload(payload), "Manual control must work without MSFS");
    const auto firstPayload = payload;
    const auto expectedManualCommand = CalculateMotion({-10, -20, -4});
    snapshot = dashboard.GetSnapshot();
    Require(snapshot.targetPositions == expectedManualCommand.positions,
        "Manual control must publish the final inverse-kinematics target immediately");
    Require(snapshot.speeds == ActuatorValues{{400, 400, 400, 400, 400, 400}},
        "Manual control must publish the Point-to-Point velocity limit");
    motion.TickManual(true, now + std::chrono::milliseconds(49));
    motion.TryGetLatestPayload(payload);
    Require(payload == firstPayload, "Calculations must not run sooner than 50 ms");
    const auto settledPayload = firstPayload;
    motion.UpdateSimulatorInput(0, 0, 0, true);
    motion.TryGetLatestPayload(payload);
    Require(payload == settledPayload, "Simulator samples must not overwrite manual commands");
    for (double invalid : {std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN()}) {
        Require(!motion.ExecuteManualInput({invalid, 0, 0}), "Nonfinite pitch must be rejected");
        Require(!motion.ExecuteManualInput({0, invalid, 0}), "Nonfinite roll must be rejected");
        Require(!motion.ExecuteManualInput({0, 0, invalid}), "Nonfinite rudder must be rejected");
    }
    Require(dashboard.GetSnapshot().manualInput.pitchDegrees == 20,
        "Rejected inputs must leave the applied target unchanged");
    dashboard.SetClientConnected(false);
    Require(!motion.ExecuteManualInput({0, 0, 0}), "Disconnected clients must reject new execution");
    motion.SetInputMode(InputMode::Simulator);
    Require(!motion.TryGetLatestPayload(payload), "Switching to MSFS must clear manual payload");
    Require(!dashboard.GetSnapshot().motionAvailable, "Source switch must clear target availability");
    motion.TickManual(true, now + MotionSettings::ControlInterval);
    Require(!motion.TryGetLatestPayload(payload), "Old manual input must not run in MSFS mode");
    motion.UpdateSimulatorInput(0, 0, 0, true);
    Require(motion.TryGetLatestPayload(payload), "MSFS input must work again after switching back");
    // Unity may serialize the sign-preserving mapping as -0.0; compare numeric
    // JSON values rather than treating signed zero as a protocol difference.
    Require(nlohmann::json::parse(payload) == nlohmann::json::parse(BuildCommandPayload(CalculateMotion({}))),
        "Live command behavior changed");
    motion.SetInputMode(InputMode::ManualAngles);
    Require(!motion.TryGetLatestPayload(payload), "Switching to manual must clear MSFS payload");
    motion.TickManual(true, now + MotionSettings::ControlInterval * 2);
    Require(!motion.TryGetLatestPayload(payload), "Reentering manual must require Execute again");
    dashboard.SetClientConnected(true);
    dashboard.UpdateFeedback(neutral);
    Require(motion.ExecuteManualInput({180, -180, 180}), "Large finite simulator inputs must be accepted");
    snapshot = dashboard.GetSnapshot();
    Require(snapshot.pitchDegrees == -30 && snapshot.rollDegrees == -30 && snapshot.yawDegrees == -30,
        "Manual simulator input must retain platform angle limits");
}

void CheckActuatorControl() {
    DashboardModel dashboard;
    MotionController motion(dashboard);
    const ActuatorValues feedback{{200, 200, 200, 200, 200, 200}};
    const ActuatorValues requested{{200, 300, 100, 240, 400, 0}};
    std::string payload;
    auto now = std::chrono::steady_clock::now();
    Require(!motion.ExecuteActuatorInput(requested), "Live mode must reject actuator execution");
    motion.SetInputMode(InputMode::ActuatorPositions);
    if (!MotionController::SupportsActuatorPositions()) {
        Require(motion.GetInputMode() == InputMode::Simulator, "Unity must not select unsupported position mode");
        Require(!motion.ExecuteActuatorInput(requested), "Unity must not publish fabricated orientation");
        return;
    }
    Require(!motion.ExecuteActuatorInput(requested), "Actuator execution must require a client");
    dashboard.SetClientConnected(true);
    Require(!motion.ExecuteActuatorInput(requested), "Actuator execution must require feedback");
    dashboard.UpdateFeedback(feedback);
    motion.TickManual(true, now);
    Require(!motion.TryGetLatestPayload(payload), "Entering position mode must not start motion");
    Require(!motion.ExecuteManualInput({20, 30, 10}), "Position mode must reject angle Execute");
    Require(motion.ExecuteActuatorInput(requested), "Six independent positions must be accepted");
    Require(dashboard.GetSnapshot().requestedPositions == requested, "Applied positions lost their actuator order");
    Require(!dashboard.GetSnapshot().motionAvailable, "Independent positions must not fabricate orientation");
    motion.TickManual(false, now);
    Require(!motion.TryGetLatestPayload(payload), "No feedback must not generate a position command");
    now += MotionSettings::ControlInterval;
    motion.TickManual(true, now);
    Require(motion.TryGetLatestPayload(payload), "Position mode must generate a PLC command without MSFS");
    auto snapshot = dashboard.GetSnapshot();
    Require(snapshot.targetPositions == requested,
        "Direct targets must bypass geometry and publish final positions in actuator order");
    Require(snapshot.speeds == ActuatorValues{{400, 400, 400, 400, 400, 400}},
        "Direct targets must use the Point-to-Point velocity limit");
    Require(nlohmann::json::parse(payload)
        == nlohmann::json::parse(BuildCommandPayload(CalculateActuatorMotion(requested))),
        "Published PLC payload must contain the final direct targets");
    Require(snapshot.actuatorCommandAvailable && !snapshot.motionAvailable,
        "Leg commands and orientation availability must be independent");
    const auto initialPayload = payload;
    motion.TickManual(true, now + std::chrono::milliseconds(49));
    motion.TryGetLatestPayload(payload);
    Require(payload == initialPayload, "Actuator calculations must respect the 50 ms interval");
    motion.UpdateSimulatorInput(1, 1, 1, true);
    motion.TryGetLatestPayload(payload);
    Require(payload == initialPayload, "Live samples must not overwrite actuator commands");
    // Feedback can show that the first move is still in progress. A new target
    // must replace it directly instead of taking another step from that feedback.
    const ActuatorValues inProgressFeedback{{220, 220, 180, 220, 220, 180}};
    const ActuatorValues changedTarget{{350, 250, 150, 260, 450, 50}};
    dashboard.UpdateFeedback(inProgressFeedback);
    Require(motion.ExecuteActuatorInput(changedTarget), "A target update must be accepted during motion");
    now += MotionSettings::ControlInterval;
    motion.TickManual(true, now);
    snapshot = dashboard.GetSnapshot();
    Require(snapshot.currentPositions == inProgressFeedback,
        "In-progress feedback must remain available for monitoring");
    Require(snapshot.targetPositions == changedTarget,
        "A changed target must not be generated as a feedback-relative intermediate step");
    motion.TryGetLatestPayload(payload);
    Require(nlohmann::json::parse(payload)
        == nlohmann::json::parse(BuildCommandPayload(CalculateActuatorMotion(changedTarget))),
        "Published payload must replace an unfinished move with the new final target");
    for (std::size_t index = 0; index < ActuatorCount; ++index) {
        for (float invalid : {-1.0f, 1000.0f, std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()}) {
            auto badInput = requested;
            badInput[index] = invalid;
            Require(!motion.ExecuteActuatorInput(badInput), "Invalid position must reject all six inputs atomically");
            Require(dashboard.GetSnapshot().requestedPositions == changedTarget,
                "Rejected input changed applied values");
        }
    }
    Require(motion.ExecuteActuatorInput({{0, 999, 200.4f, 300.6f, 100.5f, 400}}), "Protocol boundaries must be accepted");
    Require(dashboard.GetSnapshot().requestedPositions == ActuatorValues{{0, 999, 200, 301, 101, 400}},
        "Applied targets must reflect PLC whole-position rounding");
    now += MotionSettings::ControlInterval;
    motion.TickManual(true, now);
    Require(dashboard.GetSnapshot().targetPositions == ActuatorValues{{0, 999, 200, 301, 101, 400}},
        "Minimum and maximum controller positions must be sent as final targets");
    dashboard.SetClientConnected(false);
    Require(!motion.ExecuteActuatorInput(requested), "Disconnect must block new position execution");
    motion.SetInputMode(InputMode::ManualAngles);
    Require(!motion.TryGetLatestPayload(payload), "Angle mode must clear the direct command");
    Require(!dashboard.GetSnapshot().actuatorInputAvailable && !dashboard.GetSnapshot().actuatorCommandAvailable,
        "Mode change must clear applied position availability");
    Require(!motion.ExecuteActuatorInput(requested), "Angle mode must reject actuator Execute");
    motion.SetInputMode(InputMode::ActuatorPositions);
    motion.TickManual(true, now + MotionSettings::ControlInterval);
    Require(!motion.TryGetLatestPayload(payload), "Returning to position mode must require Execute again");
    motion.SetInputMode(InputMode::Simulator);
    motion.UpdateSimulatorInput(0, 0, 0, true);
    Require(motion.TryGetLatestPayload(payload), "Simulator mode must resume after direct control");
    Require(nlohmann::json::parse(payload) == nlohmann::json::parse(BuildCommandPayload(CalculateMotion({}))),
        "Live behavior changed after direct mode");
}
}

int main() {
    try {
        CheckSimulatorInputFiltering();
        CheckManualControl();
        CheckActuatorControl();
        std::cout << "Live filtering, final targets, scheduling, mapping, and feedback checks passed.\n";
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
