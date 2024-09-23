#include "../include/mfs_input_strategy.h"

#include <iostream>
#include <thread>

#ifdef _WIN32

bool MFSInputStrategy::isOK() {
	return true;
}

bool MFSInputStrategy::connect(std::function<void(bool)> onError) {
    std::cout << "connecting to MSFS...\n";

    int waited = 0;
    while (!SUCCEEDED(SimConnect_Open(&simConnect, "Client Event", NULL, NULL, NULL, NULL))) {
        if (waited >= maxWaitInSec) {
            onError(false);
        }

        // wait x time
        std::this_thread::sleep_for(std::chrono::seconds(waitBetweenTriesInSec));
        waited += waitBetweenTriesInSec;
    }

    std::cout << "connected to MSFS\n";

    SimConnect_AddToDataDefinition(simConnect, DEF1, "PLANE PITCH DEGREES", "degrees", SIMCONNECT_DATATYPE_FLOAT32);
    SimConnect_AddToDataDefinition(simConnect, DEF1, "PLANE BANK DEGREES", "degrees", SIMCONNECT_DATATYPE_FLOAT32);
    SimConnect_AddToDataDefinition(simConnect, DEF1, "RUDDER POSITION", "degrees", SIMCONNECT_DATATYPE_FLOAT32);

    SimConnect_RequestDataOnSimObject(simConnect, REQ1, DEF1, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SIM_FRAME);

    this->onError = onError;
    return true;
}

FlightData MFSInputStrategy::getInput() {
    SIMCONNECT_RECV* pData;
	DWORD cbData;

	if (SUCCEEDED(SimConnect_GetNextDispatch(simConnect, &pData, &cbData))) {
        switch (pData->dwID)
		{
		default:
			break;

		case SIMCONNECT_RECV_ID_SIMOBJECT_DATA:
		{
			//Cast pData to a simobject data object pointer.
			SIMCONNECT_RECV_SIMOBJECT_DATA* objectData = (SIMCONNECT_RECV_SIMOBJECT_DATA*)pData;

			if (objectData->dwRequestID == REQ1)
			{
				FlightData fd = *(FlightData*)&objectData->dwData;

				fd.bank = -fd.bank;
				fd.rudder = -fd.rudder;

				fd.transX = 0.0f;
				fd.transY = 0.0f;

				return fd;
			}
			break;
		}

		case SIMCONNECT_RECV_ID_QUIT:
		{
			onError(false);
			break;
		}

		}
    }
	return FlightData();
}

#endif
