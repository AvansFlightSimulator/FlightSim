#include "SimConnectHandler.h"

#include "ControllerProtocol.h"
#include "DashboardModel.h"
#include "MotionCalculator.h"
#include "TCPServer.h"

#include <iostream>
#include <utility>

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

SimConnectHandler::SimConnectHandler(TCPServer& server, DashboardModel* dashboard)
    : server_(server),
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
                HandleOrientation(orientation->pitch, orientation->bank);
            }
        }
        else if (objectData->dwRequestID == REQUEST_RUDDER) {
            const auto* rudder = reinterpret_cast<const RudderData*>(&objectData->dwData);
            rudderDeflectionDegrees_ = rudder->deflection;
            if (dashboard_) {
                dashboard_->UpdateSimulatorRudder(rudder->deflection);
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

void SimConnectHandler::HandleOrientation(double pitchRadians, double bankRadians) {
    // Raw cards preserve MSFS signs. The target cards use the motion mapping.
    const auto attitude = CalculatePlatformAttitude(pitchRadians, bankRadians, rudderDeflectionDegrees_);
    if (dashboard_) {
        dashboard_->UpdateSimulatorAttitude(RadiansToDegrees(pitchRadians), RadiansToDegrees(bankRadians));
        dashboard_->UpdateOrientation(attitude.pitchDegrees, attitude.rollDegrees, attitude.yawDegrees);
    }

    // Telemetry remains visible before a controller connects. Actuator commands
    // require feedback because step limits are relative to actual leg positions.
    if (!server_.hasPositionFeedback()) {
        return;
    }
    const auto command = CalculateMotion(attitude, server_.getCurrentPositions());
    if (dashboard_) {
        dashboard_->UpdateMotion(
            attitude.pitchDegrees, attitude.rollDegrees, attitude.yawDegrees,
            command.positions, command.speeds);
    }
    PublishPayload(BuildCommandPayload(command));
}

void SimConnectHandler::PublishPayload(std::string payload) {
    std::lock_guard<std::mutex> lock(payloadMutex_);
    latestPayload_ = std::move(payload);
}

bool SimConnectHandler::TryGetLatestPayload(std::string& payload) const {
    std::lock_guard<std::mutex> lock(payloadMutex_);
    if (latestPayload_.empty()) {
        return false;
    }

    payload = latestPayload_;
    return true;
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
}
