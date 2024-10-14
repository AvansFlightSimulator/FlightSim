#include "SimConnectHandler.h"
#include <windows.h>
#include <iostream>

// Message handling function to process window events
bool ProcessWindowsMessages() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT || msg.message == WM_CLOSE) {
            return false;  // Close the program if WM_QUIT or WM_CLOSE is received
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;  // Continue running
}

int main() {
    if (InitializeSimConnect()) {
        std::cout << "Program running. Close the window to exit..." << std::endl;

        // Main loop to keep receiving data until the window is closed
        while (true) {
            SimConnect_CallDispatch(hSimConnect, MyDispatchProcRD, nullptr);
            Sleep(1);  // Small delay to reduce CPU usage

            // Process window messages
            if (!ProcessWindowsMessages()) {
                std::cout << "Window closed. Exiting..." << std::endl;
                break;
            }
        }

        // Close the connection when done
        CloseSimConnect();
    }

    return 0;
}
