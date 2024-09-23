#include "../include/controller.h"
#include <chrono>
#include <iostream>

void Controller::start() {
    if (isRunning) {
        // already running
        return;
    }
    irm.lock();
    isRunning = true;
    irm.unlock();

    // connect
    auto handleError = [this](bool ok) {
        this->dispose();
    };
    inputStrategy->connect(handleError);
    outputStrategy->connect(handleError);

    std::cout << "[DEBUG]   connected strategies" << "\n";

    // start output thread
    std::thread output([this] {
        auto handleFlightData = [this](FlightData* fd){
            try
            {
                std::cout << "[DEBUG]   handleFlightData()" << "\n";

                // get current positions
                float positions[6];
                this->outputStrategy->getCurrentPositions(positions);
                
                // calculate speed and movements
                fdm.lock();
                Calculator::CalculationData data = calculator->calculatePistons(fd, positions);
                fdm.unlock();

                // send update
                outputStrategy->updatePositions(data);
            }
            catch(const std::exception& e)
            {
                std::cerr << "[ERROR] "<< e.what() << '\n';
            }
        };

        while (isRunning)
        {
            handleFlightData(mostRecentFlightData);

            // [THOUGHT] wait the movemont time?
        }
        
        // set to default position
        FlightData dflt = FlightData();
        handleFlightData(&dflt);
    });

    std::thread input([this] {
        while (isRunning)
        {
            FlightData d = inputStrategy->getInput();
            fdm.lock();
            mostRecentFlightData = &d;
            fdm.unlock();
        }
    });

    output.join();
    input.join();
}

void Controller::dispose() {
    // quit threads
    if (isRunning) {
        isRunning = false;
    }
}