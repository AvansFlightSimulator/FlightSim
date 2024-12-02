#include "TCPServer.h"
#include "SimConnectHandler.h"

#include <thread>
#include <windows.h>
#include <iostream>

TCPServer tcpServer;

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

void listeningThreadMethod() {
    while (true) {
        if (!tcpServer.isConnected) {
            break;  // Break if not connected
        }
        tcpServer.receiveData();
    }
}

void startPositionsThread(std::thread& listeningThread) {
    listeningThread = std::thread(&listeningThreadMethod);
}


int main() {
    SimConnectHandler handler = SimConnectHandler(&tcpServer);
    tcpServer.startListening();
    if (handler.InitializeSimConnect()) {
        std::cout << "Program running. Close the window to exit..." << std::endl;
        
        // Declare the listener thread
        std::thread listeningThread;

        // Start the listener thread
        startPositionsThread(listeningThread);

        // Main loop to keep receiving data until the window is closed
        while (true) {
            SimConnect_CallDispatch(hSimConnect, SimConnectHandler::MyDispatchProcRD, nullptr);
            Sleep(1);  // Small delay to reduce CPU usage

            // Process window messages
            if (!ProcessWindowsMessages()) {
                std::cout << "Window closed. Exiting..." << std::endl;
                break;
            }
        }

        // Join the listening thread before closing the program to ensure cleanup
        if (listeningThread.joinable()) {
            listeningThread.join();
        }

        // Close the connection when done
        handler.CloseSimConnect();
    }

    return 0;
}
