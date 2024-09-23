#ifndef __OUTPUT_STRATEGY_H__
#define __OUTPUT_STRATEGY_H__

#include <functional>
#include "calculations.h"

class OutputStrategy {
    public:
        virtual void getCurrentPositions(float[6]) = 0;
        virtual bool connect(std::function<void(bool)>) = 0;
        virtual bool updatePositions(Calculator::CalculationData data) = 0;
        virtual bool isOK() = 0;
};

#endif