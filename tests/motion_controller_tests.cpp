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
void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
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
    motion.TickManual(true, neutral, start);
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
    motion.TickManual(false, neutral, start);
    Require(!motion.TryGetLatestPayload(payload), "Missing feedback must not generate a command");
    auto now = start + MotionSettings::ControlInterval;
    motion.TickManual(true, neutral, now);
    Require(motion.TryGetLatestPayload(payload), "Manual control must work without MSFS");
    const auto firstPayload = payload;
    auto positions = dashboard.GetSnapshot().targetPositions;
    motion.TickManual(true, positions, now + std::chrono::milliseconds(49));
    motion.TryGetLatestPayload(payload);
    Require(payload == firstPayload, "Calculations must not run sooner than 50 ms");
    // Feed each limited command back as the next measured position. A one-shot
    // implementation would stop after its first 20-unit step and fail this check.
    for (int step = 0; step < 30; ++step) {
        now += MotionSettings::ControlInterval;
        const auto previous = positions;
        motion.TickManual(true, positions, now);
        snapshot = dashboard.GetSnapshot();
        positions = snapshot.targetPositions;
        for (std::size_t index = 0; index < ActuatorCount; ++index) {
            Require(std::fabs(positions[index] - previous[index]) <= 20,
                "Manual motion must retain feedback-relative step limits");
            Require(snapshot.speeds[index] >= 2 && snapshot.speeds[index] <= 500,
                "Manual motion must retain speed limits");
        }
    }
    Require(positions != neutral, "The active manual target must progress beyond the initial pose");
    Require(CalculateMotion({-10, -20, -4}, positions).positions == positions,
        "Repeated manual calculations must reach the geometric target");
    motion.TryGetLatestPayload(payload);
    const auto settledPayload = payload;
    motion.UpdateSimulatorInput(0, 0, 0, true, neutral);
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
    motion.TickManual(true, neutral, now + MotionSettings::ControlInterval);
    Require(!motion.TryGetLatestPayload(payload), "Old manual input must not run in MSFS mode");
    motion.UpdateSimulatorInput(0, 0, 0, true, neutral);
    Require(motion.TryGetLatestPayload(payload), "MSFS input must work again after switching back");
    // Unity may serialize the sign-preserving mapping as -0.0; compare numeric
    // JSON values rather than treating signed zero as a protocol difference.
    Require(nlohmann::json::parse(payload) == nlohmann::json::parse(BuildCommandPayload(CalculateMotion({}, neutral))),
        "Live command behavior changed");
    motion.SetInputMode(InputMode::ManualAngles);
    Require(!motion.TryGetLatestPayload(payload), "Switching to manual must clear MSFS payload");
    motion.TickManual(true, neutral, now + MotionSettings::ControlInterval * 2);
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
    motion.TickManual(true, feedback, now);
    Require(!motion.TryGetLatestPayload(payload), "Entering position mode must not start motion");
    Require(!motion.ExecuteManualInput({20, 30, 10}), "Position mode must reject angle Execute");
    Require(motion.ExecuteActuatorInput(requested), "Six independent positions must be accepted");
    Require(dashboard.GetSnapshot().requestedPositions == requested, "Applied positions lost their actuator order");
    Require(!dashboard.GetSnapshot().motionAvailable, "Independent positions must not fabricate orientation");
    motion.TickManual(false, feedback, now);
    Require(!motion.TryGetLatestPayload(payload), "No feedback must not generate a position command");
    now += MotionSettings::ControlInterval;
    motion.TickManual(true, feedback, now);
    Require(motion.TryGetLatestPayload(payload), "Position mode must generate a PLC command without MSFS");
    auto snapshot = dashboard.GetSnapshot();
    Require(snapshot.targetPositions == ActuatorValues{{200, 220, 180, 220, 220, 180}},
        "Direct targets must bypass geometry and retain step limits/order");
    Require(snapshot.speeds == ActuatorValues{{2, 400, 400, 400, 400, 400}}, "Direct target speeds changed");
    Require(snapshot.actuatorCommandAvailable && !snapshot.motionAvailable,
        "Leg commands and orientation availability must be independent");
    const auto initialPayload = payload;
    motion.TickManual(true, snapshot.targetPositions, now + std::chrono::milliseconds(49));
    motion.TryGetLatestPayload(payload);
    Require(payload == initialPayload, "Actuator calculations must respect the 50 ms interval");
    motion.UpdateSimulatorInput(1, 1, 1, true, feedback);
    motion.TryGetLatestPayload(payload);
    Require(payload == initialPayload, "Live samples must not overwrite actuator commands");
    for (int step = 0; step < 30; ++step) {
        const auto previous = snapshot.targetPositions;
        now += MotionSettings::ControlInterval;
        motion.TickManual(true, previous, now);
        snapshot = dashboard.GetSnapshot();
        for (std::size_t index = 0; index < ActuatorCount; ++index) {
            Require(std::fabs(snapshot.targetPositions[index] - previous[index]) <= 20,
                "Direct motion exceeded existing feedback-relative step limit");
        }
    }
    Require(snapshot.targetPositions == requested, "Direct mode must converge to all six absolute positions");
    Require(snapshot.speeds == ActuatorValues{{2, 2, 2, 2, 2, 2}}, "Settled direct targets retain minimum speed");
    for (std::size_t index = 0; index < ActuatorCount; ++index) {
        for (float invalid : {-1.0f, 1000.0f, std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()}) {
            auto badInput = requested;
            badInput[index] = invalid;
            Require(!motion.ExecuteActuatorInput(badInput), "Invalid position must reject all six inputs atomically");
            Require(dashboard.GetSnapshot().requestedPositions == requested, "Rejected input changed applied values");
        }
    }
    Require(motion.ExecuteActuatorInput({{0, 999, 200.4f, 300.6f, 100.5f, 400}}), "Protocol boundaries must be accepted");
    Require(dashboard.GetSnapshot().requestedPositions == ActuatorValues{{0, 999, 200, 301, 101, 400}},
        "Applied targets must reflect PLC whole-position rounding");
    dashboard.SetClientConnected(false);
    Require(!motion.ExecuteActuatorInput(requested), "Disconnect must block new position execution");
    motion.SetInputMode(InputMode::ManualAngles);
    Require(!motion.TryGetLatestPayload(payload), "Angle mode must clear the direct command");
    Require(!dashboard.GetSnapshot().actuatorInputAvailable && !dashboard.GetSnapshot().actuatorCommandAvailable,
        "Mode change must clear applied position availability");
    Require(!motion.ExecuteActuatorInput(requested), "Angle mode must reject actuator Execute");
    motion.SetInputMode(InputMode::ActuatorPositions);
    motion.TickManual(true, feedback, now + MotionSettings::ControlInterval);
    Require(!motion.TryGetLatestPayload(payload), "Returning to position mode must require Execute again");
    motion.SetInputMode(InputMode::Simulator);
    motion.UpdateSimulatorInput(0, 0, 0, true, feedback);
    Require(motion.TryGetLatestPayload(payload), "Simulator mode must resume after direct control");
    Require(nlohmann::json::parse(payload) == nlohmann::json::parse(BuildCommandPayload(CalculateMotion({}, feedback))),
        "Live behavior changed after direct mode");
}
}

int main() {
    try {
        CheckManualControl();
        CheckActuatorControl();
        std::cout << "Manual source, scheduling, mapping, and feedback checks passed.\n";
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
