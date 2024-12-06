#include "UnityConnection.h"

UnityConnection::UnityConnection(const std::string& server_ip, int server_port)
    : server_ip_(server_ip), server_port_(server_port), server_socket_(INVALID_SOCKET), unity_socket_(INVALID_SOCKET) {
    initializeWinsock();
    createSocket();
    bindSocket();
}

UnityConnection::~UnityConnection() {
    closeUnityConnection();
    cleanupWinsock();
}

int UnityConnection::initializeWinsock() {
    WSADATA wsaData;
    int wsaerr;
    WORD wVersionRequested = MAKEWORD(2, 2);
    wsaerr = WSAStartup(wVersionRequested, &wsaData);

    // Check for initialization success
    if (wsaerr != 0) {
        std::cout << "The Winsock dll not found!" << std::endl;
        return 0;
    } else {
        std::cout << "The Winsock dll found" << std::endl;
        std::cout << "The status: " << wsaData.szSystemStatus << std::endl;
        return 0;
    }
}

void UnityConnection::cleanupWinsock() {
    WSACleanup();
}

void UnityConnection::createSocket() {
    server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket_ == INVALID_SOCKET) {
        throw std::runtime_error("[UnityConnection] Failed to create socket");
    }
}

void UnityConnection::bindSocket() {
    sockaddr_in service{};
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr(server_ip_.c_str());
    service.sin_port = htons(server_port_);

    if (bind(server_socket_, reinterpret_cast<SOCKADDR*>(&service), sizeof(service)) == SOCKET_ERROR) {
        throw std::runtime_error("[UnityConnection] Failed to bind socket");
    }
}

void UnityConnection::enableKeepAlive(SOCKET sock, DWORD keepAliveTime, DWORD keepAliveInterval) {
    // Enable TCP keep-alive
    BOOL optval = TRUE;
    if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&optval, sizeof(optval)) == SOCKET_ERROR) {
        std::cerr << "[UnityConnection] Failed to enable TCP keep-alive: " << WSAGetLastError() << std::endl;
        return;
    }

    // Configure keep-alive parameters
    struct tcp_keepalive keepAliveSettings;
    keepAliveSettings.onoff = 1; // Enable keep-alive
    keepAliveSettings.keepalivetime = keepAliveTime; // Time in milliseconds before sending keep-alive probes
    keepAliveSettings.keepaliveinterval = keepAliveInterval; // Interval in milliseconds between probes

    DWORD bytesReturned;
    if (WSAIoctl(sock, SIO_KEEPALIVE_VALS, &keepAliveSettings, sizeof(keepAliveSettings), NULL, 0, &bytesReturned, NULL, NULL) == SOCKET_ERROR) {
        std::cerr << "[UnityConnection] Failed to set keep-alive parameters: " << WSAGetLastError() << std::endl;
    }
    else {
        std::cout << "[UnityConnection] Keep-alive enabled (time: " << keepAliveTime << " ms, interval: " << keepAliveInterval << " ms)." << std::endl;
    }
}

void UnityConnection::listenForClient() {
    if (listen(server_socket_, 1) == SOCKET_ERROR) {
        throw std::runtime_error("[UnityConnection] Error listening on socket");
    }
    std::cout << "[UnityConnection] Listening for a single connection..." << std::endl;

    unity_socket_ = accept(server_socket_, nullptr, nullptr);
    if (unity_socket_ == INVALID_SOCKET) {
        throw std::runtime_error("[UnityConnection] Failed to accept client connection");
    }
    std::cout << "[UnityConnection] Client connected!" << std::endl;
    isConnected = true;

    enableKeepAlive(unity_socket_, 20000,1000);
}

void UnityConnection::startListening() {
    listenForClient();
}

void UnityConnection::sendData(const std::string& data) {
    if (unity_socket_ == INVALID_SOCKET) {
        throw std::runtime_error("[UnityConnection] No connected client to send data to");
    }

    int bytes_sent = send(unity_socket_, data.c_str(), data.size(), 0);
    if (bytes_sent == SOCKET_ERROR) {
        throw std::runtime_error("[UnityConnection] Failed to send data");
    }

    // std::cout << "Sent " << bytes_sent << " bytes to the client." << std::endl;
}

void UnityConnection::closeUnityConnection() {
    if (unity_socket_ != INVALID_SOCKET) {
        closesocket(unity_socket_);
        unity_socket_ = INVALID_SOCKET;
    }
    if (server_socket_ != INVALID_SOCKET) {
        closesocket(server_socket_);
        server_socket_ = INVALID_SOCKET;
    }
}

std::string UnityConnection::createJson(float yaw, float roll, float pitch, const std::vector<float>& legs) {
    std::ostringstream oss;

    oss << std::fixed << std::setprecision(2) // Set precision for float
        << "{\n"
        << "    \"orientation\": {\n"
        << "        \"yaw\": " << yaw << ",\n"
        << "        \"roll\": " << roll << ",\n"
        << "        \"pitch\": " << pitch << "\n"
        << "    },\n"
        << "    \"legs\": [";

    for (size_t i = 0; i < legs.size(); ++i) {
        oss << legs[i];
        if (i < legs.size() - 1) {
            oss << ", ";
        }
    }

    oss << "]\n"
        << "}";

    return oss.str();
}



