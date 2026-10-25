#include "SessionLog.hpp"

#include "AVFrameHolder.hpp"
#include "MoonlightSession.hpp"
#include "SwitchRuntimeMetadata.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

#ifndef ARTEMIS_BUILD_HASH
#define ARTEMIS_BUILD_HASH "unknown"
#endif
#ifndef APP_VERSION
#define APP_VERSION "?.?.?"
#endif

namespace artemis::benchmark {

// Flush to SD every this many buffered lines to limit write cycles.
static constexpr int kFlushInterval = 10;

#ifdef PLATFORM_SWITCH
static constexpr const char* kLogDirectory =
    "sdmc:/switch/Artemis-Switch/logs";
#else
static constexpr const char* kLogDirectory = nullptr;
#endif

// ---- helpers ----------------------------------------------------------------

static std::string isoUtcNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32]{};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

// Replaces characters that are unsafe in filenames with '_'.
static std::string safeFilePart(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '.')
            r.push_back(static_cast<char>(c));
        else
            r.push_back('_');
    }
    return r.empty() ? "session" : r;
}

static std::string makeLogPath(const SessionLogConfig& cfg) {
    if (!kLogDirectory)
        return {};

    // Name: YYYY-MM-DDTHH-MM-SS_<host>_<mode>.log
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char dateBuf[32]{};
    std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%dT%H-%M-%S", &tm);

    const std::string mode =
        cfg.lowLatencyPacing ? "lowlat" : "legacy";

    std::ostringstream name;
    name << dateBuf << '_' << safeFilePart(cfg.host) << '_' << mode << ".log";

    return std::string(kLogDirectory) + '/' + name.str();
}

static std::string buildHeader(const SessionLogConfig& cfg,
                               const std::string& logPath) {
    const auto meta = collectSwitchRuntimeMetadata();
    std::ostringstream h;
    h << "# Artemis-Switch Session Log\n"
      << "# Build: " << ARTEMIS_BUILD_HASH
      << "  Version: " << APP_VERSION
      << "  Date: " << isoUtcNow() << "\n"
      << "# Host: " << cfg.host << "\n"
      << "# Stream: " << cfg.width << 'x' << cfg.height
      << " @ " << cfg.fps << " fps"
      << "  Bitrate: " << cfg.bitrateKbps << " kbps\n"
      << "# Pacing: " << (cfg.lowLatencyPacing ? "low-latency" : "legacy")
      << "  QueueSize: " << cfg.queueSize << "\n"
      << "# Switch: " << meta.operationMode << "\n"
      << "# File: " << logPath << "\n"
      << "#\n"
      << "# Columns:\n"
      << "#   sec  rfps  rcv_fps  dec_fps"
      << "  uflows/s  skips/s  oflows/s  pskips/s"
      << "  qdepth  rcv_ms  dec_ms"
      << "  pts_mean_ms  pts_max_ms  pts_std_ms\n"
      << "sec\trfps\trcv_fps\tdec_fps"
      << "\tuflows\tskips\toflows\tpskips"
      << "\tqdepth\trcv_ms\tdec_ms"
      << "\tpts_mean\tpts_max\tpts_std\n";
    return h.str();
}

// ---- SessionLog -------------------------------------------------------------

SessionLog& SessionLog::instance() {
    static SessionLog log;
    return log;
}

SessionLog::~SessionLog() {
    if (m_running.load())
        stop();
}

void SessionLog::start(const SessionLogConfig& config) {
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true))
        return;

    m_prev_uflows = 0;
    m_prev_skips = 0;
    m_prev_oflow_drops = 0;
    m_prev_pacing_skips = 0;
    m_elapsed_sec = 0;
    m_rfps_sum = 0.0;
    m_rfps_min = 999.0f;
    m_rfps_max = 0.0f;
    m_rfps_count = 0;
    m_pending.clear();

    const std::string logPath = makeLogPath(config);
    if (!logPath.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(kLogDirectory, ec);
        if (!ec) {
            m_file.open(logPath, std::ios::out | std::ios::trunc);
            if (m_file.is_open()) {
                m_file << buildHeader(config, logPath);
                m_file.flush();
            }
        }
    }

    m_worker = std::thread([this] { workerLoop(); });
}

