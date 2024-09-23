
#ifndef __TCP_OUTPUT_STRATEGY_H__
#define __TCP_OUTPUT_STRATEGY_H__

#include "output_strategy.h"
#include "tcp_client.h"

class TCPOutputStrategy : public OutputStrategy
{
private:
    NetworkClient* client;
    std::function<void(bool)> onError;
public:
    TCPOutputStrategy(NetworkClient* client) {
        this->client = client;
    };

    void getCurrentPositions(float[6]);
    bool connect(std::function<void(bool)>);
    bool updatePositions(Calculator::CalculationData data);
    bool isOK();
};

#endif
