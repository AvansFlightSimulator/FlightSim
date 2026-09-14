#include "SimConnectHandler.h"

#include "BuildMode.h"
#include "DashboardModel.h"
#include "calculate_legs.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <utility>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

HANDLE hSimConnect = nullptr;

namespace {
constexpr std::size_t ActuatorCount = TCPServer::ActuatorCount;
constexpr double MaximumPitchDegrees = 30.0;
constexpr double MaximumRollDegrees = 30.0;
constexpr double MaximumYawDegrees = 30.0;
constexpr float ControlRateHz = 20.0f;
constexpr float SpeedLimit = 500.0f;
constexpr float MinimumSpeed = 2.0f;
constexpr float MaximumStepPerSecond = 400.0f;
constexpr float PositionDeadzone = 0.0f;
constexpr float BaseLegLength = 1156.372420286821f;
constexpr float NeutralActuatorPosition = 200.0f;
const auto ControlInterval = std::chrono::milliseconds(50);

// Physical mounting coordinates for the six-actuator Stewart platform.
vec baseLegs[ActuatorCount] = {
    {177.53f, 723.37f, 0.0f},
    {-177.53f, 723.37f, 0.0f},
    {-715.23f, -207.94f, 0.0f},
    {-537.69f, -515.43f, 0.0f},
    {537.69f, -515.43f, 0.0f},
    {715.23f, -207.94f, 0.0f}
};

vec platformLegs[ActuatorCount] = {
    {360.59f, 346.0f, 0.0f},
    {-360.59f, 346.0f, 0.0f},
    {-480.12f, 139.59f, 0.0f},
    {-119.17f, -485.59f, 0.0f},
    {119.17f, -485.59f, 0.0f},
    {480.12f, 139.59f, 0.0f}
};

vec startHeight{ 0.0f, 0.0f, 1079.0f };

double Clamp(double value, double minimum, double maximum) {
    return std::max(minimum, std::min(value, maximum));
}

#ifdef TARGET_PLC
// Retained legacy formatter and reused by the active PLC payload builder.
std::string CheckDigitCount(int value) {
    if (value >= 100) {
        return std::to_string(value);
    }
    if (value >= 10) {
        return "0" + std::to_string(value);
    }
    return "00" + std::to_string(value);
}

std::string PadThreeDigits(int value) {
    value = std::max(0, std::min(value, 999));
    return CheckDigitCount(value);
}
#endif

#ifdef TARGET_UNITY
std::string BuildUnityPayload(
    double yawDegrees,
    double rollDegrees,
    double pitchDegrees,
    const std::array<float, ActuatorCount>& legLengths,
    const std::array<float, ActuatorCount>& speeds) {
    std::array<int, ActuatorCount> positions{};
    std::array<int, ActuatorCount> roundedSpeeds{};
    for (std::size_t index = 0; index < ActuatorCount; ++index) {
        positions[index] = static_cast<int>(std::lround(legLengths[index]));
        roundedSpeeds[index] = static_cast<int>(std::lround(speeds[index]));
    }

    nlohmann::json payload;
    payload["orientation"] = {
        {"yaw", yawDegrees},
        {"roll", rollDegrees},
        {"pitch", pitchDegrees}
    };
    payload["legs"] = positions;
    payload["positions"] = positions;
    payload["speeds"] = roundedSpeeds;
    return payload.dump();
}
#endif

#ifdef TARGET_PLC
std::string BuildPlcPayload(
    const std::array<float, ActuatorCount>& legLengths,
    const std::array<float, ActuatorCount>& speeds) {
    std::string payload = "{\"positions\":[";
    payload.reserve(160);

    for (std::size_t index = 0; index < ActuatorCount; ++index) {
        if (index > 0) {
            payload += ',';
        }
        const int value = static_cast<int>(std::lround(legLengths[index]));
        payload += PLC_VALUES_ARE_STRINGS
            ? ('\"' + PadThreeDigits(value) + '\"')
            : std::to_string(value);
    }

    payload += "],\"speeds\":[";
    for (std::size_t index = 0; index < ActuatorCount; ++index) {
        if (index > 0) {
            payload += ',';
        }
        const int value = static_cast<int>(std::lround(speeds[index]));
        payload += PLC_VALUES_ARE_STRINGS
            ? ('\"' + PadThreeDigits(value) + '\"')
            : std::to_string(value);
    }

    payload += "]}";
    return payload;
}
#endif
}

SimConnectHandler::SimConnectHandler(TCPServer& server, DashboardModel* dashboard)
    : server_(server),
      nextCalculation_(std::chrono::steady_clock::now()),
      nextLimitWarning_(std::chrono::steady_clock::time_point::min()),
      dashboard_(dashboard) {
}

