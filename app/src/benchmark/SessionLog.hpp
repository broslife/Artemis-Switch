#pragma once

#include <atomic>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace artemis::benchmark {

// Configuration captured when the stream session starts.
struct SessionLogConfig {
    std::string host;
    int width = 1280;
    int height = 720;
    int fps = 60;
    int bitrateKbps = 15000;
    bool lowLatencyPacing = false;
    int queueSize = 3;
};

// Always-on session log: one text file per streaming session on the SD card
// (sdmc:/switch/Artemis-Switch/logs/), one line per second.
// Accumulates rows in memory, flushes in batches; SD errors never crash stream.
class SessionLog {
public:
    static SessionLog& instance();
    ~SessionLog();

    // Start logging for a new session. No-op if already active.
    void start(const SessionLogConfig& config);

    // Stop the worker, write the summary, and close the file.
    void stop();

    [[nodiscard]] bool active() const { return m_running.load(); }

private:
    SessionLog() = default;
    SessionLog(const SessionLog&) = delete;
    SessionLog& operator=(const SessionLog&) = delete;

    void workerLoop();
    void captureLine();
    void flushPending();

    mutable std::mutex m_mutex;
    std::atomic<bool> m_running{false};
    std::thread m_worker;

    std::ofstream m_file;
    // Lines buffered in memory between flushes.
    std::vector<std::string> m_pending;

    // Previous cumulative counter values for per-second delta computation.
    uint64_t m_prev_uflows = 0;
    uint64_t m_prev_skips = 0;
    uint64_t m_prev_oflow_drops = 0;
    uint64_t m_prev_pacing_skips = 0;
    uint32_t m_elapsed_sec = 0;

    // Session-level accumulators for summary.
    double m_rfps_sum = 0.0;
    float m_rfps_min = 999.0f;
    float m_rfps_max = 0.0f;
    uint32_t m_rfps_count = 0;
};

} // namespace artemis::benchmark
