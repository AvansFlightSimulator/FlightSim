#include "ProtocolLogger.h"

#include <windows.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>

namespace {
    // Serializes writes to the log file across threads.
    std::mutex logMutex;
    // Reference time for log timestamps (steady clock).
    const auto processStartTime = std::chrono::steady_clock::now();

    // Lazily create the logs folder and open the log file once; returns the shared stream.
    std::ofstream& protocolLog() {
        static std::ofstream logFile;
        static bool initialized = false;

        if (!initialized) {
            initialized = true;
            // Create "logs" if missing; an existing folder is fine.
            if (!CreateDirectoryA("logs", nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
                std::cerr << "Unable to create the logs directory." << std::endl;
                return logFile;
            }
            // Open in append mode so earlier runs are kept.
            logFile.open("logs/FlightSim_StewartServer_Log.txt", std::ios::app);
            if (!logFile) {
                std::cerr << "Unable to open the protocol log." << std::endl;
            }
        }

        return logFile;
    }
}

// Write one log line: [elapsed ms] direction: payload.
void LogProtocolMessage(const char* direction, const std::string& payload) {
    // Milliseconds since process start.
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - processStartTime).count();

    std::lock_guard<std::mutex> lock(logMutex);
    std::ofstream& logFile = protocolLog();
    // Skip writing if the file couldn't be opened.
    if (logFile) {
        logFile << '[' << elapsed << "ms] " << direction << ": " << payload << '\n';
    }
}