#pragma once

#include "TCPServer.h"

#include <SimConnect.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>

class DashboardModel;

// This layout must match the two values registered with SimConnect.
struct AircraftOrientation {
    double pitch;
    double bank;
};

struct RudderData {
    double deflection;
};

enum DATA_DEFINE_ID {
    DEFINITION_ORIENTATION,
    DEFINITION_RUDDER
};

enum DATA_REQUEST_ID {
    REQUEST_ORIENTATION,
    REQUEST_RUDDER
};

extern HANDLE hSimConnect;

class SimConnectHandler {
public:
    explicit SimConnectHandler(TCPServer& server, DashboardModel* dashboard = nullptr);

    static void CALLBACK MyDispatchProcRD(SIMCONNECT_RECV* data, DWORD dataSize, void* context);

    bool InitializeSimConnect();
    void CloseSimConnect();

    // Copies the newest command built by the SimConnect callback.
    bool TryGetLatestPayload(std::string& payload) const;
    bool QuitRequested() const noexcept;

private:
    void HandleDispatch(SIMCONNECT_RECV* data);
    void HandleOrientation(const AircraftOrientation& orientation);
    void PublishPayload(std::string payload);

    TCPServer& server_;
    double rudderDeflectionDegrees_ = 0.0;
    std::chrono::steady_clock::time_point nextCalculation_;
    std::chrono::steady_clock::time_point nextLimitWarning_;

    mutable std::mutex payloadMutex_;
    std::string latestPayload_;
    std::atomic<bool> quitRequested_{ false };
    DashboardModel* dashboard_ = nullptr;
    bool connectionFailureReported_ = false;
};
