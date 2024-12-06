#ifndef UNITY_CONNECTION_H
#define UNITY_CONNECTION_H
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <string>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <iomanip> // for std::fixed and std::setprecision
#include <sstream> 
#include <Mstcpip.h> // For SIO_KEEPALIVE_VALS on Windows

// #include <nlohmann/json.hpp> // Include the nlohmann JSON library

#pragma comment(lib, "ws2_32.lib") // Link with ws2_32.lib

class UnityConnection {
public:
    UnityConnection(const std::string& server_ip = "127.0.0.1", int server_port = 4844);

    // Destructor to clean up resources
    ~UnityConnection();

    void startListening();                  // Start listening for a single client
    void sendData(const std::string& data); // Send data to the connected client
    void closeUnityConnection();           // Close the connection and cleanup
    std::array<float, 6> getCurrentPositions() const; // Getter for positions
    void enableKeepAlive(SOCKET sock, DWORD keepAliveTime, DWORD keepAliveInterval);
    std::string createJson(float yaw, float roll, float pitch, const std::vector<float>& legs);
    bool isConnected = false;
    bool sendDataToUnity = false;   //when this is true a dedicated thread will send data to Unity, mayby make it atomic
private:
    std::string server_ip_;
    int server_port_;
    SOCKET server_socket_; // For accepting client socket
    SOCKET unity_socket_; // Unity socket
    
    int initializeWinsock();  // Initialize Winsock
    void cleanupWinsock();     // Cleanup Winsock
    void createSocket();       // Create the server socket
    void bindSocket();         // Bind socket to IP and port
    void listenForClient();    // Listen for a single client connection
};


#endif