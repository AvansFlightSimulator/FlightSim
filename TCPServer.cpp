#include "TCPServer.h"

#include "DashboardModel.h"
#include "ProtocolLogger.h"
#include "TelemetryStore.h"

#include <Mstcpip.h>
#include <iostream>
#include <sstream>

namespace {
    // Add an error event with the Winsock error code to the dashboard (if any).
    void AddSocketError(DashboardModel* dashboard, const std::string& message, int errorCode) {
        if (!dashboard) {
            return;
        }
        std::ostringstream event;
        event << message << " (Winsock " << errorCode << ')';
        dashboard->AddEvent(event.str(), DashboardEventLevel::Error);
    }
}

// Start Winsock, create the server socket, and bind it to the given IP/port.
TCPServer::TCPServer(const std::string& serverIp, int serverPort, DashboardModel* dashboard, TelemetryStore* telemetry)
    : serverSocket_(INVALID_SOCKET), clientSocket_(INVALID_SOCKET), dashboard_(dashboard), telemetry_(telemetry) {
    // Initialize Winsock 2.2.
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        if (dashboard_) {
            dashboard_->AddEvent("Winsock startup failed", DashboardEventLevel::Error);
        }
        return;
    }
    winsockStarted_ = true;

    // Create a TCP/IPv4 socket.
    serverSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket_ == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        if (dashboard_) {
            dashboard_->AddEvent("TCP socket creation failed", DashboardEventLevel::Error);
        }
        return;
    }

    // Build the bind address from the IP string and port.
    serverAddress_.sin_family = AF_INET;
    serverAddress_.sin_port = htons(static_cast<u_short>(serverPort));
    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddress_.sin_addr) != 1) {
        std::cerr << "Invalid server address: " << serverIp << std::endl;
        if (dashboard_) {
            dashboard_->AddEvent("Invalid TCP bind address", DashboardEventLevel::Error);
        }
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
        return;
    }

    // Bind the socket; close it on failure.
    if (bind(serverSocket_, reinterpret_cast<sockaddr*>(&serverAddress_), sizeof(serverAddress_)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        if (dashboard_) {
            dashboard_->AddEvent("Unable to bind the TCP endpoint", DashboardEventLevel::Error);
        }
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
    }
}

// Close all sockets and shut down Winsock.
TCPServer::~TCPServer() {
    closeConnection();
    if (winsockStarted_) {
        WSACleanup();
    }
}

// Start listening (first call only) and block until a client connects; true on success.
bool TCPServer::startListening() {
    // Copy the server socket under lock.
    SOCKET listeningSocket = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        listeningSocket = serverSocket_;
    }

    if (shuttingDown_ || listeningSocket == INVALID_SOCKET) {
        return false;
    }

    // Put the socket into listening mode once.
    if (!listeningStarted_) {
        if (listen(listeningSocket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
            if (dashboard_) {
                dashboard_->SetTcpListening(false);
                dashboard_->AddEvent("TCP listener failed", DashboardEventLevel::Error);
            }
            return false;
        }
        listeningStarted_ = true;
        if (dashboard_) {
            dashboard_->SetTcpListening(true);
            dashboard_->AddEvent("TCP endpoint is listening");
        }
    }

    // Block until a client connects.
    std::cout << "[TCP] Waiting for a client..." << std::endl;
    int clientSize = sizeof(clientAddress_);
    const SOCKET acceptedSocket = accept(
        listeningSocket,
        reinterpret_cast<sockaddr*>(&clientAddress_),
        &clientSize);

    // Accept fails when shutting down; only report unexpected failures.
    if (acceptedSocket == INVALID_SOCKET) {
        if (!shuttingDown_) {
            std::cerr << "Client accept failed: " << WSAGetLastError() << std::endl;
            AddSocketError(dashboard_, "Client accept failed", WSAGetLastError());
        }
        return false;
    }

    // Store the new client, unless shutdown started meanwhile.
    bool rejectConnection = false;
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        if (shuttingDown_) {
            rejectConnection = true;
        }
        else {
            clientSocket_ = acceptedSocket;
            connected_ = true;
            hasPositionFeedback_ = false;
        }
    }
    if (rejectConnection) {
        shutdown(acceptedSocket, SD_BOTH);
        closesocket(acceptedSocket);
        return false;
    }

    // Reset the receive buffer, start a new connection ID, and enable keep-alive (20 s idle, 1 s interval).
    feedbackStream_.Clear();
    ++connectionId_;
    if (telemetry_) telemetry_->Record("connect", "", connectionId_);
    enableKeepAlive(acceptedSocket, 20000, 1000);
    if (dashboard_) {
        dashboard_->SetClientConnected(true);
    }

    // Log the client's IP address and port.
    char ipString[INET_ADDRSTRLEN]{};
    if (inet_ntop(AF_INET, &clientAddress_.sin_addr, ipString, sizeof(ipString))) {
        std::cout << "[TCP] Connected: " << ipString << ':' << ntohs(clientAddress_.sin_port) << std::endl;
        if (dashboard_) {
            dashboard_->AddEvent(std::string("Controller connected from ") + ipString);
        }
    }
    else {
        std::cerr << "Failed to convert client IP address: " << WSAGetLastError() << std::endl;
        AddSocketError(dashboard_, "Unable to display the client address", WSAGetLastError());
    }

    return true;
}

