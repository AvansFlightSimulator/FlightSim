#include "TCPServer.h"

#include "DashboardModel.h"
#include "ProtocolLogger.h"

#include <Mstcpip.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace {
constexpr std::size_t MaximumBufferedMessageSize = 64 * 1024;

void AddSocketError(DashboardModel* dashboard, const std::string& message, int errorCode) {
    if (!dashboard) {
        return;
    }
    std::ostringstream event;
    event << message << " (Winsock " << errorCode << ')';
    dashboard->AddEvent(event.str(), DashboardEventLevel::Error);
}
}

constexpr std::size_t TCPServer::ActuatorCount;

TCPServer::TCPServer(const std::string& serverIp, int serverPort, DashboardModel* dashboard)
    : serverSocket_(INVALID_SOCKET), clientSocket_(INVALID_SOCKET), dashboard_(dashboard) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        if (dashboard_) {
            dashboard_->AddEvent("Winsock startup failed", DashboardEventLevel::Error);
        }
        return;
    }
    winsockStarted_ = true;

    serverSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket_ == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        if (dashboard_) {
            dashboard_->AddEvent("TCP socket creation failed", DashboardEventLevel::Error);
        }
        return;
    }

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

    if (bind(serverSocket_, reinterpret_cast<sockaddr*>(&serverAddress_), sizeof(serverAddress_)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        if (dashboard_) {
            dashboard_->AddEvent("Unable to bind the TCP endpoint", DashboardEventLevel::Error);
        }
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
    }
}

TCPServer::~TCPServer() {
    closeConnection();
    if (winsockStarted_) {
        WSACleanup();
    }
}

bool TCPServer::startListening() {
    SOCKET listeningSocket = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        listeningSocket = serverSocket_;
    }

    if (shuttingDown_ || listeningSocket == INVALID_SOCKET) {
        return false;
    }

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

    std::cout << "[TCP] Waiting for a client..." << std::endl;
    int clientSize = sizeof(clientAddress_);
    const SOCKET acceptedSocket = accept(
        listeningSocket,
        reinterpret_cast<sockaddr*>(&clientAddress_),
        &clientSize);

    if (acceptedSocket == INVALID_SOCKET) {
        if (!shuttingDown_) {
            std::cerr << "Client accept failed: " << WSAGetLastError() << std::endl;
            AddSocketError(dashboard_, "Client accept failed", WSAGetLastError());
        }
        return false;
    }

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

    receiveBuffer_.clear();
    enableKeepAlive(acceptedSocket, 20000, 1000);
    if (dashboard_) {
        dashboard_->SetClientConnected(true);
    }

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

void TCPServer::enableKeepAlive(SOCKET socket, DWORD keepAliveTime, DWORD keepAliveInterval) const {
    BOOL enabled = TRUE;
    if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&enabled), sizeof(enabled)) == SOCKET_ERROR) {
        std::cerr << "Failed to enable TCP keep-alive: " << WSAGetLastError() << std::endl;
        AddSocketError(dashboard_, "TCP keep-alive could not be enabled", WSAGetLastError());
        return;
    }

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

