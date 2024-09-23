#include <iostream>
#include <mutex>
#include <fstream>
#include "../include/calculations.h"
#include "../include/controller.h"
#include "../include/cout_output_strategy.h"
#include "../include/csv_input_strategy.h"
#include "../include/mfs_input_strategy.h"
#include "../include/smooth_step_piston_strategy.h"

int main(int, char**){
    std::cout << "[INFO]    main()" << "\n";

    // setup calculator
    SmoothStepPistonStrategy pistonStrategy = SmoothStepPistonStrategy(5.0f, 400.0f);
    
    float x[6] = {537.69f, 715.23f, 177.53f, -177.53f, -715.25f, -537.69f};
    float y[6] = {515.43f, 207.94f, -723.37f, -723.37f, 207.94f, 515.43f};
    Calculator::Positions defaultPositions(x, y);

    float bx[6] = {177.53f, 715.23f, 537.69f, -537.69f, -715.23f, -177.53f};
    float by[6] = {723.37f, -207.94f, -515.43f, -515.43f, -207.94f, 723.37f};
    Calculator::Positions defaultBasePositions(bx, by);

    CalculatorImpl c = CalculatorImpl(824.0f, 935.7456f, 200.0f, defaultPositions, defaultBasePositions, &pistonStrategy);
    std::cout << "[DEBGUG]  initialized calculator" << "\n";

    // setup controller
    InputStrategy* inputStrategy;
    OutputStrategy* outputStrategy;

    #ifdef _WIN32
    MFSInputStrategy mfs = MFSInputStrategy(1, 30);
    inputStrategy = &mfs;
    #else
    std::cout << "[WARN]    not running on windows: choosing simulated strategy";
    std::ifstream rfile ("output.csv");
    CSVInputStrategy csv = CSVInputStrategy(100, rfile);
    inputStrategy = &csv;
    #endif

    CoutOutputStrategy coutOutputStrategy = CoutOutputStrategy();
    outputStrategy = &coutOutputStrategy;
    FlightData initialPosition = FlightData();
    Controller controller;

    std::cout << "[DEBUG]   initializing controller..." << "\n";
    controller.initialize(&c, &initialPosition, inputStrategy, outputStrategy);

    std::cout << "[DEBUG]   initialized controller, starting..." << "\n";
    controller.start();

    std::cout << "[INFO]   stopping..." << "\n";
    controller.dispose();

    std::cout << "[INFO]   stopped." << "\n";
}
