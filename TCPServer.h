#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <iostream>
#include <string>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib") // Link with ws2_32.lib

class TCPServer {
public:
    //TCPServer(const std::string& server_ip = "127.0.0.1", int server_port = 32760);
    TCPServer(const std::string& server_ip = "192.168.137.123", int server_port = 32760);
    ~TCPServer();
    void startListening();
    void sendData(const std::string& data);
    void closeConnection();

private:
    SOCKET server_sock;
    SOCKET client_sock;
    sockaddr_in server_address;
    sockaddr_in client_address;
    bool isConnected;
    void acceptClient();
};

#endif // TCP_SERVER_H