bool TCPServer::sendData(const std::string& data) {
    if (!connected_) {
        return false;
    }

    const std::string framedMessage = data + '\n';
    bool sendFailed = false;

    {
        std::lock_guard<std::mutex> sendLock(sendMutex_);

        SOCKET socket = INVALID_SOCKET;
        {
            std::lock_guard<std::mutex> connectionLock(connectionMutex_);
            socket = clientSocket_;
        }

        if (socket == INVALID_SOCKET) {
            return false;
        }

        std::size_t sentBytes = 0;
        while (socket != INVALID_SOCKET && sentBytes < framedMessage.size()) {
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

    if (sendFailed) {
        std::cerr << "Send failed: " << WSAGetLastError() << std::endl;
        AddSocketError(dashboard_, "Command send failed", WSAGetLastError());
        closeClientConnection();
        return false;
    }

    return true;
}

bool TCPServer::receiveData() {
    SOCKET socket = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        socket = clientSocket_;
    }
    if (socket == INVALID_SOCKET) {
        return false;
    }

    char buffer[4096];
    const int bytesReceived = recv(socket, buffer, sizeof(buffer), 0);
    if (bytesReceived == SOCKET_ERROR) {
        if (!shuttingDown_) {
            std::cerr << "Receive failed: " << WSAGetLastError() << std::endl;
            AddSocketError(dashboard_, "Position receive failed", WSAGetLastError());
        }
        closeClientConnection();
        return false;
    }
    if (bytesReceived == 0) {
        std::cout << "[TCP] Client disconnected." << std::endl;
        closeClientConnection();
        return false;
    }

    receiveBuffer_.append(buffer, static_cast<std::size_t>(bytesReceived));

    std::size_t delimiter = receiveBuffer_.find('\n');
    while (delimiter != std::string::npos) {
        std::string message = receiveBuffer_.substr(0, delimiter);
        receiveBuffer_.erase(0, delimiter + 1);
        if (!message.empty() && message.back() == '\r') {
            message.pop_back();
        }
        if (!message.empty()) {
            processMessage(message);
        }
        delimiter = receiveBuffer_.find('\n');
    }

    // Backward compatibility for clients that send one complete JSON object
    // without a newline. Fragmented objects stay buffered until complete.
    if (!receiveBuffer_.empty()) {
        const nlohmann::json candidate = nlohmann::json::parse(receiveBuffer_, nullptr, false);
        if (!candidate.is_discarded()) {
            processMessage(receiveBuffer_);
            receiveBuffer_.clear();
        }
    }

    if (receiveBuffer_.size() > MaximumBufferedMessageSize) {
        std::cerr << "Incoming message exceeded 64 KiB and was discarded." << std::endl;
        if (dashboard_) {
            dashboard_->AddEvent("Oversized feedback message discarded", DashboardEventLevel::Warning);
        }
        receiveBuffer_.clear();
    }

    return true;
}

void TCPServer::processMessage(const std::string& message) {
    try {
        const nlohmann::json received = nlohmann::json::parse(message);
        const auto positionsIterator = received.find("currentPositions");
        if (positionsIterator == received.end() || !positionsIterator->is_array()
            || positionsIterator->size() != ActuatorCount) {
            std::cerr << "Feedback must contain exactly six 'currentPositions' values." << std::endl;
            if (dashboard_) {
                dashboard_->AddEvent("Feedback rejected: expected six positions", DashboardEventLevel::Warning);
            }
            return;
        }

        std::array<float, ActuatorCount> updatedPositions{};
        for (std::size_t index = 0; index < ActuatorCount; ++index) {
            if (!(*positionsIterator)[index].is_number()) {
                std::cerr << "Every 'currentPositions' value must be numeric." << std::endl;
                if (dashboard_) {
                    dashboard_->AddEvent("Feedback rejected: position is not numeric", DashboardEventLevel::Warning);
                }
                return;
            }
            updatedPositions[index] = (*positionsIterator)[index].get<float>();
        }

        {
            std::lock_guard<std::mutex> lock(positionsMutex_);
            currentPositions_ = updatedPositions;
        }
        if (dashboard_) {
            dashboard_->UpdateFeedback(updatedPositions);
        }
        const bool firstFeedback = !hasPositionFeedback_.exchange(true);
        if (firstFeedback) {
            std::cout << "[TCP] Position feedback received; actuator output enabled." << std::endl;
            if (dashboard_) {
                dashboard_->AddEvent("Position feedback valid; output enabled");
            }
        }
        LogProtocolMessage("RECV", message);
    }
    catch (const nlohmann::json::exception& exception) {
        std::cerr << "Failed to parse feedback JSON: " << exception.what() << std::endl;
        if (dashboard_) {
            dashboard_->AddEvent("Feedback JSON could not be parsed", DashboardEventLevel::Warning);
        }
    }
}

bool TCPServer::isConnected() const noexcept {
    return connected_;
}

bool TCPServer::hasPositionFeedback() const noexcept {
    return hasPositionFeedback_;
}

std::array<float, TCPServer::ActuatorCount> TCPServer::getCurrentPositions() const {
    std::lock_guard<std::mutex> lock(positionsMutex_);
    return currentPositions_;
}

void TCPServer::closeClientConnection() {
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

    if (dashboard_) {
        dashboard_->SetClientConnected(false);
        if (wasConnected) {
            dashboard_->AddEvent("Controller disconnected", DashboardEventLevel::Warning);
        }
    }

    if (socket != INVALID_SOCKET) {
        // Interrupt a blocking send/recv before waiting for the send lock.
        shutdown(socket, SD_BOTH);
        std::lock_guard<std::mutex> sendLock(sendMutex_);
        closesocket(socket);
    }
}

void TCPServer::closeConnection() {
    if (shuttingDown_.exchange(true)) {
        return;
    }

    closeClientConnection();

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
