
#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__

#include "calculations.h"
#include "output_strategy.h"
#include "input_strategy.h"

#include <thread>
#include <mutex>

class Controller {
    private:
        Calculator* calculator;
        InputStrategy* inputStrategy;
        OutputStrategy* outputStrategy;
        
        bool isRunning;
        std::mutex irm;

        FlightData* mostRecentFlightData;
        std::mutex fdm;

    public: 
        void initialize(Calculator* calculator, FlightData* d, InputStrategy* inputStrategy, OutputStrategy* outputStrategy) {
            this->calculator = calculator;
            this->mostRecentFlightData = d;
            this->inputStrategy = inputStrategy;
            this->outputStrategy = outputStrategy;
        }

        void start();
        void dispose();
};

#endif