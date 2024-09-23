#ifndef __SMOOTH_STEP_PISTON_STRATEGY_H__
#define __SMOOTH_STEP_PISTON_STRATEGY_H__

#include "max_piston_strategy.h"

class SmoothStepPistonStrategy : public MaxPistonStrategy
{
private:
    float minimum;
    float maximum;

public:
    SmoothStepPistonStrategy(float minimum, float maximum) {
        this->minimum = minimum;
        this->maximum = maximum;
    }

    float getValue(float wantedValue);  
};

#endif