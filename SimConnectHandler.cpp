#include "TCPServer.h"
#include "SimConnectHandler.h"
#include "calculate_legs.h"  // Include the calculate_legs header
#include <windows.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <cmath>  // For M_PI
#include <thread>
#include <vector>  
#include <cstdio>   // [CHANGED] voor std::snprintf

#include "BuildMode.h" // [CHANGED] modus-schakelaar

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

HANDLE hSimConnect = nullptr;

std::ofstream logFile;

TCPServer* tcpServer;
static int counter = 0;

SimConnectHandler::SimConnectHandler(TCPServer* server)
{
    tcpServer = server;
    logFile.open("logs/FlightSim_StewardServer_Log.txt");
}

// Base and platform leg positions for computation
vec base_legs[6] = {
    {177.53, 723.37, 0},
    {-177.53, 723.37, 0},
    {-715.23, -207.94, 0},
    {-537.69, -515.43, 0},
    {537.69, -515.43, 0},
    {715.23, -207.94, 0}
};

vec platform_legs[6] = {
    {360.59, 346.0, 0},
    {-360.59, 346.0, 0},
    {-480.12, 139.59, 0},
    {-119.17, -485.59, 0},
    {119.17, -485.59, 0},
    {480.12, 139.59, 0}
};

vec start_height{ 0, 0, 1079 };  // Starting height of the platform
std::vector<float> legLengths(6); // To hold the computed leg lengths
std::vector<float> speeds(6);

const float REFRESH_RATE = 20.0f;     // in Hz
const float SPEED_LIMIT = 500.0f;     // absolute hard cap naar PLC (eventueel lager kiezen)
const float MIN_SPEED = 2.0f;       // minimale snelheid zodat hij niet “doodvalt”

// Max positieverschil dat we per seconde mogen willen (in dezelfde units als legLengths)
const float MAX_STEP_PER_SEC = 150.0f;   // tuning: kleiner = smoother, minder kans op overspeed
const float POSITION_DEADZONE = .0f;   // kleiner dan dit → behandelen als stilstand


// Function to clamp a value between min and max
double clamp(double value, double min, double max) {
    if (value > max) {
        return max;
    }
    else if (value < min) {
        return min;
    }
    return value;
}

std::string CheckDigitCount(int value)
{
    if (value >= 100)
        return std::to_string(value);
    else if (value < 100 && value >= 10)
        return "0" + std::to_string(value);
    else
        return "00" + std::to_string(value);
}

// ===== [CHANGED] helpers voor payload-opbouw (Unity/PLC) =====
static inline std::string pad3(int v) {
    if (v < 0) v = 0; if (v > 999) v = 999;
    char buf[4]{};
    std::snprintf(buf, sizeof(buf), "%03d", v);
    return std::string(buf);
}

static std::string buildUnityPayload(double yaw_deg, double roll_deg, double pitch_deg,
    const std::vector<float>& legLengths,
    const std::vector<float>& speeds) {
    std::vector<int> pos(6), spd(6);
    for (int i = 0; i < 6; ++i) {
        pos[i] = static_cast<int>(std::lround(legLengths[i]));
        spd[i] = static_cast<int>(std::lround(speeds[i]));
    }
    nlohmann::json j;
    j["orientation"] = { {"yaw", yaw_deg}, {"roll", roll_deg}, {"pitch", pitch_deg} };
    j["legs"] = pos;
    j["positions"] = pos;
    j["speeds"] = spd;
    return j.dump();
}

static std::string buildPlcPayload(const std::vector<float>& legLengths,
    const std::vector<float>& speeds) {
    std::string s = "{\"positions\":[";
    for (int i = 0; i < 6; ++i) {
        if (i) s += ",";
        const int v = static_cast<int>(std::lround(legLengths[i]));
        s += (PLC_VALUES_ARE_STRINGS ? ("\"" + pad3(v) + "\"") : std::to_string(v));
    }
    s += "],\"speeds\":[";
    for (int i = 0; i < 6; ++i) {
        if (i) s += ",";
        const int v = static_cast<int>(std::lround(speeds[i]));
        s += (PLC_VALUES_ARE_STRINGS ? ("\"" + pad3(v) + "\"") : std::to_string(v));
    }
    s += "]}";
    return s;
}
// ===== [CHANGED] einde helpers =====

