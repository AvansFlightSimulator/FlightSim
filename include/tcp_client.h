#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__

#include <string>
#ifdef _WIN32
#include <WS2tcpip.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#endif

class NetworkClient 
{
public:
    virtual bool initialize(char* addr, int port) = 0;
    virtual int getData(char* data) = 0;
    virtual bool sendData(std::string data) = 0;
    virtual void dispose() = 0;
};

class TCPClient : public NetworkClient
{
private:
    int clientSd;
    #ifdef _WIN32
    SOCKET socket;
    #else
    sockaddr_in sendSockAddr;
    #endif
public:
    TCPClient() {

    };

    bool initialize(char* addr, int port);
    int getData(char* data);
    bool sendData(std::string data);
    void dispose();
};

#endif