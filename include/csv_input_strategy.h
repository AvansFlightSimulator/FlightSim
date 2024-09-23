#ifndef __MFS_INPUT_STRATEGY_H__
#define __MFS_INPUT_STRATEGY_H__

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>
#include <thread>

#include "input_strategy.h"

class CSVInputStrategy : public InputStrategy
{
private:
    int msPerRow; 
    std::ifstream& s;

    std::string line;
    std::string word;

public:
    CSVInputStrategy(int msPerRow, std::ifstream& s) : s(s) {
        this->msPerRow = msPerRow;
    }

    ~CSVInputStrategy();

    bool isOK() override;
    bool connect(std::function<void(bool)>) override;

    FlightData getInput() override;
};

#endif
