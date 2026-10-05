#include "ControllerProtocol.h"

#include "BuildMode.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <nlohmann/json.hpp>

namespace {
    /**
     * MaximumBufferedMessageSize
     *
     * Maximum allowed size (bytes) for the internal feedback buffer. If the
     * buffer grows beyond this limit the remainder is discarded to avoid
     * unbounded memory growth.
     */
    constexpr std::size_t MaximumBufferedMessageSize = 64 * 1024;

    /**
     * FormatPlcValue
     *
     * Convert a floating-point actuator value into a PLC-friendly string.
     *
     * Behavior:
     * - Rounds `value` to the nearest integer.
     * - If `asString` is false, returns the rounded integer as an unquoted
     *   numeric string (e.g. "42").
     * - If `asString` is true, clamps the rounded value to [0, 999], pads with
     *   leading zeros to three digits and returns it quoted (e.g. "\"042\"").
     *
     * Parameters:
     *  - value: input actuator value.
     *  - asString: controls quoted zero-padded PLC string format when true.
     *
     * Returns:
     *  - A formatted std::string suitable for inclusion in PLC payloads.
     */
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

    /**
     * AppendPlcValues
     *
     * Append a comma-separated sequence of formatted actuator values to `payload`.
     * Each value is formatted via `FormatPlcValue`.
     *
     * Parameters:
     *  - payload: string to append formatted values into.
     *  - values: collection of actuator float values to format.
     *  - asStrings: forwarded to `FormatPlcValue` to select numeric vs string format.
     */
    void AppendPlcValues(std::string& payload, const ActuatorValues& values, bool asStrings) {
        for (std::size_t index = 0; index < values.size(); ++index) {
            if (index > 0) {
                payload += ',';
            }
            payload += FormatPlcValue(values[index], asStrings);
        }
    }
}

/**
 * BuildPlcPayload
 *
 * Create a compact JSON object: {"positions":[...],"speeds":[...]}.
 * Values are formatted by `AppendPlcValues`; if `valuesAreStrings` is true
 * each value is emitted as a zero-padded 3-digit quoted string.
 */
std::string BuildPlcPayload(const MotionCommand& command, bool valuesAreStrings) {
    std::string payload = "{\"positions\":[";
    payload.reserve(160);
    AppendPlcValues(payload, command.positions, valuesAreStrings);
    payload += "],\"speeds\":[";
    AppendPlcValues(payload, command.speeds, valuesAreStrings);
    payload += "]}";
    return payload;
}

/**
 * BuildUnityPayload
 *
 * Create a JSON payload consumed by Unity clients.
 *
 * Behavior:
 * - Rounds each actuator `positions` and `speeds` value to the nearest integer.
 * - Emits an `orientation` object with `yaw`, `roll`, and `pitch` (degrees).
 * - Emits three arrays:
 *     - "legs": same as `positions` (kept for legacy/compatibility).
 *     - "positions": rounded positions.
 *     - "speeds": rounded speeds.
 * - Uses `nlohmann::json` and returns the compact serialized string via `dump()`.
 *
 * Parameters:
 *  - `command` : MotionCommand containing `positions`, `speeds`, and `attitude`.
 *
 * Returns:
 *  - `std::string` : serialized JSON payload.
 */
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

/**
 * BuildCommandPayload
 *
 * Build a JSON command payload appropriate for the current target/configuration.
 *
 * Behavior:
 * - When compiled for PLC (__TARGET_PLC__):
 *     - If `USE_PLC_CSP` is enabled, emit a CSP-style payload with an explicit
 *       `controlMode` discriminator (prevents confusion with legacy PP step
 *       messages), send raw `positions` without rounding, and send six
 *       zero `speeds` (the PLC CSP parser rejects nonzero speeds).
 *     - Otherwise, emit a compact PLC payload using `BuildPlcPayload`. Whether
 *       values are encoded as quoted, zero-padded strings is controlled by
 *       `PLC_VALUES_ARE_STRINGS`.
 * - When not compiled for PLC, emit the Unity-compatible payload produced by
 *   `BuildUnityPayload`.
 */
std::string BuildCommandPayload(const MotionCommand& command) {
#ifdef TARGET_PLC
    if (USE_PLC_CSP) {
        // An explicit discriminator prevents a CSP endpoint from being mistaken
        // for a legacy PP feedback-relative step. Do not round away small motion.
        nlohmann::json payload;
        payload["controlMode"] = CONTROL_MODE_CSP;
        payload["positions"] = command.positions;
        // The PLC CSP parser accepts only zero speeds; the PLC plans its own
        // trajectory. command.speeds stays intact for diagnostics.
        payload["speeds"] = std::array<int, ActuatorCount>{};
        return payload.dump();
    }
    return BuildPlcPayload(command, PLC_VALUES_ARE_STRINGS);
#else
    return BuildUnityPayload(command);
#endif
}

/**
 * TryParseFeedback
 *
 * Parse a JSON feedback message and extract actuator positions.
 *
 * Behavior and validation:
 * - Expects the message to be a JSON object containing a "currentPositions"
 *   field that is an array of exactly `ActuatorCount` numeric values.
 * - Each array element must be numeric; values are read as `float`.
 * - On successful validation all values are copied into `positions` and the
 *   function returns true.
 * - On failure `positions` is left unchanged, `error` is set to a short,
 *   descriptive message and the function returns false.
 * - JSON parse errors are caught and reported in `error`.
 *
 * Parameters:
 *  - `message`  : input JSON text to parse.
 *  - `positions`: output parameter populated with the parsed actuator values
 *                 only when the function returns true.
 *  - `error`    : output string receiving a short diagnostic on failure.
 *
 * Returns:
 *  - `true`  if parsing and validation succeed.
 *  - `false` if the message is invalid, malformed, or does not contain the
 *            expected numeric array.
 */
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

/**
 * FeedbackStream::Append
 *
 * Append raw bytes from a stream to the internal buffer and extract complete
 * newline-delimited JSON messages.
 *
 * Behavior:
 * - Appends `count` bytes from `bytes` into an internal `pending_` buffer.
 * - Splits the buffer on '\n' and returns each complete line as a message.
 * - Trims trailing '\r' for CRLF-terminated lines.
 * - Leaves partial lines in `pending_` for later completion.
 * - If the remaining `pending_` contains a single, complete JSON value but no
 *   trailing newline, it will be accepted (using `nlohmann::json::accept`) and
 *   returned as a message. This supports legacy clients that omit the newline.
 * - If the buffer grows beyond `MaximumBufferedMessageSize`, it is cleared and
 *   `oversizedRemainderDiscarded` is set on the returned batch.
 *
 * Returns:
 *  - `FeedbackBatch` containing the extracted messages (without newline/CR)
 *    and a flag indicating whether an oversized remainder was discarded.
 */
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

/**
 * Clear the cache that is currently pending
 */
void FeedbackStream::Clear() {
    pending_.clear();
}
