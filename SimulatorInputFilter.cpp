#include "SimulatorInputFilter.h"

#include <cmath>

// Store the filter time constant in seconds.
SimulatorInputFilter::SimulatorInputFilter(std::chrono::duration<double> timeConstant)
    : timeConstantSeconds_(timeConstant.count()) {}

// Low-pass filter a new sample; writes the result to output. False if input is invalid.
bool SimulatorInputFilter::Update(
    const SimulatorInput& input, Clock::time_point timestamp, SimulatorInput& output) {
    // Reject NaN/infinite values.
    if (!std::isfinite(input.pitchDegrees) || !std::isfinite(input.rollDegrees)
        || !std::isfinite(input.rudderDegrees)) {
        return false;
    }

    if (!initialized_) {
        // Snap to the first real sample instead of easing from an artificial zero.
        filtered_ = input;
        lastUpdate_ = timestamp;
        initialized_ = true;
        output = filtered_;
        return true;
    }

    // No time has passed (or clock went back): keep the current value.
    const double elapsedSeconds = std::chrono::duration<double>(timestamp - lastUpdate_).count();
    if (elapsedSeconds <= 0.0) {
        output = filtered_;
        return true;
    }

    lastUpdate_ = timestamp;
    // Time-based smoothing factor; a time constant of 0 or less disables filtering.
    const double alpha = timeConstantSeconds_ <= 0.0
        ? 1.0
        : 1.0 - std::exp(-elapsedSeconds / timeConstantSeconds_);
    filtered_.pitchDegrees = FilterValue(input.pitchDegrees, filtered_.pitchDegrees, alpha);
    filtered_.rollDegrees = FilterValue(input.rollDegrees, filtered_.rollDegrees, alpha);
    filtered_.rudderDegrees = FilterValue(input.rudderDegrees, filtered_.rudderDegrees, alpha);
    output = filtered_;
    return true;
}

// Clear filter state so the next sample is used as-is.
void SimulatorInputFilter::Reset() noexcept {
    initialized_ = false;
    filtered_ = {};
    lastUpdate_ = {};
}

// Move the filtered value toward the input by fraction alpha.
double SimulatorInputFilter::FilterValue(double input, double filtered, double alpha) const noexcept {
    return filtered + alpha * (input - filtered);
}