#include "BuildMode.h"
#include "DashboardModel.h"
#include "HmiWindow.h"
#include "ProtocolLogger.h"
#include "MotionCalculator.h"
#include "MotionController.h"
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
// Interval used by the output worker to send motion commands. Derived from
// MotionSettings so the send rate matches the configured control loop rate.
constexpr auto OutputInterval = MotionSettings::ControlInterval;

// How long to wait before retrying a SimConnect initialization after a
// failure. This keeps reconnect attempts from spinning too quickly.
constexpr auto SimConnectRetryInterval = std::chrono::seconds(5);

// PrintStartupBanner
// -------------------
// Prints a small informational banner to stdout describing the current build
// settings, network endpoint, number of actuators, and output rate. This is
// useful during startup to confirm which configuration the application is
// running with.
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

// ProcessWindowsMessages
// -----------------------
// Pumps the Win32 message queue and forwards control messages to the HMI
// window. Returns false if a WM_QUIT message was received which signals the
// application should exit. Any messages not handled by the HMI are translated
// and dispatched normally.
bool ProcessWindowsMessages(HmiWindow& hmi) {
    MSG message;
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            return false;
        }
        if (!hmi.ProcessControlMessage(message)) {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
    }
    return true;
}

// FeedbackThreadMethod
// --------------------
// Dedicated worker that accepts a TCP client and receives feedback packets
// (position reports) from the networked motion source. The thread loops
// until stopRequested becomes true. If no client is connected it repeatedly
// tries to accept one; on accept failure it waits briefly and retries so the
// thread does not spin.
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

        // Blocking call that reads incoming feedback messages and updates the
        // server's internal state (e.g., last-known actuator positions).
        server.receiveData();
    }
}

// OutputThreadMethod
// ------------------
// Periodic sender running at MotionSettings::ControlInterval. When a
// connected client has provided position feedback and the motion controller
// has a fresh payload, the worker sends the motion command over TCP, logs
// the protocol message, and updates the dashboard metrics. The loop uses a
// steady_clock sleep_until pattern to maintain a stable periodic cadence and
// prevents catch-up bursts after long delays.
void OutputThreadMethod(
    TCPServer& server,
    const MotionController& motion,
    DashboardModel& dashboard,
    const std::atomic<bool>& stopRequested) {
    auto nextWakeup = std::chrono::steady_clock::now();
    std::string payload;

    while (!stopRequested) {
        nextWakeup += OutputInterval;

        // Only send when: client connected, we have position feedback, and a
        // newly calculated payload is available from the motion controller.
        if (server.isConnected()
            && server.hasPositionFeedback()
            && motion.TryGetLatestPayload(payload)) {
            if (server.sendData(payload)) {
                LogProtocolMessage("SEND", payload);
                dashboard.RecordCommandSent();
            }
        }

        // Sleep until the next scheduled send time to maintain a steady rate.
        std::this_thread::sleep_until(nextWakeup);

        // Avoid running a burst of sends if we fell behind; reset the schedule
        // to 'now' so the loop resumes a normal cadence.
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
    MotionController motion(dashboard);
    dashboard.AddEvent(std::string("Motion bridge started in ") + ACTIVE_TARGET_NAME + " mode");

    HmiWindow hmi(dashboard, motion, ACTIVE_TARGET_NAME, ACTIVE_BIND_IP, ACTIVE_PORT);
    if (!hmi.Create(GetModuleHandle(nullptr), SW_SHOWDEFAULT)) {
        std::cerr << "[HMI] Unable to create the dashboard window." << std::endl;
        return 1;
    }

    TCPServer server(ACTIVE_BIND_IP, ACTIVE_PORT, &dashboard);
    SimConnectHandler simulator(server, motion, &dashboard);
    std::atomic<bool> stopRequested{ false };
    // Blocking accept/receive and send use separate workers so the HMI and
    // SimConnect dispatch remain responsive on this main thread.
    std::thread feedbackThread(FeedbackThreadMethod, std::ref(server), std::cref(stopRequested));
    std::thread outputThread(
        OutputThreadMethod,
        std::ref(server),
        std::cref(motion),
        std::ref(dashboard),
        std::cref(stopRequested));

    std::cout << "[APP] Running. Close the HMI, console, or MSFS to stop." << std::endl;
    dashboard.AddEvent("Dashboard ready; control workers are running");

    auto nextSimConnectAttempt = std::chrono::steady_clock::now();

    while (!simulator.QuitRequested() && ProcessWindowsMessages(hmi)) {
        const auto now = std::chrono::steady_clock::now();
        if (motion.GetInputMode() != InputMode::Simulator) {
            if (simulator.IsConnected()) {
                simulator.CloseSimConnect();
            }
            motion.TickManual(server.isConnected() && server.hasPositionFeedback(), server.getCurrentPositions(), now);
            Sleep(5);
            continue;
        }
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
    // The server and motion controller must outlive both workers.
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
