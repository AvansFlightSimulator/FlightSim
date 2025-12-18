#include "TCPServer.h"
#include "SimConnectHandler.h"

#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <windows.h>
#include <iostream>

#include "BuildMode.h"  // [CHANGED] modus-schakelaar toevoegen

// [CHANGED] instantiate TCPServer o.b.v. BuildMode
#ifdef TARGET_PLC
TCPServer tcpServer(BIND_IP_PLC, PORT_PLC);
#else
TCPServer tcpServer(BIND_IP_UNITY, PORT_UNITY);
#endif

// [NEW] Shared state voor 20Hz output thread
std::mutex dataMutex;  // Mutex voor thread-safe data sharing
std::vector<float> sharedLegLengths(6, 0.0f);  // Laatste berekende leg lengths
std::vector<float> sharedSpeeds(6, 0.0f);      // Laatste berekende speeds
std::string sharedPayload = "";                // Laatste gebouwde payload (voor Unity mode)
bool shouldStop = false;                        // Stop signal voor threads

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

// [NEW] 20Hz Output thread - stuurt data naar PLC op exacte 20Hz
void outputThreadMethod() {
    using namespace std::chrono;
    
    const auto interval = milliseconds(50);  // 20Hz = 50ms interval
    auto nextWakeup = steady_clock::now();
    
    // Timing verificatie variabelen
    int cycleCounter = 0;
    auto lastLogTime = steady_clock::now();
    
    std::cout << "[OUTPUT THREAD] Started 20Hz output thread" << std::endl;
    
    while (!shouldStop) {
        // Check if connected
        if (!tcpServer.isConnected) {
            std::this_thread::sleep_for(milliseconds(100));
            continue;
        }
        
        // Send data (mutex-protected)
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            if (!sharedPayload.empty()) {
                tcpServer.sendData(sharedPayload);
            }
        }
        
        // Timing verificatie: log elke 100 cycles (5 seconden bij 20Hz)
        cycleCounter++;
        if (cycleCounter % 100 == 0) {
            auto now = steady_clock::now();
            auto elapsed = duration_cast<milliseconds>(now - lastLogTime).count();
            std::cout << "[OUTPUT THREAD] 100 cycles in " << elapsed 
                      << "ms (target: 5000ms, error: " << (elapsed - 5000) << "ms)" << std::endl;
            lastLogTime = now;
        }
        
        // Wait until next cycle
        nextWakeup += interval;
        std::this_thread::sleep_until(nextWakeup);
    }
    
    std::cout << "[OUTPUT THREAD] Stopped" << std::endl;
}

void startOutputThread(std::thread& outputThread) {
    outputThread = std::thread(&outputThreadMethod);
}

int main() {
    SimConnectHandler handler = SimConnectHandler(&tcpServer);
    tcpServer.startListening();
    if (handler.InitializeSimConnect()) {
        std::cout << "Program running. Close the window to exit..." << std::endl;

        // [NEW] Declare both listener and output threads
        std::thread listeningThread;
        std::thread outputThread;

        // [NEW] Start both threads
        startPositionsThread(listeningThread);  // Ontvangt position feedback van PLC
        startOutputThread(outputThread);         // Stuurt data naar PLC op 20Hz

        // Main loop to keep receiving data until the window is closed
        while (true) {
            SimConnect_CallDispatch(hSimConnect, SimConnectHandler::MyDispatchProcRD, nullptr);
            Sleep(1);  // Small delay to reduce CPU usage

            // Process window messages
            if (!ProcessWindowsMessages()) {
                std::cout << "Window closed. Exiting..." << std::endl;
                shouldStop = true;  // Signal threads to stop
                break;
            }
        }

        // [NEW] Join both threads before closing
        if (outputThread.joinable()) {
            outputThread.join();
        }
        if (listeningThread.joinable()) {
            listeningThread.join();
        }

        // Close the connection when done
        handler.CloseSimConnect();
    }

    return 0;
}