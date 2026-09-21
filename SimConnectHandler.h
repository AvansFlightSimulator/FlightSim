#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <SimConnect.h>

#include <chrono>

class DashboardModel;
class MotionController;
class TCPServer;

// Owns the SDK session and telemetry subscriptions on the main thread.
class SimConnectHandler {
public:
    SimConnectHandler(TCPServer& server, MotionController& motion, DashboardModel* dashboard = nullptr);
    ~SimConnectHandler();

    SimConnectHandler(const SimConnectHandler&) = delete;
    SimConnectHandler& operator=(const SimConnectHandler&) = delete;

    bool InitializeSimConnect();
    void CloseSimConnect();
    bool IsConnected() const noexcept;
    bool Dispatch();
    bool QuitRequested() const noexcept;

private:
    static void CALLBACK DispatchCallback(SIMCONNECT_RECV* data, DWORD dataSize, void* context);
    void HandleDispatch(SIMCONNECT_RECV* data);
    void HandleOrientation(double pitchRadians, double bankRadians);

    HANDLE connection_ = nullptr;
    TCPServer& server_;
    MotionController& motion_;
    DashboardModel* dashboard_ = nullptr;
    double rudderDeflectionDegrees_ = 0.0;
    std::chrono::steady_clock::time_point nextCalculation_;
    bool quitRequested_ = false;
    bool connectionFailureReported_ = false;
};