void CALLBACK SimConnectHandler::MyDispatchProcRD(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext) {
    static double rudder_deflection_deg = 0.0; // Variable to hold rudder deflection

    //Constraints for maximum attitude
    static const double PITCH_CONSTRAINT = 30;
    static const double ROLL_CONSTRAINT = 30;
    static const double YAW_CONSTRAINT = 30;

    switch (pData->dwID) {
    case SIMCONNECT_RECV_ID_SIMOBJECT_DATA: {
        SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData = (SIMCONNECT_RECV_SIMOBJECT_DATA*)pData;

        // Handle orientation data
        if (pObjData->dwRequestID == REQUEST_ORIENTATION) {
            AircraftOrientation* pS = (AircraftOrientation*)&pObjData->dwData;

            std::array<float, 6> currentLegLengths = tcpServer->getCurrentPositions();

            // Invert pitch and convert radians to degrees
            double pitch_deg = -pS->pitch * (180.0 / M_PI);  // Invert pitch
            double roll_deg = pS->bank * (180.0 / M_PI);
            double yaw_deg = rudder_deflection_deg * -1; // Assign yaw from rudder deflection

            // Store pre-clamp values for logging
            double preclamp_pitch = pitch_deg;
            double preclamp_roll = roll_deg;
            double preclamp_yaw = yaw_deg;

            // Clamp the values to the defined limits
            pitch_deg = clamp(pitch_deg, -PITCH_CONSTRAINT, PITCH_CONSTRAINT);
            roll_deg = clamp(roll_deg, -ROLL_CONSTRAINT, ROLL_CONSTRAINT);
            yaw_deg = clamp(yaw_deg, -YAW_CONSTRAINT, YAW_CONSTRAINT);

            std::cout << "Current actuator positions: ";
            for (float v : currentLegLengths) {
                std::cout << v << " ";
            }
            std::cout << std::endl;
           
            // Log warnings if the values are adjusted
            if (preclamp_pitch != pitch_deg) {
                std::cout << "Warning: Pitch adjusted to stay within limits: " << pitch_deg << " degrees\n";
            }
            if (preclamp_roll != roll_deg) {
                std::cout << "Warning: Roll adjusted to stay within limits: " << roll_deg << " degrees\n";
            }
            if (preclamp_yaw != yaw_deg) {
                std::cout << "Warning: Yaw adjusted to stay within limits: " << yaw_deg << " degrees\n";
            }

            // Calculate the leg lengths based on the current orientation
            float base_len = 1156.372420286821;  // Base leg length

            //float time_in_seconds = 1.0f / REFRESH_RATE; // Convert hertz to time in seconds

            /*for (size_t i = 0; i < 6; i++) {
                // Yaw_deg, roll_deg, pitch_deg
                float li = compute_li_length(start_height, yaw_deg, (roll_deg * -1), pitch_deg, platform_legs[i], base_legs[i]);
                // Append each computed leg length to the string
                legLengths[i] = std::round((li - base_len) + 200); // Store the computed leg length

                // Calculate speed based on leg length and time, enforce speed limit
                int calculatedSpeed = std::abs((legLengths[i] - currentLegLengths[i]) / time_in_seconds);
                float raw_speed = std::abs(calculatedSpeed / time_in_seconds); // Ensure speed is positive

                // Enforce the speed limit
                if (calculatedSpeed > SPEED_LIMIT) {
                    speeds[i] = SPEED_LIMIT;
                }
                else if (calculatedSpeed <= SPEED_LIMIT && calculatedSpeed > 0)
                {
                    speeds[i] = calculatedSpeed;
                }
                else if (calculatedSpeed <= 0) {
                    speeds[i] = 5;
                }
            }
            */
            float time_in_seconds = 1.0f / REFRESH_RATE; // 20 Hz → 0.05 s

            for (size_t i = 0; i < 6; i++) {
                // 1. Bereken “ideale” cilinderlengte uit de geometrie
                float li = compute_li_length(start_height, yaw_deg, (roll_deg * -1), pitch_deg, platform_legs[i], base_legs[i]);
                float desiredLength = std::round((li - base_len) + 200);  // oude legLengths[i]-logica

                // 2. Gebruik de teruggekoppelde positie als startpunt
                float currentLength = currentLegLengths[i];

                // 3. Begrens hoeveel we per cycle mogen veranderen (acceleration limiter)
                float delta = desiredLength - currentLength;
                float maxStep = MAX_STEP_PER_SEC * time_in_seconds;  // max verplaatsing in deze cycle

                if (delta > maxStep)       delta = maxStep;
                else if (delta < -maxStep) delta = -maxStep;

                // Nieuwe targetpositie = huidige positie + begrensde stap
                float limitedTarget = currentLength + delta;
                legLengths[i] = limitedTarget;

                // 4. Bepaal afstand die we in deze cycle willen afleggen
                float stepDistance = std::fabs(delta);

                // 5. Deadzone: als we bijna op positie zitten, stuur minimale snelheid
                float speed;
                if (stepDistance < POSITION_DEADZONE) {
                    speed = MIN_SPEED;
                }
                else {
                    // Bepaal gewenste snelheid op basis van afstand per tijd
                    float requestedSpeed = stepDistance / time_in_seconds; // units per seconde

                    // Begrens de snelheid
                    if (requestedSpeed > SPEED_LIMIT)   requestedSpeed = SPEED_LIMIT;
                    if (requestedSpeed < MIN_SPEED)     requestedSpeed = MIN_SPEED;

                    speed = requestedSpeed;
                }

                speeds[i] = speed;
            }

            // ===== [CHANGED] build & send payload per modus =====
            std::string payload;
#ifdef TARGET_PLC
            payload = buildPlcPayload(legLengths, speeds);
#else
            payload = buildUnityPayload(yaw_deg, roll_deg, pitch_deg, legLengths, speeds);
#endif

            tcpServer->sendData(payload);   // sendData voegt terminator toe
            logFile << payload << "\n";
            // ===== [CHANGED] einde =====
        }
        // Handle rudder deflection data
        else if (pObjData->dwRequestID == REQUEST_RUDDER) {
            RudderData* rudder = (RudderData*)&pObjData->dwData; // Cast to your rudder data struct
            rudder_deflection_deg = rudder->deflection; // Update rudder deflection
        }
        break;
    }

    case SIMCONNECT_RECV_ID_QUIT:
        std::cout << "Simulator has exited\n";
        tcpServer->closeConnection(); // Close connection on exit
        break;

    default:
        break;
    }
}

