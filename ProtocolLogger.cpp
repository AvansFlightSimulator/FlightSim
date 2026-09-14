#include "ProtocolLogger.h"

#include <windows.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>

namespace {
std::mutex logMutex;
const auto processStartTime = std::chrono::steady_clock::now();

std::ofstream& protocolLog() {
    static std::ofstream logFile;
    static bool initialized = false;

    if (!initialized) {
        initialized = true;
        if (!CreateDirectoryA("logs", nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
            std::cerr << "Unable to create the logs directory." << std::endl;
            return logFile;
        }
        logFile.open("logs/FlightSim_StewartServer_Log.txt", std::ios::app);
        if (!logFile) {
            std::cerr << "Unable to open the protocol log." << std::endl;
        }
    }

    return logFile;
}
}

void LogProtocolMessage(const char* direction, const std::string& payload) {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - processStartTime).count();

    std::lock_guard<std::mutex> lock(logMutex);
    std::ofstream& logFile = protocolLog();
    if (logFile) {
        logFile << '[' << elapsed << "ms] " << direction << ": " << payload << '\n';
    }
}
