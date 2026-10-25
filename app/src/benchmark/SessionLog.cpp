#include "SessionLog.hpp"

#include "AVFrameHolder.hpp"
#include "FramePipelineTelemetry.hpp"
#include "MoonlightSession.hpp"
#include "SwitchRuntimeMetadata.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
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

// Per-second delta of a cumulative counter; a counter that went backwards was
// reset, so its current value is the delta.
static uint64_t deltaSince(uint64_t current, uint64_t previous) {
    return current >= previous ? current - previous : current;
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

    const std::string mode = cfg.cadencePacing
                                 ? "cadence"
                                 : (cfg.lowLatencyPacing ? "lowlat" : "legacy");

    std::ostringstream name;
    name << dateBuf << '_' << safeFilePart(cfg.host) << '_' << mode << ".log";

    return std::string(kLogDirectory) + '/' + name.str();
}

// Appends "# name  min=.. mean=.. p95=.. max=.." for a per-second series.
static void appendSeriesSummary(std::ostream& out, const char* name,
                                const std::vector<float>& series) {
    if (series.empty())
        return;
    std::vector<float> sorted(series);
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (float v : sorted)
        sum += v;
    const size_t p95Index = static_cast<size_t>(
        std::ceil(0.95 * static_cast<double>(sorted.size()))) - 1;
    out << "# " << name << "  min=" << sorted.front()
        << "  mean=" << sum / static_cast<double>(sorted.size())
        << "  p95=" << sorted[std::min(p95Index, sorted.size() - 1)]
        << "  max=" << sorted.back() << "\n";
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
      << "  Codec: " << cfg.codec
      << "  Bitrate: " << cfg.bitrateKbps << " kbps\n"
      << "# Pacing: "
      << (cfg.cadencePacing
              ? "cadence"
              : (cfg.lowLatencyPacing ? "low-latency" : "legacy"))
      << "  QueueSize: " << cfg.queueSize
      << (cfg.cadencePacing
              ? (cfg.cadenceCushion == 1
                     ? "  Cushion: tight"
                     : (cfg.cadenceCushion >= 2 ? "  Cushion: minimal"
                                                : "  Cushion: standard"))
              : "")
      << "\n"
      << "# Switch: " << meta.operationMode << "\n"
      << "# File: " << logPath << "\n"
      << "#\n"
      << "# Columns:\n"
      << "#   sec  rfps  rcv_fps  dec_fps"
      << "  uflows/s  skips/s  oflows/s  pskips/s  holds/s"
      << "  qdepth  rcv_ms  dec_ms"
      << "  pts_mean_ms  pts_max_ms  pts_std_ms"
      << "  judder_permille  n0  n1  n2  n3plus  shown"
      << "  netdrops/s  late/s  reanchors/s  cushion_ms"
      << "  qwait_ms  qwait_max  cpipe_ms  cpipe_max\n"
      << "# n0..n3plus = display slots between two new frames (0, 1, 2, 3+);"
      << " judder_permille = share outside the bucket expected for the stream"
      << " fps (n1 at 60 fps); shown = new frames put on screen that second.\n"
      << "# netdrops = frames lost on the network; late, reanchors, cushion_ms"
      << " are only filled in cadence mode.\n"
      << "# Client latency per new frame (ms, mean and max over the second):"
      << " qwait = decode done to chosen for display; cpipe = packet fully"
      << " received to submitted for display. Host and network transit are"
      << " not included.\n"
      << "sec\trfps\trcv_fps\tdec_fps"
      << "\tuflows\tskips\toflows\tpskips\tholds"
      << "\tqdepth\trcv_ms\tdec_ms"
      << "\tpts_mean\tpts_max\tpts_std"
      << "\tjudder\tn0\tn1\tn2\tn3plus\tshown"
      << "\tnetdrops\tlate\treanchors\tcushion"
      << "\tqwait\tqwait_max\tcpipe\tcpipe_max\n";
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
    m_prev_holds = 0;
    m_prev_net_drops = 0;
    m_prev_late = 0;
    m_prev_reanchors = 0;
    m_total_net_drops = 0;
    m_total_late = 0;
    m_total_reanchors = 0;
    m_prev_slots = {};
    m_total_slots = {};
    // Display slots between new frames if the stream is perfectly paced.
    m_expected_slots = std::clamp(
        config.fps > 0 ? static_cast<int>(std::lround(60.0 / config.fps)) : 1,
        1, 3);
    m_judder_series.clear();
    m_qwait_series.clear();
    m_cpipe_series.clear();
    m_cpipe_max_series.clear();
    // Drop anything accumulated before this session started.
    (void)artemis::streaming::FramePipelineTelemetry::instance()
        .takeLatencyWindow();
    m_elapsed_sec = 0;
    m_total_uflows = 0;
    m_total_skips = 0;
    m_total_oflows = 0;
    m_total_pskips = 0;
    m_total_holds = 0;
    m_rfps_series.clear();
    m_rcv_fps_series.clear();
    m_dec_fps_series.clear();
    m_qdepth_series.clear();
    m_rcv_ms_series.clear();
    m_dec_ms_series.clear();
    m_pts_max_series.clear();
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

        if (!m_rfps_series.empty()) {
            m_file << std::fixed << std::setprecision(2)
                   << "# SESSION SUMMARY\n"
                   << "# Duration: " << m_elapsed_sec << " s"
                   << "  Samples: " << m_rfps_series.size() << "\n"
                   << "# Totals  uflows=" << m_total_uflows
                   << "  skips=" << m_total_skips
                   << "  oflows=" << m_total_oflows
                   << "  pskips=" << m_total_pskips
                   << "  holds=" << m_total_holds << "\n"
                   << "# Slots  n0=" << m_total_slots.n0
                   << "  n1=" << m_total_slots.n1
                   << "  n2=" << m_total_slots.n2
                   << "  n3plus=" << m_total_slots.n3plus
                   << "  judder_permille="
                   << m_total_slots.judderPermille(m_expected_slots) << "\n"
                   << "# Network  netdrops=" << m_total_net_drops
                   << "  cadence_late=" << m_total_late
                   << "  cadence_reanchors=" << m_total_reanchors << "\n";
            appendSeriesSummary(m_file, "judder_permille", m_judder_series);
            appendSeriesSummary(m_file, "qwait_ms", m_qwait_series);
            appendSeriesSummary(m_file, "cpipe_ms", m_cpipe_series);
            appendSeriesSummary(m_file, "cpipe_max_ms", m_cpipe_max_series);
            appendSeriesSummary(m_file, "rendered_fps", m_rfps_series);
            appendSeriesSummary(m_file, "rcv_fps", m_rcv_fps_series);
            appendSeriesSummary(m_file, "dec_fps", m_dec_fps_series);
            appendSeriesSummary(m_file, "qdepth", m_qdepth_series);
            appendSeriesSummary(m_file, "rcv_ms", m_rcv_ms_series);
            appendSeriesSummary(m_file, "dec_ms", m_dec_ms_series);
            appendSeriesSummary(m_file, "pts_max_ms", m_pts_max_series);
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

    const uint64_t d_uflows = deltaSince(uflows, m_prev_uflows);
    const uint64_t d_skips = deltaSince(skips, m_prev_skips);
    const uint64_t d_oflows = deltaSince(oflows, m_prev_oflow_drops);
    const uint64_t d_pskips = deltaSince(pskips, m_prev_pacing_skips);
    const uint64_t holds = holder.getFrameQueueScheduledHoldStat();
    const uint64_t d_holds = deltaSince(holds, m_prev_holds);

    m_prev_uflows = uflows;
    m_prev_skips = skips;
    m_prev_oflow_drops = oflows;
    m_prev_pacing_skips = pskips;
    m_prev_holds = holds;

    m_total_uflows += d_uflows;
    m_total_skips += d_skips;
    m_total_oflows += d_oflows;
    m_total_pskips += d_pskips;
    m_total_holds += d_holds;

    const auto slots = holder.getSlotIntervalCounts();
    const auto d_slots = slots.since(m_prev_slots);
    m_prev_slots = slots;
    m_total_slots = slots;

    // Cumulative in this session; clamp keeps a counter reset from underflowing.
    const uint64_t netDrops = dec.network_dropped_frames;
    const uint64_t d_netDrops =
        netDrops >= m_prev_net_drops ? netDrops - m_prev_net_drops : 0;
    m_prev_net_drops = netDrops;
    m_total_net_drops += d_netDrops;

    const auto cadence = holder.getCadenceHealth();
    const uint64_t d_late =
        cadence.late >= m_prev_late ? cadence.late - m_prev_late : 0;
    const uint64_t d_reanchors = cadence.reanchors >= m_prev_reanchors
                                     ? cadence.reanchors - m_prev_reanchors
                                     : 0;
    m_prev_late = cadence.late;
    m_prev_reanchors = cadence.reanchors;
    m_total_late += d_late;
    m_total_reanchors += d_reanchors;

    const auto lat = artemis::streaming::FramePipelineTelemetry::instance()
                         .takeLatencyWindow();

    const float rfps = ren.rendered_fps;
    if (rfps > 0.0f) {
        if (lat.frames > 0) {
            m_qwait_series.push_back(lat.queueWaitMeanMs);
            m_cpipe_series.push_back(lat.clientPipelineMeanMs);
            m_cpipe_max_series.push_back(lat.clientPipelineMaxMs);
        }
        m_judder_series.push_back(
            static_cast<float>(d_slots.judderPermille(m_expected_slots)));
        m_rfps_series.push_back(rfps);
        m_rcv_fps_series.push_back(dec.current_received_fps);
        m_dec_fps_series.push_back(dec.current_decoded_fps);
        m_qdepth_series.push_back(
            static_cast<float>(holder.getFrameQueueSize()));
        m_rcv_ms_series.push_back(dec.current_receive_time);
        m_dec_ms_series.push_back(dec.current_decoding_time);
        m_pts_max_series.push_back(dec.current_pts_delta_max_ms);
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
         << '\t' << d_holds
         << '\t' << holder.getFrameQueueSize()
         << '\t' << dec.current_receive_time
         << '\t' << dec.current_decoding_time
         << std::setprecision(2)
         << '\t' << dec.current_pts_delta_mean_ms
         << '\t' << dec.current_pts_delta_max_ms
         << '\t' << dec.current_pts_delta_stddev_ms
         << std::setprecision(0)
         << '\t' << d_slots.judderPermille(m_expected_slots)
         << '\t' << d_slots.n0
         << '\t' << d_slots.n1
         << '\t' << d_slots.n2
         << '\t' << d_slots.n3plus
         << '\t' << d_slots.total()
         << '\t' << d_netDrops
         << '\t' << d_late
         << '\t' << d_reanchors
         << std::setprecision(2)
         << '\t' << static_cast<double>(cadence.cushionNs) / 1.0e6
         << '\t' << lat.queueWaitMeanMs
         << '\t' << lat.queueWaitMaxMs
         << '\t' << lat.clientPipelineMeanMs
         << '\t' << lat.clientPipelineMaxMs
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