// Enable TCP keep-alive with custom idle time and probe interval (ms) to detect dead links.
void TCPServer::enableKeepAlive(SOCKET socket, DWORD keepAliveTime, DWORD keepAliveInterval) const {
    // Turn keep-alive on.
    BOOL enabled = TRUE;
    if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&enabled), sizeof(enabled)) == SOCKET_ERROR) {
        std::cerr << "Failed to enable TCP keep-alive: " << WSAGetLastError() << std::endl;
        AddSocketError(dashboard_, "TCP keep-alive could not be enabled", WSAGetLastError());
        return;
    }

    // Set keep-alive timing (Windows-specific).
    tcp_keepalive settings{};
    settings.onoff = 1;
    settings.keepalivetime = keepAliveTime;
    settings.keepaliveinterval = keepAliveInterval;

    DWORD bytesReturned = 0;
    if (WSAIoctl(
        socket,
        SIO_KEEPALIVE_VALS,
        &settings,
        sizeof(settings),
        nullptr,
        0,
        &bytesReturned,
        nullptr,
        nullptr) == SOCKET_ERROR) {
        std::cerr << "Failed to configure TCP keep-alive: " << WSAGetLastError() << std::endl;
        AddSocketError(dashboard_, "TCP keep-alive setup failed", WSAGetLastError());
    }
}

// Send one newline-terminated message to the client; false on failure.
bool TCPServer::sendData(const std::string& data) {
    // Capture the connection ID for telemetry before sending.
    const auto connection = connectionId_.load();
    if (!connected_) {
        return false;
    }

    const std::string framedMessage = data + '\n';
    bool sendFailed = false;

    {
        // One sender at a time.
        std::lock_guard<std::mutex> sendLock(sendMutex_);

        SOCKET socket = INVALID_SOCKET;
        {
            std::lock_guard<std::mutex> connectionLock(connectionMutex_);
            socket = clientSocket_;
        }

        if (socket == INVALID_SOCKET) {
            return false;
        }

        // Loop until the whole message is sent (send may be partial).
        std::size_t sentBytes = 0;
        while (sentBytes < framedMessage.size()) {
            const int result = send(
                socket,
                framedMessage.data() + sentBytes,
                static_cast<int>(framedMessage.size() - sentBytes),
                0);
            if (result == SOCKET_ERROR || result == 0) {
                sendFailed = true;
                break;
            }
            sentBytes += static_cast<std::size_t>(result);
        }
    }

    // On failure, report and drop the client.
    if (sendFailed) {
        std::cerr << "Send failed: " << WSAGetLastError() << std::endl;
        AddSocketError(dashboard_, "Command send failed", WSAGetLastError());
        closeClientConnection();
        return false;
    }

    if (telemetry_) telemetry_->Record("send", data, connection);
    return true;
}

