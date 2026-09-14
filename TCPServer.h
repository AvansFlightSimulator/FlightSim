#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

class DashboardModel;

class TCPServer {
public:
    static constexpr std::size_t ActuatorCount = 6;

    explicit TCPServer(
        const std::string& serverIp = "192.168.137.123",
        int serverPort = 32760,
        DashboardModel* dashboard = nullptr);
    ~TCPServer();

    TCPServer(const TCPServer&) = delete;
    TCPServer& operator=(const TCPServer&) = delete;

    // Blocks until a client connects or the server is shut down.
    bool startListening();

    // Sends/receives one newline-delimited JSON message. False means that the
    // client disconnected or an unrecoverable socket error occurred.
    bool sendData(const std::string& data);
    bool receiveData();

    // Closes both sockets and unblocks any pending accept/recv operation.
    void closeConnection();

    bool isConnected() const noexcept;
    bool hasPositionFeedback() const noexcept;
    std::array<float, ActuatorCount> getCurrentPositions() const;

private:
    void closeClientConnection();
    void enableKeepAlive(SOCKET socket, DWORD keepAliveTime, DWORD keepAliveInterval) const;
    void processMessage(const std::string& message);

    SOCKET serverSocket_;
    SOCKET clientSocket_;
    sockaddr_in serverAddress_{};
    sockaddr_in clientAddress_{};

    std::atomic<bool> connected_{ false };
    std::atomic<bool> hasPositionFeedback_{ false };
    std::atomic<bool> shuttingDown_{ false };
    bool winsockStarted_ = false;
    bool listeningStarted_ = false;

    mutable std::mutex connectionMutex_;
    mutable std::mutex positionsMutex_;
    std::mutex sendMutex_;

    std::array<float, ActuatorCount> currentPositions_{};
    std::string receiveBuffer_;
    DashboardModel* dashboard_ = nullptr;
};
