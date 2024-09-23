#ifndef __INPUT_STRATEGY_H__
#define __INPUT_STRATEGY_H__

#include <functional>
#include "flight_data.h"

class InputStrategy {
    public:
        virtual bool isOK() = 0;
        virtual bool connect(std::function<void(bool)>) = 0;
        virtual FlightData getInput() = 0;
};

#endif