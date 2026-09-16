#include "BuildMode.h"
#include "DashboardModel.h"
#include "HmiWindow.h"
#include "ProtocolLogger.h"
#include "MotionCalculator.h"
#include "SimConnectHandler.h"
#include "TCPServer.h"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

namespace {
constexpr auto OutputInterval = MotionSettings::ControlInterval;
constexpr auto SimConnectRetryInterval = std::chrono::seconds(5);

void PrintStartupBanner() {
    std::cout
        << "========================================\n"
        << " Flight Simulator Motion Bridge\n"
        << "----------------------------------------\n"
        << " Target     : " << ACTIVE_TARGET_NAME << '\n'
        << " Endpoint   : " << ACTIVE_BIND_IP << ':' << ACTIVE_PORT << '\n'
        << " Actuators  : " << ActuatorCount << '\n'
        << " Output rate: " << MotionSettings::ControlRateHz << " Hz\n"
        << "========================================\n";
}

bool ProcessWindowsMessages() {
    MSG message;
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    return true;
}

void FeedbackThreadMethod(TCPServer& server, const std::atomic<bool>& stopRequested) {
    while (!stopRequested) {
        if (!server.isConnected()) {
            if (!server.startListening()) {
                if (!stopRequested) {
                    std::cerr << "[TCP] Unable to accept a client; retrying." << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(250));
                    continue;
                }
                return;
            }
        }

        server.receiveData();
    }
}

void OutputThreadMethod(
    TCPServer& server,
    const SimConnectHandler& simulator,
    DashboardModel& dashboard,
    const std::atomic<bool>& stopRequested) {
    auto nextWakeup = std::chrono::steady_clock::now();
    std::string payload;

    while (!stopRequested) {
        nextWakeup += OutputInterval;

        // This gate requires a first valid sample, not a feedback-age watchdog.
        if (server.isConnected()
            && server.hasPositionFeedback()
            && simulator.TryGetLatestPayload(payload)) {
            if (server.sendData(payload)) {
                LogProtocolMessage("SEND", payload);
                dashboard.RecordCommandSent();
            }
        }

        std::this_thread::sleep_until(nextWakeup);

        // Do not run a burst of catch-up sends after a delayed cycle.
        const auto now = std::chrono::steady_clock::now();
        if (now > nextWakeup + OutputInterval) {
            nextWakeup = now;
        }
    }
}
}

int main() {
    SetProcessDPIAware();
    PrintStartupBanner();

    DashboardModel dashboard;
    dashboard.AddEvent(std::string("Motion bridge started in ") + ACTIVE_TARGET_NAME + " mode");

    HmiWindow hmi(dashboard, ACTIVE_TARGET_NAME, ACTIVE_BIND_IP, ACTIVE_PORT);
    if (!hmi.Create(GetModuleHandle(nullptr), SW_SHOWDEFAULT)) {
        std::cerr << "[HMI] Unable to create the dashboard window." << std::endl;
        return 1;
    }

    TCPServer server(ACTIVE_BIND_IP, ACTIVE_PORT, &dashboard);
    SimConnectHandler simulator(server, &dashboard);
    std::atomic<bool> stopRequested{ false };
    // Blocking accept/receive and send use separate workers so the HMI and
    // SimConnect dispatch remain responsive on this main thread.
    std::thread feedbackThread(FeedbackThreadMethod, std::ref(server), std::cref(stopRequested));
    std::thread outputThread(
        OutputThreadMethod,
        std::ref(server),
        std::cref(simulator),
        std::ref(dashboard),
        std::cref(stopRequested));

    std::cout << "[APP] Running. Close the HMI, console, or MSFS to stop." << std::endl;
    dashboard.AddEvent("Dashboard ready; control workers are running");

    auto nextSimConnectAttempt = std::chrono::steady_clock::now();

    while (!simulator.QuitRequested() && ProcessWindowsMessages()) {
        const auto now = std::chrono::steady_clock::now();
        if (!simulator.IsConnected() && now >= nextSimConnectAttempt) {
            simulator.InitializeSimConnect();
            nextSimConnectAttempt = now + SimConnectRetryInterval;
        }

        if (simulator.IsConnected() && !simulator.Dispatch()) {
            std::cerr << "[SIM] Dispatch failed; reconnecting." << std::endl;
            dashboard.AddEvent("SimConnect dispatch failed; reconnecting", DashboardEventLevel::Warning);
            simulator.CloseSimConnect();
            nextSimConnectAttempt = now + SimConnectRetryInterval;
        }
        Sleep(5);
    }

    std::cout << "[APP] Shutting down..." << std::endl;
    dashboard.AddEvent("Application shutdown requested");
    stopRequested = true;
    // Closing sockets interrupts blocking network calls before we join workers.
    // The server and simulator must outlive both workers.
    server.closeConnection();

    if (outputThread.joinable()) {
        outputThread.join();
    }
    if (feedbackThread.joinable()) {
        feedbackThread.join();
    }

    simulator.CloseSimConnect();
    std::cout << "[APP] Shutdown complete." << std::endl;
    return 0;
}
