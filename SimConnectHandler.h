#ifndef SIMCONNECTHANDLER_H
#define SIMCONNECTHANDLER_H

#include <windows.h>
#include <SimConnect.h>

// Define a data structure to hold the roll, pitch, and yaw
struct AircraftOrientation {
    double pitch;   // Pitch angle in radians
    double bank;    // Roll angle in radians (also known as bank)
    double heading; // Heading angle in radians (also known as yaw)
};

// Define a data structure to hold wind information
struct WindData {
    float direction; // Wind direction in degrees
    float speed;     // Wind speed in knots
};

struct RudderData {
    double deflection; // Rudder deflection in degrees
};

// Simulation event IDs
enum DATA_DEFINE_ID {
    DEFINITION_ORIENTATION,
    DEFINITION_WIND, // Added wind data definition
    DEFINITION_RUDDER
};

// Request IDs
enum DATA_REQUEST_ID {
    REQUEST_ORIENTATION,
    REQUEST_WIND, // Added wind data request
    REQUEST_RUDDER
};

extern HANDLE hSimConnect;

// Callback function to handle SimConnect data reception
void CALLBACK MyDispatchProcRD(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext);

// Function to initialize the connection to SimConnect and request data
bool InitializeSimConnect();

// Function to close the SimConnect connection
void CloseSimConnect();

#endif // SIMCONNECTHANDLER_H
