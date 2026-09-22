#include "SimulatorInputFilter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool Near(double actual, double expected, double tolerance = 1e-9) {
    return std::fabs(actual - expected) <= tolerance;
}

void CheckInitializationAndConstantInput() {
    SimulatorInputFilter filter;
    const auto start = SimulatorInputFilter::Clock::time_point{};
    SimulatorInput output;
    const SimulatorInput first{12.0, -8.0, 3.0};
    Require(filter.Update(first, start, output), "First finite sample must be accepted");
    Require(Near(output.pitchDegrees, 12.0) && Near(output.rollDegrees, -8.0)
        && Near(output.rudderDegrees, 3.0), "Filter must initialize directly from the first sample");

    for (int sample = 1; sample <= 20; ++sample) {
        Require(filter.Update(first, start + std::chrono::milliseconds(50 * sample), output),
            "Constant sample was rejected");
        Require(Near(output.pitchDegrees, 12.0) && Near(output.rollDegrees, -8.0)
            && Near(output.rudderDegrees, 3.0), "Constant input must remain constant");
    }
}

void CheckSmallNoiseIsSmoothed() {
    SimulatorInputFilter filter;
    const auto start = SimulatorInputFilter::Clock::time_point{};
    SimulatorInput output;
    Require(filter.Update({2.0, 2.0, 2.0}, start, output), "Initial noise sample was rejected");

    const double noisyValues[] = {2.04, 1.97, 2.06, 2.02, 2.10, 1.95, 2.03};
    double largestFilteredDeviation = 0.0;
    double largestRawDeviation = 0.0;
    for (int index = 0; index < 7; ++index) {
        const double raw = noisyValues[index];
        Require(filter.Update({raw, raw, raw},
            start + std::chrono::milliseconds(50 * (index + 1)), output), "Noisy sample was rejected");
        largestRawDeviation = (std::max)(largestRawDeviation, std::fabs(raw - 2.0));
        largestFilteredDeviation = (std::max)(largestFilteredDeviation,
            std::fabs(output.pitchDegrees - 2.0));
    }
    Require(largestFilteredDeviation < largestRawDeviation * 0.5,
        "Small high-frequency fluctuations were not sufficiently reduced");
}

void CheckSlowIntentionalMovement() {
    SimulatorInputFilter filter;
    const auto start = SimulatorInputFilter::Clock::time_point{};
    SimulatorInput output;
    Require(filter.Update({}, start, output), "Initial ramp sample was rejected");
    double previous = output.pitchDegrees;
    for (int sample = 1; sample <= 100; ++sample) {
        const double raw = sample * 0.05;
        Require(filter.Update({raw, 0.0, 0.0},
            start + std::chrono::milliseconds(50 * sample), output), "Ramp sample was rejected");
        Require(output.pitchDegrees >= previous && output.pitchDegrees <= raw,
            "Filtered slow movement must remain smooth and monotonic");
        previous = output.pitchDegrees;
    }
    Require(output.pitchDegrees > 4.8,
        "Filter lagged too far behind a slow intentional movement");
}

void CheckSuddenMeaningfulMovement() {
    SimulatorInputFilter filter;
    const auto start = SimulatorInputFilter::Clock::time_point{};
    SimulatorInput output;
    Require(filter.Update({}, start, output), "Initial step sample was rejected");
    Require(filter.Update({10.0, 10.0, 10.0}, start + std::chrono::milliseconds(50), output),
        "Step sample was rejected");
    Require(output.pitchDegrees > 3.0 && output.pitchDegrees < 4.0,
        "Meaningful movement must propagate during the first control cycle");

    for (int sample = 2; sample <= 7; ++sample) {
        Require(filter.Update({10.0, 10.0, 10.0},
            start + std::chrono::milliseconds(50 * sample), output), "Step response sample was rejected");
    }
    Require(output.pitchDegrees > 9.4,
        "Meaningful movement must be mostly applied within 350 ms");
}

void CheckTimeBasedBehavior() {
    const auto start = SimulatorInputFilter::Clock::time_point{};
    SimulatorInputFilter oneLongStep;
    SimulatorInputFilter twoShortSteps;
    SimulatorInput longOutput;
    SimulatorInput shortOutput;
    Require(oneLongStep.Update({}, start, longOutput), "Long-step filter failed to initialize");
    Require(twoShortSteps.Update({}, start, shortOutput), "Short-step filter failed to initialize");
    Require(oneLongStep.Update({10.0, 0.0, 0.0}, start + std::chrono::milliseconds(100), longOutput),
        "100 ms sample was rejected");
    Require(twoShortSteps.Update({10.0, 0.0, 0.0}, start + std::chrono::milliseconds(50), shortOutput),
        "First 50 ms sample was rejected");
    Require(twoShortSteps.Update({10.0, 0.0, 0.0}, start + std::chrono::milliseconds(100), shortOutput),
        "Second 50 ms sample was rejected");
    Require(Near(longOutput.pitchDegrees, shortOutput.pitchDegrees),
        "Equal elapsed time must produce the same response at different update intervals");
}

void CheckResetAndInvalidInput() {
    SimulatorInputFilter filter;
    const auto start = SimulatorInputFilter::Clock::time_point{};
    SimulatorInput output;
    Require(filter.Update({5.0, 5.0, 5.0}, start, output), "Filter failed to initialize");
    const SimulatorInput sentinel{91.0, 92.0, 93.0};
    output = sentinel;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    Require(!filter.Update({nan, 0.0, 0.0}, start + std::chrono::milliseconds(50), output),
        "Nonfinite input must be rejected");
    Require(Near(output.pitchDegrees, sentinel.pitchDegrees),
        "Rejected input must not overwrite the caller's output");

    filter.Reset();
    Require(filter.Update({-20.0, 15.0, -4.0}, start + std::chrono::seconds(1), output),
        "First sample after reset was rejected");
    Require(Near(output.pitchDegrees, -20.0) && Near(output.rollDegrees, 15.0)
        && Near(output.rudderDegrees, -4.0), "Reset must make the next sample initialize immediately");
}
}

int main() {
    try {
        CheckInitializationAndConstantInput();
        CheckSmallNoiseIsSmoothed();
        CheckSlowIntentionalMovement();
        CheckSuddenMeaningfulMovement();
        CheckTimeBasedBehavior();
        CheckResetAndInvalidInput();
        std::cout << "Simulator input filter checks passed.\n";
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
