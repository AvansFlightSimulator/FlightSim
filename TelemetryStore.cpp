#include "TelemetryStore.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <limits>
#include <algorithm>

namespace {
constexpr std::size_t MaximumQueue = 4096;
constexpr std::size_t MaximumHistory = 24000;
constexpr std::uint64_t MaximumFileBytes = 64 * 1024 * 1024;
constexpr std::size_t MaximumReplayFrames = 200000;
constexpr std::size_t MaximumQueuedBytes = 8 * 1024 * 1024;
constexpr std::size_t MaximumHistoryBytes = 16 * 1024 * 1024;

bool ReadValues(const nlohmann::json& object, const char* key,
    std::array<double, ActuatorCount>& values, bool strings) {
    const auto found = object.find(key);
    if (found == object.end()) return true;
    if (!found->is_array() || found->size() != ActuatorCount) return false;
    for (std::size_t i = 0; i < ActuatorCount; ++i) {
        const auto& value = (*found)[i];
        if (value.is_null()) continue;
        double number;
        if (value.is_number()) number = value.get<double>();
        else if (strings && value.is_string()) {
            const auto text = value.get<std::string>();
            std::size_t end = 0;
            number = std::stod(text, &end);
            if (end != text.size()) return false;
        }
        else return false;
        if (!std::isfinite(number)) return false;
        values[i] = number;
    }
    return true;
}
}

TelemetryFrame::TelemetryFrame() {
    for (auto& signal : values) signal.fill((std::numeric_limits<double>::quiet_NaN)());
}

bool TelemetryStore::Decode(const std::string& kind, const std::string& payload,
    TelemetryFrame& frame, std::string& error) {
    TelemetryFrame decoded;
    decoded.kind = kind;
    decoded.payload = payload;
    error.clear();
    try {
        if (kind == "send" || kind == "feedback") {
            const auto object = nlohmann::json::parse(payload);
            if (!object.is_object()) throw std::runtime_error("Expected a JSON object");
            const auto read = [&](const nlohmann::json& source, const char* key, TelemetrySignal signal, bool strings = false) {
                if (!ReadValues(source, key, decoded.values[static_cast<std::size_t>(signal)], strings))
                    throw std::runtime_error(std::string("Invalid diagnostic array: ") + key);
            };
            if (kind == "send") {
                read(object, "positions", TelemetrySignal::PcPosition, true);
                read(object, "speeds", TelemetrySignal::PcSpeed, true);
            }
            else {
                const auto diagnostics = object.find("diagnostics");
                if (diagnostics != object.end()) {
                    if (!diagnostics->is_object()
                        || (diagnostics->value("version", 0) != 1 && diagnostics->value("version", 0) != 2))
                        throw std::runtime_error("Unsupported diagnostics version");
                    read(*diagnostics, "driveTargetPositions", TelemetrySignal::DriveTarget);
                    read(*diagnostics, "actualPositions", TelemetrySignal::DrivePosition);
                    read(*diagnostics, "actualVelocities", TelemetrySignal::DriveVelocity);
                    read(*diagnostics, "followingErrors", TelemetrySignal::FollowingError);
                    read(*diagnostics, "receivedPositions", TelemetrySignal::PlcPosition);
                    read(*diagnostics, "receivedSpeeds", TelemetrySignal::PlcSpeed);
                    read(*diagnostics, "commandPositions", TelemetrySignal::CommandPosition);
                    read(*diagnostics, "commandVelocities", TelemetrySignal::CommandVelocity);
                    read(*diagnostics, "commandAccelerations", TelemetrySignal::CommandAcceleration);
                }
                // Legacy feedback retains its original identity in the raw packet;
                // it is not relabelled as validated CMMT-AS telemetry.
            }
        }
        frame = std::move(decoded);
        return true;
    }
    catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

TelemetryStore::TelemetryStore(const std::string& sessionFile) {
    status_.file = sessionFile;
    writer_ = std::thread(&TelemetryStore::WriteLoop, this);
}

TelemetryStore::~TelemetryStore() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    ready_.notify_one();
    writer_.join();
}

