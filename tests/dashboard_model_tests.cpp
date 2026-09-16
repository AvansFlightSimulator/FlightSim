#include "DashboardModel.h"

#include <array>
#include <cassert>
#include <iostream>

int main() {
    // Raw simulator telemetry must work without a client and survive motion updates.
    DashboardModel rawModel;
    assert(!rawModel.GetSnapshot().simulatorAttitudeAvailable);
    assert(!rawModel.GetSnapshot().simulatorRudderAvailable);
    rawModel.SetSimulatorConnected(true);
    rawModel.UpdateSimulatorRudder(-12.5);
    assert(!rawModel.GetSnapshot().simulatorAttitudeAvailable);
    const double rollSamples[] = { 0.0, -60.0, -80.0, -179.9, 179.9, 80.0, 60.0, 0.0 };
    const std::array<float, 6> noMotion{};
    for (double roll : rollSamples) {
        rawModel.UpdateSimulatorAttitude(-45.0, roll);
        rawModel.UpdateOrientation(22.5, 30.0, 6.25);
        rawModel.UpdateMotion(22.5, 30.0, 6.25, noMotion, noMotion);
        const DashboardSnapshot raw = rawModel.GetSnapshot();
        assert(!raw.clientConnected && !raw.positionFeedback);
        assert(raw.simulatorAttitudeAvailable && raw.simulatorRudderAvailable);
        assert(raw.simulatorPitchDegrees == -45.0);
        assert(raw.simulatorRollDegrees == roll);
        assert(raw.simulatorRudderDegrees == -12.5);
        assert(raw.pitchDegrees == 22.5 && raw.rollDegrees == 30.0 && raw.yawDegrees == 6.25);
    }
    rawModel.SetSimulatorConnected(false);
    assert(!rawModel.GetSnapshot().simulatorAttitudeAvailable);
    assert(!rawModel.GetSnapshot().simulatorRudderAvailable);
    rawModel.SetSimulatorConnected(true);
    assert(!rawModel.GetSnapshot().simulatorAttitudeAvailable);
    assert(!rawModel.GetSnapshot().simulatorRudderAvailable);
    rawModel.UpdateSimulatorAttitude(10.0, -90.0);
    assert(rawModel.GetSnapshot().simulatorAttitudeAvailable);
    assert(!rawModel.GetSnapshot().simulatorRudderAvailable);
    rawModel.UpdateSimulatorRudder(2.0);
    assert(rawModel.GetSnapshot().simulatorRudderAvailable);

    DashboardModel model;
    model.SetTcpListening(true);
    model.SetClientConnected(true);
    model.SetSimulatorConnected(true);

    const std::array<float, 6> current = {{ 101.0f, 102.0f, 103.0f, 104.0f, 105.0f, 106.0f }};
    const std::array<float, 6> target = {{ 111.0f, 112.0f, 113.0f, 114.0f, 115.0f, 116.0f }};
    const std::array<float, 6> speeds = {{ 21.0f, 22.0f, 23.0f, 24.0f, 25.0f, 26.0f }};

    model.UpdateFeedback(current);
    model.UpdateOrientation(1.0, 2.0, 3.0);
    model.UpdateMotion(4.0, -5.0, 6.0, target, speeds);
    model.RecordCommandSent();
    model.AddEvent("Telemetry test event", DashboardEventLevel::Warning);

    DashboardSnapshot snapshot = model.GetSnapshot();
    assert(snapshot.tcpListening);
    assert(snapshot.clientConnected);
    assert(snapshot.simulatorConnected);
    assert(snapshot.positionFeedback);
    assert(snapshot.motionAvailable);
    assert(snapshot.currentPositions == current);
    assert(snapshot.targetPositions == target);
    assert(snapshot.speeds == speeds);
    assert(snapshot.pitchDegrees == 4.0);
    assert(snapshot.rollDegrees == -5.0);
    assert(snapshot.yawDegrees == 6.0);
    assert(snapshot.receivedMessages == 1);
    assert(snapshot.sentMessages == 1);
    assert(snapshot.lastFeedbackMilliseconds >= 0);
    assert(snapshot.lastSendMilliseconds >= 0);
    assert(snapshot.events.size() == 1);
    assert(snapshot.events.front().level == DashboardEventLevel::Warning);

    model.SetClientConnected(false);
    snapshot = model.GetSnapshot();
    assert(!snapshot.clientConnected);
    assert(!snapshot.positionFeedback);

    std::cout << "Dashboard model checks passed." << std::endl;
    return 0;
}
