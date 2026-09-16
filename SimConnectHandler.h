#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <SimConnect.h>

#include <chrono>
#include <mutex>
#include <string>

class DashboardModel;
class TCPServer;

// Owns the SDK session. Connection, dispatch and calculation run on the main
// thread; only TryGetLatestPayload is called by the TCP output worker.
class SimConnectHandler {
public:
    explicit SimConnectHandler(TCPServer& server, DashboardModel* dashboard = nullptr);
    ~SimConnectHandler();

    SimConnectHandler(const SimConnectHandler&) = delete;
    SimConnectHandler& operator=(const SimConnectHandler&) = delete;

    bool InitializeSimConnect();
    void CloseSimConnect();
    bool IsConnected() const noexcept;
    bool Dispatch();
    bool QuitRequested() const noexcept;

    // Copies the newest command under a lock; the worker never sees a partial JSON.
    // The cache intentionally retains the existing behavior across reconnects.
    bool TryGetLatestPayload(std::string& payload) const;

private:
    static void CALLBACK DispatchCallback(SIMCONNECT_RECV* data, DWORD dataSize, void* context);
    void HandleDispatch(SIMCONNECT_RECV* data);
    void HandleOrientation(double pitchRadians, double bankRadians);
    void PublishPayload(std::string payload);

    HANDLE connection_ = nullptr;
    TCPServer& server_;
    DashboardModel* dashboard_ = nullptr;
    double rudderDeflectionDegrees_ = 0.0;
    std::chrono::steady_clock::time_point nextCalculation_;
    bool quitRequested_ = false;
    bool connectionFailureReported_ = false;

    // Shared with the output worker. All other fields belong to the main thread.
    mutable std::mutex payloadMutex_;
    std::string latestPayload_;
};