// Read available bytes from the client and process each complete message; false on disconnect/error.
bool TCPServer::receiveData() {
    SOCKET socket = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        socket = clientSocket_;
    }
    if (socket == INVALID_SOCKET) {
        return false;
    }

    // Blocking read of up to 4 KiB.
    char buffer[4096];
    const int bytesReceived = recv(socket, buffer, sizeof(buffer), 0);
    // Error: report unless shutting down, then drop the client.
    if (bytesReceived == SOCKET_ERROR) {
        if (!shuttingDown_) {
            std::cerr << "Receive failed: " << WSAGetLastError() << std::endl;
            AddSocketError(dashboard_, "Position receive failed", WSAGetLastError());
        }
        closeClientConnection();
        return false;
    }
    // 0 bytes means the client closed the connection.
    if (bytesReceived == 0) {
        std::cout << "[TCP] Client disconnected." << std::endl;
        closeClientConnection();
        return false;
    }

    // Split the stream into complete messages and handle each one.
    const auto batch = feedbackStream_.Append(buffer, static_cast<std::size_t>(bytesReceived));
    for (const auto& message : batch.messages) {
        processMessage(message);
    }
    if (batch.oversizedRemainderDiscarded) {
        std::cerr << "Incoming message exceeded 64 KiB and was discarded." << std::endl;
        if (dashboard_) {
            dashboard_->AddEvent("Oversized feedback message discarded", DashboardEventLevel::Warning);
        }
    }

    return true;
}

// Parse one feedback message and update the stored actuator positions.
void TCPServer::processMessage(const std::string& message) {
    // Reject and log messages that fail to parse.
    ActuatorValues updatedPositions{};
    std::string error;
    if (!TryParseFeedback(message, updatedPositions, error)) {
        if (telemetry_) telemetry_->Record("rejected-feedback", message, connectionId_);
        std::cerr << error << std::endl;
        if (dashboard_) {
            dashboard_->AddEvent(error, DashboardEventLevel::Warning);
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(positionsMutex_);
        currentPositions_ = updatedPositions;
    }
    if (dashboard_) {
        dashboard_->UpdateFeedback(updatedPositions);
    }
    // Announce the first valid feedback on this connection (enables output).
    const bool firstFeedback = !hasPositionFeedback_.exchange(true);
    if (firstFeedback) {
        std::cout << "[TCP] Position feedback received; actuator output enabled." << std::endl;
        if (dashboard_) {
            dashboard_->AddEvent("Position feedback valid; output enabled");
        }
    }
    LogProtocolMessage("RECV", message);
    if (telemetry_) telemetry_->Record("feedback", message, connectionId_);
}

// True while a client is connected.
bool TCPServer::isConnected() const noexcept {
    return connected_;
}

// True once valid position feedback has arrived on the current connection.
bool TCPServer::hasPositionFeedback() const noexcept {
    return hasPositionFeedback_;
}

// Thread-safe copy of the latest actuator positions.
ActuatorValues TCPServer::getCurrentPositions() const {
    std::lock_guard<std::mutex> lock(positionsMutex_);
    return currentPositions_;
}

// Disconnect the current client and reset connection state.
void TCPServer::closeClientConnection() {
    // Take the socket and clear state under lock.
    SOCKET socket = INVALID_SOCKET;
    bool wasConnected = false;
    {
        std::lock_guard<std::mutex> connectionLock(connectionMutex_);
        socket = clientSocket_;
        clientSocket_ = INVALID_SOCKET;
        wasConnected = connected_;
        connected_ = false;
        hasPositionFeedback_ = false;
    }

    // Report the disconnect only if a client was actually connected.
    if (dashboard_) {
        dashboard_->SetClientConnected(false);
        if (wasConnected) {
            dashboard_->AddEvent("Controller disconnected", DashboardEventLevel::Warning);
        }
    }
    if (wasConnected && telemetry_) telemetry_->Record("disconnect", "", connectionId_);

    if (socket != INVALID_SOCKET) {
        // Interrupt a blocking send/recv before waiting for the send lock.
        shutdown(socket, SD_BOTH);
        std::lock_guard<std::mutex> sendLock(sendMutex_);
        closesocket(socket);
    }
}

// Shut down the server: close the client and listening sockets (runs only once).
void TCPServer::closeConnection() {
    // Skip if shutdown already started.
    if (shuttingDown_.exchange(true)) {
        return;
    }

    closeClientConnection();

    // Take and close the listening socket (also unblocks accept).
    SOCKET serverSocket = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        serverSocket = serverSocket_;
        serverSocket_ = INVALID_SOCKET;
    }

    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
    }
    if (dashboard_) {
        dashboard_->SetTcpListening(false);
    }
}