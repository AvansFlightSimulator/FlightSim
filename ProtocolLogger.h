#pragma once

#include <string>

// Thread-safe logging shared by the simulator and TCP worker threads.
void LogProtocolMessage(const char* direction, const std::string& payload);
