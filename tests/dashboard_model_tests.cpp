#include "DashboardModel.h"

#include <array>
#include <cassert>
#include <iostream>

int main() {
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
