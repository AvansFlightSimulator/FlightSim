#ifndef SIMCONNECTHANDLER_H
#define SIMCONNECTHANDLER_H

#include <windows.h>
#include <SimConnect.h>
#include <iostream>
#include <fstream>
#include "TCPServer.h"
#include "UnityConnection.h"

// Define a data structure to hold the roll, pitch, and yaw
struct AircraftOrientation {
    double pitch;   // Pitch angle in radians
    double bank;    // Roll angle in radians (also known as bank)
    double heading; // Heading angle in radians (also known as yaw)
};

struct RudderData {
    double deflection; // Rudder deflection in degrees
};

// Simulation event IDs
enum DATA_DEFINE_ID {
    DEFINITION_ORIENTATION,
    DEFINITION_RUDDER
};

// Request IDs
enum DATA_REQUEST_ID {
    REQUEST_ORIENTATION,
    REQUEST_RUDDER
};

extern HANDLE hSimConnect;

class SimConnectHandler {
public:
    SimConnectHandler(TCPServer* server);

    // Callback function to handle SimConnect data reception
    static void CALLBACK MyDispatchProcRD(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext);

    // Function to initialize the connection to SimConnect and request data
    bool InitializeSimConnect();

    // Function to close the SimConnect connection
    void CloseSimConnect();

    std::vector<float> dataForUnity(9); // To hold data(leglengths + yaw,pitch,roll)

private:    
};



#endif // SIMCONNECTHANDLER_H
