#include "TelemetryStore.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main() {
    try {
        TelemetryFrame frame;
        std::string error;
        const std::string send = R"({"positions":["001","002","003","004","005","006"],"speeds":[2,3,4,5,6,7]})";
        Check(TelemetryStore::Decode("send", send, frame, error), "PLC strings");
        Check(frame.values[0][5] == 6 && frame.values[1][0] == 2, "Wire values and actuator order");
        Check(std::isnan(frame.values[2][0]), "Never fabricate drive target");
        Check(TelemetryStore::Decode("feedback", R"({"currentPositions":[1,2,3,4,5,6]})", frame, error), "Legacy feedback");
        Check(std::isnan(frame.values[3][0]), "Legacy feedback has no validated drive telemetry");
        const std::string feedback = R"({"currentPositions":[1,2,3,4,5,6],"diagnostics":{"version":1,"actualPositions":[1,2,3,4,5,6],"actualVelocities":[-2,null,3,4,5,6],"followingErrors":[-0.1,0,0,0,0,0]}})";
        Check(TelemetryStore::Decode("feedback", feedback, frame, error), "Diagnostic decode");
        Check(frame.values[4][0] == -2 && std::isnan(frame.values[4][1]), "Signed velocity and missing samples");
        Check(!TelemetryStore::Decode("feedback", R"({"diagnostics":{"version":1,"actualPositions":[1,2]}})", frame, error), "Reject partial arrays");
        Check(!TelemetryStore::Decode("feedback", R"({"diagnostics":{"version":8}})", frame, error), "Reject unknown schema");
        Check(!TelemetryStore::Decode("send", R"({"positions":["1oops",0,0,0,0,0]})", frame, error), "Reject invalid strings");
        const char* file = "telemetry-test-session.jsonl";
        {
            TelemetryStore store(file);
            store.Record("connect", "", 1);
            store.Record("send", send, 1);
            store.Record("feedback", feedback, 1);
            store.Record("disconnect", "", 1);
            store.Record("connect", "", 2);
            Check(store.Recent().size() == 5, "Live history");
        }
        std::vector<TelemetryFrame> replay;
        Check(TelemetryStore::Load(file, replay, error), "Replay session");
        Check(replay.size() == 5 && replay.back().connection == 2, "Flush on shutdown, connection boundaries");
        Check(replay[1].payload == send && replay[2].values[4][0] == -2, "Raw packet and numeric roundtrip");
        {
            std::ofstream append(file, std::ios::app); append << "{partial";
        }
        Check(TelemetryStore::Load(file, replay, error) && replay.size() == 5, "Recover truncated final event");
        std::remove(file);
        Check(!TelemetryStore::Load("no-such-recording.jsonl", replay, error), "Missing recording error");
        Check(replay.size() == 5, "Failed load preserves current view");
        std::cout << "Telemetry storage tests passed\n";
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
