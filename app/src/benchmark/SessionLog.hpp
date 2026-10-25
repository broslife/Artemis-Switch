#pragma once

#include <atomic>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "SlotIntervals.hpp"

namespace artemis::benchmark {

// Configuration captured when the stream session starts.
struct SessionLogConfig {
    std::string host;
    int width = 1280;
    int height = 720;
    int fps = 60;
    int bitrateKbps = 15000;
    bool lowLatencyPacing = false;
    // Takes priority over lowLatencyPacing, as in AVFrameQueue::configure.
    bool cadencePacing = false;
    int queueSize = 3;
    std::string codec = "HEVC";
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
    uint64_t m_prev_holds = 0;
    uint64_t m_prev_net_drops = 0;
    uint64_t m_prev_late = 0;
    uint64_t m_prev_reanchors = 0;
    uint64_t m_total_net_drops = 0;
    uint64_t m_total_late = 0;
    uint64_t m_total_reanchors = 0;
    artemis::streaming::SlotIntervalCounts m_prev_slots;
    artemis::streaming::SlotIntervalCounts m_total_slots;
    uint32_t m_elapsed_sec = 0;

    // Session totals of the per-second counter deltas.
    uint64_t m_total_uflows = 0;
    uint64_t m_total_skips = 0;
    uint64_t m_total_oflows = 0;
    uint64_t m_total_pskips = 0;
    uint64_t m_total_holds = 0;

    // One value per logged second (only while frames are being rendered),
    // used for the min/mean/p95/max summary.
    std::vector<float> m_rfps_series;
    std::vector<float> m_rcv_fps_series;
    std::vector<float> m_dec_fps_series;
    std::vector<float> m_qdepth_series;
    std::vector<float> m_rcv_ms_series;
    std::vector<float> m_dec_ms_series;
    std::vector<float> m_pts_max_series;
    std::vector<float> m_judder_series;
    std::vector<float> m_qwait_series;
    std::vector<float> m_cpipe_series;
    std::vector<float> m_cpipe_max_series;
};

} // namespace artemis::benchmark
