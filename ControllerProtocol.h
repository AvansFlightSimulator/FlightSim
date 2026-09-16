#pragma once

#include "BridgeTypes.h"

#include <string>
#include <vector>

// JSON encoding is separate from TCP transport. TCPServer adds the final newline.
// Both encoders are available so protocol tests can cover both client contracts.
std::string BuildPlcPayload(const MotionCommand& command, bool valuesAreStrings);
std::string BuildUnityPayload(const MotionCommand& command);
std::string BuildCommandPayload(const MotionCommand& command);

// Failure leaves positions unchanged and provides a readable rejection reason.
bool TryParseFeedback(
    const std::string& message, ActuatorValues& positions, std::string& error);

struct FeedbackBatch {
    std::vector<std::string> messages;
    bool oversizedRemainderDiscarded = false;
};

// TCP delivers bytes, not message boundaries. Keep partial messages between reads
// and extract all complete messages. Owned only by the TCP receive worker.
class FeedbackStream {
public:
    FeedbackBatch Append(const char* bytes, std::size_t count);
    void Clear();

private:
    std::string pending_;
};
