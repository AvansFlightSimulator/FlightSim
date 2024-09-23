#ifndef __COUT_OUTPUT_STRATEGY_H__
#define __COUT_OUTPUT_STRATEGY_H__

#include "output_strategy.h"

class CoutOutputStrategy : public OutputStrategy
{
private:
    float positions[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
public:
    CoutOutputStrategy() {
    };
    
    void getCurrentPositions(float[6]);
    bool connect(std::function<void(bool)>);
    bool updatePositions(Calculator::CalculationData data);
    bool isOK();
};

#endif