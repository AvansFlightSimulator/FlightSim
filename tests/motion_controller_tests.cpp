#include "MotionController.h"
#include "MotionCalculator.h"
#include "ControllerProtocol.h"
#include "DashboardModel.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

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
    motion.SetManualMode(true);
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
    motion.SetManualMode(false);
    Require(!motion.TryGetLatestPayload(payload), "Switching to MSFS must clear manual payload");
    Require(!dashboard.GetSnapshot().motionAvailable, "Source switch must clear target availability");
    motion.TickManual(true, neutral, now + MotionSettings::ControlInterval);
    Require(!motion.TryGetLatestPayload(payload), "Old manual input must not run in MSFS mode");
    motion.UpdateSimulatorInput(0, 0, 0, true, neutral);
    Require(motion.TryGetLatestPayload(payload), "MSFS input must work again after switching back");
    Require(payload == BuildCommandPayload(CalculateMotion({}, neutral)), "Live command behavior changed");
    motion.SetManualMode(true);
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
}

int main() {
    try {
        CheckManualControl();
        std::cout << "Manual source, scheduling, mapping, and feedback checks passed.\n";
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