void TelemetryStore::Record(const std::string& kind, const std::string& payload, std::uint64_t connection) {
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
    TelemetryFrame frame;
    std::string error;
    if (!Decode(kind, payload, frame, error)) {
        frame.kind = "invalid-diagnostics";
        frame.payload = payload;
    }
    frame.seconds = seconds;
    frame.connection = connection;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!error.empty()) status_.error = error;
        history_.push_back(frame);
        historyBytes_ += sizeof(TelemetryFrame) + frame.payload.size();
        while (history_.size() > MaximumHistory || historyBytes_ > MaximumHistoryBytes) {
            historyBytes_ -= sizeof(TelemetryFrame) + history_.front().payload.size();
            history_.pop_front();
        }
        if (pending_.size() >= MaximumQueue || pendingBytes_ + frame.payload.size() + sizeof(frame) > MaximumQueuedBytes) {
            ++status_.dropped;
        }
        else {
            pendingBytes_ += frame.payload.size() + sizeof(frame);
            pending_.push_back(std::move(frame));
        }
    }
    ready_.notify_one();
}

TelemetryStatus TelemetryStore::Status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto status = status_;
    status.elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
    return status;
}

std::vector<TelemetryFrame> TelemetryStore::Recent() const {
    std::vector<TelemetryFrame> frames;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        frames.assign(history_.begin(), history_.end());
    }
    std::stable_sort(frames.begin(), frames.end(), [](const TelemetryFrame& a, const TelemetryFrame& b) { return a.seconds < b.seconds; });
    return frames;
}

void TelemetryStore::WriteLoop() {
    std::ofstream file(status_.file, std::ios::binary | std::ios::out);
    std::uint64_t bytes = 0;
    std::size_t events = 0;
    if (file) {
        const auto epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        file << nlohmann::json({ {"schema", 1}, {"startedUtcMs", epoch},
            {"timebase", "PC steady-clock seconds; PLC timestamps are separate"} }).dump() << '\n';
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        status_.recording = static_cast<bool>(file);
        if (!file) status_.error = "Cannot create recording";
    }
    for (;;) {
        std::deque<TelemetryFrame> batch;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            ready_.wait_for(lock, std::chrono::milliseconds(250), [&] { return stopping_ || !pending_.empty(); });
            batch.swap(pending_);
            pendingBytes_ = 0;
            if (batch.empty() && stopping_) break;
        }
        for (const auto& frame : batch) {
            const auto line = nlohmann::json({ {"t", frame.seconds}, {"connection", frame.connection},
                {"kind", frame.kind}, {"payload", frame.payload} }).dump(-1, ' ', false,
                    nlohmann::json::error_handler_t::replace) + '\n';
            if (file && bytes + line.size() <= MaximumFileBytes && events < MaximumReplayFrames) {
                file << line;
                bytes += line.size();
                ++events;
            }
            else {
                std::lock_guard<std::mutex> lock(mutex_);
                status_.recording = false;
                status_.error = bytes + line.size() > MaximumFileBytes || events >= MaximumReplayFrames
                    ? "Recording reached size/event limit; live view continues" : "Recording write failed";
                ++status_.dropped;
            }
        }
        file.flush();
        if (!file) {
            std::lock_guard<std::mutex> lock(mutex_);
            status_.recording = false;
            status_.error = "Recording write failed";
        }
    }
}

bool TelemetryStore::Load(const std::string& path, std::vector<TelemetryFrame>& frames, std::string& error) {
    error.clear();
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file || file.tellg() > static_cast<std::streamoff>(MaximumFileBytes + 4096)) {
        error = "Cannot open recording, or recording exceeds 64 MiB";
        return false;
    }
    file.seekg(0);
    std::vector<TelemetryFrame> loaded;
    std::string line;
    try {
        if (!std::getline(file, line) || nlohmann::json::parse(line).value("schema", 0) != 1)
            throw std::runtime_error("Unsupported recording schema");
        while (std::getline(file, line)) {
            // A crash can leave one incomplete final line; retain the complete prefix.
            if (file.eof()) break;
            if (loaded.size() >= MaximumReplayFrames) throw std::runtime_error("Recording has too many events");
            const auto object = nlohmann::json::parse(line);
            TelemetryFrame frame;
            std::string diagnosticError;
            const auto kind = object.at("kind").get<std::string>();
            const auto payload = object.at("payload").get<std::string>();
            if (!Decode(kind, payload, frame, diagnosticError)) {
                frame.kind = "invalid-diagnostics";
                frame.payload = payload;
            }
            frame.seconds = object.at("t").get<double>();
            frame.connection = object.at("connection").get<std::uint64_t>();
            if (!std::isfinite(frame.seconds) || frame.seconds < 0) throw std::runtime_error("Invalid recording timestamp");
            loaded.push_back(std::move(frame));
        }
        std::stable_sort(loaded.begin(), loaded.end(), [](const TelemetryFrame& a, const TelemetryFrame& b) { return a.seconds < b.seconds; });
        frames.swap(loaded);
        return true;
    }
    catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