void SessionLog::stop() {
    m_running.store(false);
    if (m_worker.joinable())
        m_worker.join();

    // Write any pending lines and the session summary.
    if (m_file.is_open()) {
        flushPending();

        if (m_rfps_count > 0) {
            const double mean = m_rfps_sum / m_rfps_count;
            m_file << "# SESSION SUMMARY\n"
                   << "# Duration: " << m_elapsed_sec << " s"
                   << "  Samples: " << m_rfps_count << "\n"
                   << "# rendered_fps  mean=" << std::fixed
                   << std::setprecision(2) << mean
                   << "  min=" << m_rfps_min
                   << "  max=" << m_rfps_max << "\n";
        }
        m_file.flush();
        m_file.close();
    }
}

void SessionLog::workerLoop() {
    // Use 100 ms ticks so stop() unblocks within 100 ms; count 10 ticks
    // before calling captureLine() to achieve the ~1 s sampling rate.
    int ticks = 0;
    while (m_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!m_running.load())
            break;
        if (++ticks >= 10) {
            ticks = 0;
            captureLine();
        }
    }
}

void SessionLog::captureLine() {
    auto* session = MoonlightSession::activeSession();
    if (!session || !session->is_active())
        return;

    const auto* stats = session->session_stats();
    if (!stats)
        return;

    const auto& dec = stats->video_decode_stats;
    const auto& ren = stats->video_render_stats;
    auto& holder = AVFrameHolder::instance();

    const uint64_t uflows = holder.getFakeFrameStat();
    const uint64_t skips = holder.getFrameDropStat();
    const uint64_t oflows = holder.getFrameQueueOverflowDropStat();
    const uint64_t pskips = holder.getFrameQueuePacingSkipStat();

    const uint64_t d_uflows = uflows - m_prev_uflows;
    const uint64_t d_skips = skips - m_prev_skips;
    const uint64_t d_oflows = oflows - m_prev_oflow_drops;
    const uint64_t d_pskips = pskips - m_prev_pacing_skips;

    m_prev_uflows = uflows;
    m_prev_skips = skips;
    m_prev_oflow_drops = oflows;
    m_prev_pacing_skips = pskips;

    const float rfps = ren.rendered_fps;
    if (rfps > 0.0f) {
        m_rfps_sum += rfps;
        if (rfps < m_rfps_min) m_rfps_min = rfps;
        if (rfps > m_rfps_max) m_rfps_max = rfps;
        m_rfps_count++;
    }

    std::ostringstream line;
    line << std::fixed << std::setprecision(1);
    line << m_elapsed_sec
         << '\t' << rfps
         << '\t' << dec.current_received_fps
         << '\t' << dec.current_decoded_fps
         << '\t' << d_uflows
         << '\t' << d_skips
         << '\t' << d_oflows
         << '\t' << d_pskips
         << '\t' << holder.getFrameQueueSize()
         << '\t' << dec.current_receive_time
         << '\t' << dec.current_decoding_time
         << std::setprecision(2)
         << '\t' << dec.current_pts_delta_mean_ms
         << '\t' << dec.current_pts_delta_max_ms
         << '\t' << dec.current_pts_delta_stddev_ms
         << '\n';

    m_elapsed_sec++;

    {
        std::lock_guard lock(m_mutex);
        m_pending.push_back(line.str());
    }

    if (static_cast<int>(m_pending.size()) >= kFlushInterval)
        flushPending();
}

void SessionLog::flushPending() {
    std::vector<std::string> lines;
    {
        std::lock_guard lock(m_mutex);
        lines.swap(m_pending);
    }

    if (!m_file.is_open() || lines.empty())
        return;

    for (const auto& l : lines) {
        m_file << l;
        if (!m_file.good())
            break;
    }
    m_file.flush();
}

} // namespace artemis::benchmark
