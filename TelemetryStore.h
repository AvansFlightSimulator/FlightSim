#pragma once

#include "BridgeTypes.h"
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Missing values are NaN, never zero. Each event keeps its own PC timestamp;
// incoming and outgoing messages are not presented as simultaneous samples.
enum class TelemetrySignal : std::size_t {
    PcPosition, PcSpeed, DriveTarget, DrivePosition, DriveVelocity, FollowingError,
    PlcPosition, PlcSpeed, CommandPosition, CommandVelocity, CommandAcceleration, Count
};
constexpr std::size_t TelemetrySignalCount = static_cast<std::size_t>(TelemetrySignal::Count);
struct TelemetryFrame {
    double seconds = 0;
    std::uint64_t connection = 0;
    std::array<std::array<double, ActuatorCount>, TelemetrySignalCount> values;
    std::string kind;
    std::string payload;
    TelemetryFrame();
};
struct TelemetryStatus {
    std::string file;
    std::string error;
    std::uint64_t dropped = 0;
    bool recording = false;
    double elapsedSeconds = 0;
};

// Recording is passive. Socket workers enqueue events; a separate worker writes
// the session. Queue and live history have fixed bounds, and failures are visible.
class TelemetryStore {
public:
    explicit TelemetryStore(const std::string& sessionFile);
    ~TelemetryStore();
    TelemetryStore(const TelemetryStore&) = delete;
    TelemetryStore& operator=(const TelemetryStore&) = delete;
    void Record(const std::string& kind, const std::string& payload, std::uint64_t connection);
    TelemetryStatus Status() const;
    std::vector<TelemetryFrame> Recent() const;
    static bool Decode(const std::string& kind, const std::string& payload,
        TelemetryFrame& frame, std::string& error);
    static bool Load(const std::string& file, std::vector<TelemetryFrame>& frames, std::string& error);
private:
    void WriteLoop();
    const std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<TelemetryFrame> history_;
    std::deque<TelemetryFrame> pending_;
    TelemetryStatus status_;
    bool stopping_ = false;
    std::size_t pendingBytes_ = 0;
    std::size_t historyBytes_ = 0;
    std::thread writer_;
};
