#pragma once

#include "BridgeTypes.h"

#include <chrono>

namespace SimulatorFilterSettings {
// The output reaches about 63% of a step after one time constant. At the
// nominal 20 Hz update rate, 120 ms gives useful noise rejection while still
// responding to deliberate motion within a few control cycles.
constexpr auto TimeConstant = std::chrono::milliseconds(120);
}

// Smooths raw live-MSFS angles before the existing motion mapping and geometry.
// Manual input is intentionally excluded so Execute retains its exact behavior.
// There is no hard deadband: exponential smoothing attenuates small changes
// continuously instead of storing them up for a later stair-step movement.
class SimulatorInputFilter {
public:
    using Clock = std::chrono::steady_clock;

    explicit SimulatorInputFilter(
        std::chrono::duration<double> timeConstant = SimulatorFilterSettings::TimeConstant);

    bool Update(const SimulatorInput& input, Clock::time_point timestamp, SimulatorInput& output);
    void Reset() noexcept;

private:
    double FilterValue(double input, double filtered, double alpha) const noexcept;

    double timeConstantSeconds_;
    bool initialized_ = false;
    SimulatorInput filtered_;
    Clock::time_point lastUpdate_{};
};
