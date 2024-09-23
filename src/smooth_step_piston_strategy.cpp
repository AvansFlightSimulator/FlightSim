#include "../include/smooth_step_piston_strategy.h"
#include "../include/math_func.h"

#include <iostream>

float SmoothStepPistonStrategy::getValue(float wantedValue) {
    float step = smootherstep(minimum, maximum, wantedValue);
    // std::cout << "[DEBUG]    minimum: " << minimum << " maximum: " << maximum << " wantedValue: " << wantedValue << " step: " << step << "\n";
    return minimum + step * (maximum - minimum);
}