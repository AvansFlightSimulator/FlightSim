#include "../include/tcp_output_strategy.h"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

bool TCPOutputStrategy::isOK() {
    return true;
}

void TCPOutputStrategy::getCurrentPositions(float positions[6]) {
    // get data over TCP
    char buf[4096];
    int nBytes = client->getData(buf);
    if (nBytes == 0) {
        onError(false);
    }

    try
    {
        // map buffer to json 
        json d = json::parse(buf, 0, nBytes);

        auto l = d["currentPositions"];

        // set positions
        int pos = 0;
        for (auto i = l.begin(); i != l.end(); ++i)
        {
            positions[pos] = i.value();
            pos++;
        }
    }
    catch(const std::exception& e)
    {
        onError(false);
    }
}

bool TCPOutputStrategy::connect(std::function<void(bool)> onError) {
    this->onError = onError;
    return true;
}

bool TCPOutputStrategy::updatePositions(Calculator::CalculationData data) {
    json j;
    j["positions"] = data.lengths;
    j["speeds"] = data.speeds;
    client->sendData(j.dump());
    return true;
}
