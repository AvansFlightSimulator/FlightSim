#include <math.h>  
#include "flight_data.h"
#include "max_piston_strategy.h"

#ifndef __CALCULATIONS_FUNC_H__
#define __CALCULATIONS_FUNC_H__

#include <iostream>

class Calculator {
    public:
        struct Positions {
            float x[6];
            float y[6];

            Positions() {}

            Positions(float xv[6], float yv[6]) {
                for (int i = 0; i < 6; ++i) {
                    x[i] = xv[i];
                    y[i] = yv[i];
                }
            }
        };

        struct CalculationData {
            float lengths[6], speeds[6];
        };

        virtual Calculator::CalculationData calculatePistons(FlightData* fd, float* oldPistonLengths) = 0;
};


class CalculatorImpl : public Calculator {
    private:
        const int numOfPistons = 6;
        float translationZ;
        float legLength;
        float maxVelocity;
        Calculator::Positions defaultPositions;
        Calculator::Positions defaultBasePositions;
        MaxPistonStrategy* pistonStrategy;

    public:
        CalculatorImpl(float legLength, float translationZ, float maxVelocity, Calculator::Positions defaultPositions, Calculator::Positions defaultBasePositions, MaxPistonStrategy* maxPistonStrategy) {
            this->legLength = legLength;
            this->translationZ = translationZ;
            this->maxVelocity = maxVelocity;
            this->defaultPositions = defaultPositions;
            this->defaultBasePositions = defaultBasePositions;
            this->pistonStrategy = maxPistonStrategy;
        }
  
        Calculator::CalculationData calculatePistons(FlightData* fd, float* oldPistonLengths);
};

#endif