void CALLBACK SimConnectHandler::MyDispatchProcRD(
    SIMCONNECT_RECV* data,
    DWORD dataSize,
    void* context) {
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
                nextCalculation_ = now + ControlInterval;
                const auto* orientation = reinterpret_cast<const AircraftOrientation*>(&objectData->dwData);
                HandleOrientation(*orientation);
            }
        }
        else if (objectData->dwRequestID == REQUEST_RUDDER) {
            const auto* rudder = reinterpret_cast<const RudderData*>(&objectData->dwData);
            rudderDeflectionDegrees_ = rudder->deflection;
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

void SimConnectHandler::HandleOrientation(const AircraftOrientation& orientation) {
    const double rawPitch = -orientation.pitch * (180.0 / M_PI);
    const double rawRoll = orientation.bank * (180.0 / M_PI);
    const double rawYaw = -rudderDeflectionDegrees_;

    const double pitchDegrees = Clamp(rawPitch, -MaximumPitchDegrees, MaximumPitchDegrees);
    const double rollDegrees = Clamp(rawRoll, -MaximumRollDegrees, MaximumRollDegrees);
    const double yawDegrees = Clamp(rawYaw, -MaximumYawDegrees, MaximumYawDegrees);

    const auto now = std::chrono::steady_clock::now();
    if ((rawPitch != pitchDegrees || rawRoll != rollDegrees || rawYaw != yawDegrees)
        && now >= nextLimitWarning_) {
        std::cout << "[SIM] Attitude limited to pitch=" << pitchDegrees
                  << ", roll=" << rollDegrees
                  << ", yaw=" << yawDegrees << " degrees." << std::endl;
        if (dashboard_) {
            std::ostringstream message;
            message << "Attitude limited: P " << std::lround(pitchDegrees)
                    << " / R " << std::lround(rollDegrees)
                    << " / Y " << std::lround(yawDegrees) << " deg";
            dashboard_->AddEvent(message.str(), DashboardEventLevel::Warning);
        }
        nextLimitWarning_ = now + std::chrono::seconds(1);
    }

    if (dashboard_) {
        dashboard_->UpdateOrientation(pitchDegrees, rollDegrees, yawDegrees);
    }

    if (!server_.hasPositionFeedback()) {
        return;
    }

    const auto currentLegLengths = server_.getCurrentPositions();

    constexpr float controlStepSeconds = 1.0f / ControlRateHz;
    constexpr float maximumStep = MaximumStepPerSecond * controlStepSeconds;
    std::array<float, ActuatorCount> targetLengths{};
    std::array<float, ActuatorCount> speeds{};

    for (std::size_t index = 0; index < ActuatorCount; ++index) {
        const float geometricLength = compute_li_length(
            startHeight,
            static_cast<float>(yawDegrees),
            static_cast<float>(-rollDegrees),
            static_cast<float>(pitchDegrees),
            platformLegs[index],
            baseLegs[index]);
        const float desiredLength = std::round(
            geometricLength - BaseLegLength + NeutralActuatorPosition);

        const float requestedDelta = desiredLength - currentLegLengths[index];
        const float limitedDelta = std::max(-maximumStep, std::min(requestedDelta, maximumStep));
        targetLengths[index] = currentLegLengths[index] + limitedDelta;

        const float distance = std::fabs(limitedDelta);
        if (distance <= PositionDeadzone) {
            speeds[index] = MinimumSpeed;
        }
        else {
            speeds[index] = std::max(
                MinimumSpeed,
                std::min(distance / controlStepSeconds, SpeedLimit));
        }
    }

    if (dashboard_) {
        dashboard_->UpdateMotion(
            pitchDegrees,
            rollDegrees,
            yawDegrees,
            targetLengths,
            speeds);
    }

#ifdef TARGET_PLC
    PublishPayload(BuildPlcPayload(targetLengths, speeds));
#else
    PublishPayload(BuildUnityPayload(yawDegrees, rollDegrees, pitchDegrees, targetLengths, speeds));
#endif
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
    if (FAILED(SimConnect_Open(
        &hSimConnect,
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
        hSimConnect, DEFINITION_ORIENTATION, "PLANE PITCH DEGREES", "radians");
    const HRESULT bankResult = SimConnect_AddToDataDefinition(
        hSimConnect, DEFINITION_ORIENTATION, "PLANE BANK DEGREES", "radians");
    const HRESULT rudderResult = SimConnect_AddToDataDefinition(
        hSimConnect, DEFINITION_RUDDER, "RUDDER DEFLECTION", "degrees");
    const HRESULT orientationRequest = SimConnect_RequestDataOnSimObject(
        hSimConnect,
        REQUEST_ORIENTATION,
        DEFINITION_ORIENTATION,
        SIMCONNECT_OBJECT_ID_USER,
        SIMCONNECT_PERIOD_SIM_FRAME);
    const HRESULT rudderRequest = SimConnect_RequestDataOnSimObject(
        hSimConnect,
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
    if (hSimConnect != nullptr) {
        SimConnect_Close(hSimConnect);
        hSimConnect = nullptr;
    }
    if (dashboard_) {
        dashboard_->SetSimulatorConnected(false);
    }
}