bool SimConnectHandler::InitializeSimConnect() {
    HRESULT hr;

    if (SUCCEEDED(SimConnect_Open(&hSimConnect, "Retrieve Aircraft Orientation", nullptr, 0, nullptr, 0))) {
        std::cout << "Connected to MSFS SimConnect\n";

        // Define the data structure we want to receive
        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_ORIENTATION, "PLANE PITCH DEGREES", "radians");
        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_ORIENTATION, "PLANE BANK DEGREES", "radians");

        // Add rudder deflection data definition
        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_RUDDER, "RUDDER DEFLECTION", "degrees");

        // Request data for orientation, wind, and rudder
        hr = SimConnect_RequestDataOnSimObject(hSimConnect, REQUEST_ORIENTATION, DEFINITION_ORIENTATION, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SIM_FRAME);
        hr = SimConnect_RequestDataOnSimObject(hSimConnect, REQUEST_RUDDER, DEFINITION_RUDDER, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SIM_FRAME);

        // Request data for orientation, wind, and rudder 1hz
        //hr = SimConnect_RequestDataOnSimObject(hSimConnect, REQUEST_ORIENTATION, DEFINITION_ORIENTATION, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SECOND);
        //hr = SimConnect_RequestDataOnSimObject(hSimConnect, REQUEST_RUDDER, DEFINITION_RUDDER, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SECOND);

        return true;
    }
    else {
        std::cout << "Unable to connect to MSFS SimConnect\n";
        return false;
    }
}

void SimConnectHandler::CloseSimConnect() {
    if (hSimConnect) {
        SimConnect_Close(hSimConnect);
        hSimConnect = nullptr;
    }
}
