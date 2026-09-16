#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "ControllerProtocol.h"
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
    explicit TCPServer(
        const std::string& serverIp,
        int serverPort,
        DashboardModel* dashboard = nullptr);
    ~TCPServer();

    TCPServer(const TCPServer&) = delete;
    TCPServer& operator=(const TCPServer&) = delete;

    // Blocks until a client connects or the server is shut down.
    bool startListening();

    // Sends one JSON object, adding its newline. Receive reads a TCP chunk,
    // which may contain partial or multiple messages. False means disconnected
    // or an unrecoverable socket error. Use one worker per direction.
    bool sendData(const std::string& data);
    bool receiveData();

    // Closes both sockets and unblocks any pending accept/recv operation.
    void closeConnection();

    bool isConnected() const noexcept;
    bool hasPositionFeedback() const noexcept;
    ActuatorValues getCurrentPositions() const;

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

    // Never hold connectionMutex_ during blocking socket calls. sendMutex_
    // prevents interleaved sends; positionsMutex_ protects feedback snapshots.
    mutable std::mutex connectionMutex_;
    mutable std::mutex positionsMutex_;
    std::mutex sendMutex_;

    ActuatorValues currentPositions_{};
    // Only the feedback worker accesses stream state and accepts clients.
    FeedbackStream feedbackStream_;
    DashboardModel* dashboard_ = nullptr;
};
