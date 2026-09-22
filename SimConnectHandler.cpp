#include "SimConnectHandler.h"

#include "MotionController.h"
#include "DashboardModel.h"
#include "MotionCalculator.h"
#include "TCPServer.h"

#include <cmath>
#include <iostream>

namespace {
    // These layouts and field order must match the SDK data definitions below.
    struct AircraftOrientation {
        double pitch;
        double bank;
    };

    struct RudderData {
        double deflection;
    };

    enum DataDefinitionId { DEFINITION_ORIENTATION, DEFINITION_RUDDER };
    enum DataRequestId { REQUEST_ORIENTATION, REQUEST_RUDDER };
}

SimConnectHandler::SimConnectHandler(TCPServer& server, MotionController& motion, DashboardModel* dashboard)
    : server_(server),
      motion_(motion),
      dashboard_(dashboard),
      nextCalculation_(std::chrono::steady_clock::now()) {
}

SimConnectHandler::~SimConnectHandler() {
    CloseSimConnect();
}

bool SimConnectHandler::IsConnected() const noexcept {
    return connection_ != nullptr;
}

bool SimConnectHandler::Dispatch() {
    return connection_ != nullptr
        && SUCCEEDED(SimConnect_CallDispatch(connection_, DispatchCallback, this));
}

void CALLBACK SimConnectHandler::DispatchCallback(SIMCONNECT_RECV* data, DWORD dataSize, void* context) {
    (void)dataSize;
    if (context != nullptr) {
        static_cast<SimConnectHandler*>(context)->HandleDispatch(data);
    }
}

void SimConnectHandler::HandleDispatch(SIMCONNECT_RECV* data) {
    switch (data->dwID) {
    case SIMCONNECT_RECV_ID_SIMOBJECT_DATA: {
        auto* objectData = reinterpret_cast<SIMCONNECT_RECV_SIMOBJECT_DATA*>(data);
        if (objectData->dwRequestID == REQUEST_ORIENTATION) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= nextCalculation_) {
                nextCalculation_ = now + MotionSettings::ControlInterval;
                const auto* orientation = reinterpret_cast<const AircraftOrientation*>(&objectData->dwData);
                HandleOrientation(orientation->pitch, orientation->bank, now);
            }
        }
        else if (objectData->dwRequestID == REQUEST_RUDDER) {
            const auto* rudder = reinterpret_cast<const RudderData*>(&objectData->dwData);
            if (std::isfinite(rudder->deflection)) {
                rudderDeflectionDegrees_ = rudder->deflection;
                rudderSampleAvailable_ = true;
                if (dashboard_) {
                    dashboard_->UpdateSimulatorRudder(rudder->deflection);
                }
            }
        }
        break;
    }

    case SIMCONNECT_RECV_ID_QUIT:
        std::cout << "[SIM] Microsoft Flight Simulator exited." << std::endl;
        if (dashboard_) {
            dashboard_->SetSimulatorConnected(false);
            dashboard_->AddEvent("Microsoft Flight Simulator exited", DashboardEventLevel::Warning);
        }
        quitRequested_ = true;
        break;

    default:
        break;
    }
}

void SimConnectHandler::HandleOrientation(double pitchRadians, double bankRadians,
    std::chrono::steady_clock::time_point timestamp) {
    if (!rudderSampleAvailable_) {
        return;
    }
    motion_.UpdateSimulatorInput(pitchRadians, bankRadians, rudderDeflectionDegrees_,
        server_.hasPositionFeedback(), timestamp);
}

bool SimConnectHandler::QuitRequested() const noexcept {
    return quitRequested_;
}

bool SimConnectHandler::InitializeSimConnect() {
    if (IsConnected()) {
        return true;
    }
    if (FAILED(SimConnect_Open(
        &connection_,
        "Retrieve Aircraft Orientation",
        nullptr,
        0,
        nullptr,
        0))) {
        if (!connectionFailureReported_) {
            std::cerr << "[SIM] Unable to connect to Microsoft Flight Simulator." << std::endl;
            if (dashboard_) {
                dashboard_->AddEvent("Waiting for Microsoft Flight Simulator", DashboardEventLevel::Warning);
            }
            connectionFailureReported_ = true;
        }
        if (dashboard_) {
            dashboard_->SetSimulatorConnected(false);
        }
        return false;
    }

    connectionFailureReported_ = false;
    std::cout << "[SIM] Connected to Microsoft Flight Simulator." << std::endl;
    if (dashboard_) {
        dashboard_->SetSimulatorConnected(true);
        dashboard_->AddEvent("SimConnect session established");
    }

    const HRESULT pitchResult = SimConnect_AddToDataDefinition(
        connection_, DEFINITION_ORIENTATION, "PLANE PITCH DEGREES", "radians");
    const HRESULT bankResult = SimConnect_AddToDataDefinition(
        connection_, DEFINITION_ORIENTATION, "PLANE BANK DEGREES", "radians");
    const HRESULT rudderResult = SimConnect_AddToDataDefinition(
        connection_, DEFINITION_RUDDER, "RUDDER DEFLECTION", "degrees");
    const HRESULT orientationRequest = SimConnect_RequestDataOnSimObject(
        connection_,
        REQUEST_ORIENTATION,
        DEFINITION_ORIENTATION,
        SIMCONNECT_OBJECT_ID_USER,
        SIMCONNECT_PERIOD_SIM_FRAME);
    const HRESULT rudderRequest = SimConnect_RequestDataOnSimObject(
        connection_,
        REQUEST_RUDDER,
        DEFINITION_RUDDER,
        SIMCONNECT_OBJECT_ID_USER,
        SIMCONNECT_PERIOD_SIM_FRAME);

    if (FAILED(pitchResult) || FAILED(bankResult) || FAILED(rudderResult)
        || FAILED(orientationRequest) || FAILED(rudderRequest)) {
        std::cerr << "[SIM] Failed to register one or more data requests." << std::endl;
        if (dashboard_) {
            dashboard_->AddEvent("SimConnect data registration failed", DashboardEventLevel::Error);
        }
        CloseSimConnect();
        return false;
    }

    return true;
}

void SimConnectHandler::CloseSimConnect() {
    if (connection_ != nullptr) {
        SimConnect_Close(connection_);
        connection_ = nullptr;
    }
    if (dashboard_) {
        dashboard_->SetSimulatorConnected(false);
    }
    rudderDeflectionDegrees_ = 0.0;
    rudderSampleAvailable_ = false;
    // A replacement session must initialize from its first sample, not stale data.
    motion_.ResetSimulatorInputFilter();
}
