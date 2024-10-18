#include "TCPServer.h"

// Constructor initializes the server IP and port, but waits for client connections
TCPServer::TCPServer(const std::string& server_ip, int server_port)
    : server_sock(INVALID_SOCKET), client_sock(INVALID_SOCKET), isConnected(false) {
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        return; // Constructor exits early
    }

    // Create server socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        return; // Constructor exits early
    }

    // Set up the server address structure
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip.c_str(), &server_address.sin_addr);

    // Bind the socket
    if (bind(server_sock, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(server_sock);
        return; // Constructor exits early
    }

    startListening();
}

// Destructor closes the socket
TCPServer::~TCPServer() {
    closeConnection();
    WSACleanup();
}

// Start listening for incoming client connections
void TCPServer::startListening() {
    std::cout << "Server listening for connections..." << std::endl;
    if (listen(server_sock, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
        return;
    }

    while (true) {
        int client_size = sizeof(client_address);
        client_sock = accept(server_sock, (sockaddr*)&client_address, &client_size);
        if (client_sock == INVALID_SOCKET) {
            std::cerr << "Client accept failed: " << WSAGetLastError() << std::endl;
            continue;
        }
        else {
            isConnected = true;
            char ip_str[INET6_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &client_address.sin_addr, ip_str, sizeof(ip_str))) {
                std::cout << "Client connected from: " << ip_str << ":" << ntohs(client_address.sin_port) << std::endl;
            }
            else {
                std::cerr << "Failed to convert client IP address: " << WSAGetLastError() << std::endl;
            }
            break;
        }
    }
}


// Private function to accept a client connection
void TCPServer::acceptClient() {
    int client_size = sizeof(client_address);
    client_sock = accept(server_sock, (sockaddr*)&client_address, &client_size);
    if (client_sock == INVALID_SOCKET) {
        std::cerr << "Client accept failed: " << WSAGetLastError() << std::endl;
        closesocket(server_sock);
        return;
    }

    isConnected = true;

    // Use inet_ntop to convert the client IP address
    char ip_str[INET6_ADDRSTRLEN]; // Enough space for both IPv4 and IPv6
    if (inet_ntop(AF_INET, &client_address.sin_addr, ip_str, sizeof(ip_str))) {
        std::cout << "Client connected from: " << ip_str << ":" << ntohs(client_address.sin_port) << std::endl;
    }
    else {
        std::cerr << "Failed to convert client IP address: " << WSAGetLastError() << std::endl;
    }
}

// Function to send data to the client
void TCPServer::sendData(const std::string& data) {
    if (isConnected) {
        send(client_sock, data.c_str(), data.length(), 0);
        std::cout << data << std::endl;
    }
    else {
        std::cerr << "Cannot send data; no client connected." << std::endl;
    }
}

// Function to close the socket connection
void TCPServer::closeConnection() {
    if (client_sock != INVALID_SOCKET) {
        closesocket(client_sock);
        client_sock = INVALID_SOCKET;
        isConnected = false;
        std::cout << "Disconnected from client." << std::endl;
    }
    if (server_sock != INVALID_SOCKET) {
        closesocket(server_sock);
        server_sock = INVALID_SOCKET;
    }
}
