#include "TCPServer.h"
#include "SimConnectHandler.h"
#include "calculate_legs.h"  // Include the calculate_legs header
#include <windows.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <cmath>  // For M_PI


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

HANDLE hSimConnect = nullptr;

TCPServer tcpServer;

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
    {537.69, 515.43, 0},
    {-537.69, 515.43, 0},
    {-715.23, 207.94, 0},
    {-177.53, -723.37, 0},
    {177.53, -723.37, 0},
    {715.23, 207.94, 0}
};

vec start_height{ 0, 0, 1079 };  // Starting height of the platform
std::vector<float> legLengths(6); // To hold the computed leg lengths
std::vector<float> speeds(6);



#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif



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

void CALLBACK MyDispatchProcRD(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext) {
    static double wind_direction_deg = 0.0; // Variable to hold wind direction
    static double rudder_deflection_deg = 0.0; // Variable to hold rudder deflection

    static const double PITCH_CONSTRAINT = 9;
    static const double ROLL_CONSTRAINT = 13;
    static const double YAW_CONSTRAINT = 5;

    switch (pData->dwID) {
    case SIMCONNECT_RECV_ID_SIMOBJECT_DATA: {
        SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData = (SIMCONNECT_RECV_SIMOBJECT_DATA*)pData;

        // Handle orientation data
        if (pObjData->dwRequestID == REQUEST_ORIENTATION) {
            AircraftOrientation* pS = (AircraftOrientation*)&pObjData->dwData;

            // Invert pitch and convert radians to degrees
            double pitch_deg = -pS->pitch * (180.0 / M_PI);  // Invert pitch
            double roll_deg = pS->bank * (180.0 / M_PI);
            double yaw_deg = rudder_deflection_deg; // Assign yaw from rudder deflection

            // Store pre-clamp values for logging
            double preclamp_pitch = pitch_deg;
            double preclamp_roll = roll_deg;
            double preclamp_yaw = yaw_deg;

            // Clamp the values to the defined limits
            pitch_deg = clamp(pitch_deg, -PITCH_CONSTRAINT, PITCH_CONSTRAINT);
            roll_deg = clamp(roll_deg, -ROLL_CONSTRAINT, ROLL_CONSTRAINT);
            yaw_deg = clamp(yaw_deg, -YAW_CONSTRAINT, YAW_CONSTRAINT);

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

            for (size_t i = 0; i < 6; i++) {
                float li = compute_li_length(start_height, 0, 0, pitch_deg, platform_legs[i], base_legs[i]);
                std::cout << "Leg " << i + 1 << ": " << std::round(li - base_len) << " units\n";

                // Append each computed leg length to the string
                legLengths[i] = std::round(li - base_len); // Store the computed leg length
                speeds[i] = 200;
            }

            nlohmann::json j;


            // Store the computed leg lengths in the JSON object under "positions"
            j["positions"] = legLengths; // legLengths contains the leg lengths
            j["speeds"] = speeds;

            // Send the JSON data
            tcpServer.sendData(j.dump()); // Send the JSON as a string over TCP connection

        }
        // Handle wind data
        else if (pObjData->dwRequestID == REQUEST_WIND) {
            WindData* wind = (WindData*)&pObjData->dwData; // Cast to your wind data struct
            wind_direction_deg = wind->direction; // Update wind direction
            std::cout << "Wind Direction: " << wind_direction_deg << " degrees\n"; // Optional debug output
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
        tcpServer.closeConnection(); // Close connection on exit
        break;

    default:
        break;
    }
}

bool InitializeSimConnect() {
    HRESULT hr;

    if (SUCCEEDED(SimConnect_Open(&hSimConnect, "Retrieve Aircraft Orientation", nullptr, 0, nullptr, 0))) {
        std::cout << "Connected to MSFS SimConnect\n";

        // Define the data structure we want to receive
        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_ORIENTATION, "PLANE PITCH DEGREES", "radians");
        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_ORIENTATION, "PLANE BANK DEGREES", "radians");
        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_ORIENTATION, "PLANE HEADING DEGREES TRUE", "radians");

        // Add wind data definition
        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_WIND, "WIND DIRECTION", "degrees");
        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_WIND, "WIND SPEED", "knots");

        // Add rudder deflection data definition
        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_RUDDER, "RUDDER DEFLECTION", "degrees");

        // Request data for orientation, wind, and rudder
        hr = SimConnect_RequestDataOnSimObject(hSimConnect, REQUEST_ORIENTATION, DEFINITION_ORIENTATION, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SIM_FRAME);
        hr = SimConnect_RequestDataOnSimObject(hSimConnect, REQUEST_WIND, DEFINITION_WIND, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SIM_FRAME);
        hr = SimConnect_RequestDataOnSimObject(hSimConnect, REQUEST_RUDDER, DEFINITION_RUDDER, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SIM_FRAME);

        // Request data for orientation, wind, and rudder 1hz
        //hr = SimConnect_RequestDataOnSimObject(hSimConnect, REQUEST_ORIENTATION, DEFINITION_ORIENTATION, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SECOND);
        //hr = SimConnect_RequestDataOnSimObject(hSimConnect, REQUEST_WIND, DEFINITION_WIND, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SECOND);
        //hr = SimConnect_RequestDataOnSimObject(hSimConnect, REQUEST_RUDDER, DEFINITION_RUDDER, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SECOND);

        return true;
    }
    else {
        std::cout << "Unable to connect to MSFS SimConnect\n";
        return false;
    }
}

void CloseSimConnect() {
    if (hSimConnect) {
        SimConnect_Close(hSimConnect);
        hSimConnect = nullptr;
    }
}
