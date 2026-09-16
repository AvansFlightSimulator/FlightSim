#include "ControllerProtocol.h"
#include "BuildMode.h"

#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

const std::string Feedback = "{\"currentPositions\":[100,200,300,400,500,600]}";

void CheckCommandFormats() {
    MotionCommand command;
    command.attitude = {1, -2, 3};
    command.positions = {{-1, 0, 4.5f, 99.5f, 999, 1000}};
    command.speeds = {{2, 5, 10, 99, 100, 500}};
    const auto plcStrings = BuildPlcPayload(command, true);
    Require(plcStrings == "{\"positions\":[\"000\",\"000\",\"005\",\"100\",\"999\",\"999\"],\"speeds\":[\"002\",\"005\",\"010\",\"099\",\"100\",\"500\"]}",
        "PLC string format, order, or rounding changed");
    const auto plcNumbers = nlohmann::json::parse(BuildPlcPayload(command, false));
    Require(plcNumbers.size() == 2 && plcNumbers["positions"] == nlohmann::json::array({-1, 0, 5, 100, 999, 1000}),
        "PLC numeric format must not clamp to three digits");
    const auto unityText = BuildUnityPayload(command);
    const auto unity = nlohmann::json::parse(unityText);
    Require(unity.size() == 4 && unity["positions"] == plcNumbers["positions"]
        && unity["legs"] == unity["positions"] && unity["speeds"] == plcNumbers["speeds"], "Unity actuator fields changed");
    Require(unity["orientation"] == nlohmann::json({{"pitch", 1}, {"roll", -2}, {"yaw", 3}}), "Unity orientation axes changed");
    Require(plcStrings.back() == '}' && unityText.back() == '}', "Newline framing belongs to TCPServer");
#ifdef TARGET_PLC
    Require(BuildCommandPayload(command) == BuildPlcPayload(command, PLC_VALUES_ARE_STRINGS), "Wrong selected target");
#else
    Require(BuildCommandPayload(command) == unityText, "Wrong selected target");
#endif
}

void CheckFeedbackValidation() {
    ActuatorValues positions{};
    std::string error;
    Require(TryParseFeedback(Feedback, positions, error), "Valid feedback rejected");
    const ActuatorValues expected{{100, 200, 300, 400, 500, 600}};
    Require(positions == expected && error.empty(), "Valid feedback values changed");
    const char* rejected[] = {
        "invalid JSON", "null", "[]", "{}", "{\"currentPositions\":1}",
        "{\"currentPositions\":[1,2,3,4,5]}", "{\"currentPositions\":[1,2,3,4,5,6,7]}",
        "{\"currentPositions\":[1,2,3,4,5,\"6\"]}", "{\"currentPositions\":[1,2,3,4,5,null]}",
        "{\"currentPositions\":[1,2,3,4,5,true]}"
    };
    for (const auto* message : rejected) {
        Require(!TryParseFeedback(message, positions, error), "Invalid feedback accepted");
        Require(positions == expected && !error.empty(), "Rejected feedback must preserve the entire previous sample");
    }
}

void CheckStreamFraming() {
    // Exercise every possible split point, including a complete legacy object
    // arriving before its optional newline in the next TCP read.
    const std::string framed = Feedback + '\n';
    for (std::size_t split = 1; split < framed.size(); ++split) {
        FeedbackStream stream;
        auto first = stream.Append(framed.data(), split);
        auto second = stream.Append(framed.data() + split, framed.size() - split);
        first.messages.insert(first.messages.end(), second.messages.begin(), second.messages.end());
        Require(first.messages == std::vector<std::string>{Feedback}, "Fragmented feedback lost or duplicated");
    }
    FeedbackStream stream;
    const std::string combined = "\n\r\n" + Feedback + "\r\n" + Feedback + '\n' + Feedback.substr(0, 10);
    auto batch = stream.Append(combined.data(), combined.size());
    Require(batch.messages == std::vector<std::string>({Feedback, Feedback}), "Combined CRLF messages changed");
    batch = stream.Append(Feedback.data() + 10, Feedback.size() - 10);
    Require(batch.messages == std::vector<std::string>{Feedback}, "Partial remainder or legacy feedback lost");

    stream.Append(Feedback.data(), 10);
    stream.Clear(); // A new connection must not inherit partial bytes.
    batch = stream.Append(Feedback.data(), Feedback.size());
    Require(batch.messages == std::vector<std::string>{Feedback}, "Stream reset left bytes behind");

    const std::string oversized(64 * 1024 + 1, 'x');
    batch = stream.Append(oversized.data(), oversized.size());
    Require(batch.oversizedRemainderDiscarded && batch.messages.empty(), "Oversized incomplete input was not discarded");
    batch = stream.Append(framed.data(), framed.size());
    Require(batch.messages == std::vector<std::string>{Feedback}, "Stream did not recover after oversized input");
}
}

int main() {
    try {
        CheckCommandFormats();
        CheckFeedbackValidation();
        CheckStreamFraming();
        std::cout << "PLC, Unity, feedback validation and stream framing checks passed.\n";
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
