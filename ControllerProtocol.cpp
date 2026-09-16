#include "ControllerProtocol.h"

#include "BuildMode.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <nlohmann/json.hpp>

namespace {
constexpr std::size_t MaximumBufferedMessageSize = 64 * 1024;

std::string FormatPlcValue(float value, bool asString) {
    int rounded = static_cast<int>(std::lround(value));
    if (!asString) {
        return std::to_string(rounded);
    }

    // Only the PLC string format clamps to three digits. Numeric mode preserves
    // the rounded value, as does Unity's existing protocol.
    rounded = (std::max)(0, (std::min)(rounded, 999));
    const std::string digits = std::to_string(rounded);
    return '"' + std::string(3 - digits.size(), '0') + digits + '"';
}

void AppendPlcValues(std::string& payload, const ActuatorValues& values, bool asStrings) {
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            payload += ',';
        }
        payload += FormatPlcValue(values[index], asStrings);
    }
}
}

std::string BuildPlcPayload(const MotionCommand& command, bool valuesAreStrings) {
    std::string payload = "{\"positions\":[";
    payload.reserve(160);
    AppendPlcValues(payload, command.positions, valuesAreStrings);
    payload += "],\"speeds\":[";
    AppendPlcValues(payload, command.speeds, valuesAreStrings);
    payload += "]}";
    return payload;
}

std::string BuildUnityPayload(const MotionCommand& command) {
    std::array<int, ActuatorCount> positions{};
    std::array<int, ActuatorCount> speeds{};
    for (std::size_t index = 0; index < ActuatorCount; ++index) {
        positions[index] = static_cast<int>(std::lround(command.positions[index]));
        speeds[index] = static_cast<int>(std::lround(command.speeds[index]));
    }

    nlohmann::json payload;
    payload["orientation"] = {
        {"yaw", command.attitude.yawDegrees},
        {"roll", command.attitude.rollDegrees},
        {"pitch", command.attitude.pitchDegrees}
    };
    payload["legs"] = positions;
    payload["positions"] = positions;
    payload["speeds"] = speeds;
    return payload.dump();
}

std::string BuildCommandPayload(const MotionCommand& command) {
#ifdef TARGET_PLC
    return BuildPlcPayload(command, PLC_VALUES_ARE_STRINGS);
#else
    return BuildUnityPayload(command);
#endif
}

bool TryParseFeedback(
    const std::string& message, ActuatorValues& positions, std::string& error) {
    error.clear();
    try {
        const auto received = nlohmann::json::parse(message);
        const auto values = received.find("currentPositions");
        if (values == received.end() || !values->is_array() || values->size() != ActuatorCount) {
            error = "Feedback rejected: expected six positions";
            return false;
        }

        ActuatorValues parsed{};
        for (std::size_t index = 0; index < ActuatorCount; ++index) {
            if (!(*values)[index].is_number()) {
                error = "Feedback rejected: position is not numeric";
                return false;
            }
            parsed[index] = (*values)[index].get<float>();
        }
        // Commit the whole sample only after all six values have passed validation.
        positions = parsed;
        return true;
    }
    catch (const nlohmann::json::exception& exception) {
        error = std::string("Feedback JSON could not be parsed: ") + exception.what();
        return false;
    }
}

FeedbackBatch FeedbackStream::Append(const char* bytes, std::size_t count) {
    pending_.append(bytes, count);
    FeedbackBatch batch;
    std::size_t start = 0;
    std::size_t delimiter = pending_.find('\n');
    while (delimiter != std::string::npos) {
        std::string message = pending_.substr(start, delimiter - start);
        if (!message.empty() && message.back() == '\r') {
            message.pop_back();
        }
        if (!message.empty()) {
            batch.messages.push_back(std::move(message));
        }
        start = delimiter + 1;
        delimiter = pending_.find('\n', start);
    }
    // Erase the consumed prefix once, rather than shifting it after every line.
    pending_.erase(0, start);

    // Legacy clients may omit the newline on one complete JSON value. Partial
    // JSON stays buffered. Adjacent objects still require newline delimiters.
    if (!pending_.empty() && nlohmann::json::accept(pending_)) {
        batch.messages.push_back(std::move(pending_));
        pending_.clear();
    }
    if (pending_.size() > MaximumBufferedMessageSize) {
        batch.oversizedRemainderDiscarded = true;
        pending_.clear();
    }
    return batch;
}

void FeedbackStream::Clear() {
    pending_.clear();
}
