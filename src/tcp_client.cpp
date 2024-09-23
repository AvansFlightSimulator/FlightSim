#include "../include/tcp_client.h"

bool TCPClient::initialize(char* addr, int port) {
    #ifdef _WIN32
    WSADATA wsData;
    WORD ver = MAKEWORD(0, 0);

    if (_WINSOCK2API_::WSAStartup(ver, &wsData) != 0) {
        return false;
    }

    SOCKET ls = _WINSOCK2API_::socket(AF_INET, SOCK_STREAM, 0);
    if (ls == INVALID_SOCKET) {
        return false;
    }

    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(port);
    hint.sin_addr.S_un.S_addr = INADDR_ANY;

    _WINSOCK2API_::bind(ls, (sockaddr*)&hint, sizeof(hint));
    _WINSOCK2API_::listen(ls, SOMAXCONN);

    sockaddr_in c;
    int cSize = sizeof(c);
    socket = _WINSOCK2API_::accept(ls, (sockaddr*)&c, &cSize);
    retrun true;
    #else
    struct hostent* host = gethostbyname(addr);
    bzero((char*)&sendSockAddr, sizeof(sendSockAddr)); 
    sendSockAddr.sin_family = AF_INET; 
    sendSockAddr.sin_addr.s_addr = 
        inet_addr(inet_ntoa(*(struct in_addr*)*host->h_addr_list));
    sendSockAddr.sin_port = htons(port);
    clientSd = socket(AF_INET, SOCK_STREAM, 0);

    return connect(port, (sockaddr*) &sendSockAddr, sizeof(sendSockAddr)) >= 0;
    #endif
}

int TCPClient::getData(char* data) {
    #ifdef _WIN32
    return _WINSOCK2API_::recv(socket, data, sizeof(data), 0);
    #else
    return recv(clientSd, &data, sizeof(data), 0);
    #endif
}

bool TCPClient::sendData(std::string data) {
    #ifdef _WIN32
    auto status = _WINSOCK2API_::send(socket, data.c_str(), data.size() + 1, 0);
    return status != -1;
    #else
    return send(clientSd, data.c_str(), sizeof(data), 0) != 0;
    #endif
}

void TCPClient::dispose() {
    #ifdef _WIN32
    _WINSOCK2API_::closesocket(socket);
    _WINSOCK2API_::WSACleanup();
    #else
    close(clientSd);
    #endif
}

