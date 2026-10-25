#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

// Source-timestamp playout clock. The due time of a frame follows the cadence
// stamped by the host, not the instant the frame arrived from the network.
// Ported to C++ from the Punktfunk project (MIT / Apache-2.0),
// crates/core/punktfunk-core/src/phase.rs (CadenceClock, CadenceTuning,
// CadenceHealth). Integer math only, so tests produce identical numbers on
// every platform. Only the "snapping" tuning is kept: the Switch panel is a
// fixed 60 Hz display.
namespace artemis::streaming {

struct CadenceTuning {
    // Proportional gain: 1 >> offsetShift of the residual per frame.
    uint8_t offsetShift = 5;
    // Integral gain on the per-frame drift (skew): 1 >> skewShift.
    uint8_t skewShift = 10;
    // EMA of |residual|.
    uint8_t jitterShift = 5;
    // One outlier must not yank the estimate.
    int64_t errorClampNs = 20'000'000;
    // Cushion = mad * cushionNum / cushionDen, clamped to
    // [cushionFloorNs, frame interval].
    uint16_t cushionNum = 2;
    uint16_t cushionDen = 1;
    int64_t cushionFloorNs = 500'000;
    // Source timestamp gap beyond which the loop re-anchors instead of tracking.
    int64_t reanchorGapNs = 500'000'000;
};

// Cushion presets, selectable in Settings so logs can be compared A/B.
// 0 = standard (2x MAD, floor 0.5 ms), 1 = tight (1x MAD, floor 0.25 ms; the
// Punktfunk vrr_latency cushion), 2 = minimal (0.5x MAD, no floor).
inline CadenceTuning cadenceTuningForCushion(int preset) {
    CadenceTuning t;
    if (preset == 1) {
        t.cushionNum = 1;
        t.cushionDen = 1;
        t.cushionFloorNs = 250'000;
    } else if (preset >= 2) {
        t.cushionNum = 1;
        t.cushionDen = 2;
        t.cushionFloorNs = 0;
    }
    return t;
}

struct CadenceHealth {
    uint64_t frames = 0;
    // Due time was already past when the frame became presentable.
    uint64_t late = 0;
    // Gave up tracking (gap, regression, or first sample).
    uint64_t reanchors = 0;
    int64_t offsetNs = 0;
    int64_t skewNs = 0;
    int64_t jitterNs = 0;
    int64_t cushionNs = 0;
};

class CadenceClock {
  public:
    explicit CadenceClock(CadenceTuning tuning = {}) : m_tuning(tuning) {}

    // Re-anchor on the next sample. The measured jitter survives: it describes
    // the link, not the stream.
    void reset() {
        m_hasLastPts = false;
        m_skewNs = 0;
    }

    // Due time in the client clock domain. May be earlier than readyNs (late):
    // present at the next opportunity, do not clamp to now.
    int64_t dueNs(uint64_t srcPtsNs, int64_t readyNs, int64_t frameIntervalNs) {
        m_frameIntervalNs = frameIntervalNs;
        m_health.frames++;
        const int64_t pts = static_cast<int64_t>(srcPtsNs);
        const int64_t raw = readyNs - pts;

        bool anchored = false;
        if (m_hasLastPts) {
            // PTS went backwards, or the gap is too long to have tracked:
            // re-anchor rather than slew for seconds.
            anchored = !(srcPtsNs < m_lastPtsNs ||
                         srcPtsNs - m_lastPtsNs >
                             static_cast<uint64_t>(m_tuning.reanchorGapNs));
        }

        if (anchored) {
            m_offsetNs = satAdd(m_offsetNs, m_skewNs);
            const int64_t err = std::clamp(raw - m_offsetNs,
                                           -m_tuning.errorClampNs,
                                           m_tuning.errorClampNs);
            m_offsetNs = satAdd(m_offsetNs, shrTowardZero(err, m_tuning.offsetShift));
            m_skewNs = satAdd(m_skewNs, shrTowardZero(err, m_tuning.skewShift));
            const int64_t dev = (err < 0 ? -err : err) - m_madNs;
            m_madNs += shrTowardZero(dev, m_tuning.jitterShift);
        } else {
            m_offsetNs = raw;
            m_skewNs = 0;
            m_health.reanchors++;
        }
        m_lastPtsNs = srcPtsNs;
        m_hasLastPts = true;

        const int64_t due = satAdd(satAdd(pts, m_offsetNs), cushionNs());
        if (due < readyNs)
            m_health.late++;
        return due;
    }

    // Hold added on top of the tracked offset: a multiple of the typical
    // arrival deviation, never more than one frame interval.
    [[nodiscard]] int64_t cushionNs() const {
        const int64_t den = std::max<int64_t>(m_tuning.cushionDen, 1);
        const int64_t want = (m_madNs * m_tuning.cushionNum) / den;
        const int64_t ceiling =
            m_frameIntervalNs > 0 ? m_frameIntervalNs
                                  : std::numeric_limits<int64_t>::max();
        return std::clamp(want, std::min(m_tuning.cushionFloorNs, ceiling),
                          ceiling);
    }

    [[nodiscard]] CadenceHealth health() const {
        CadenceHealth h = m_health;
        h.offsetNs = m_offsetNs;
        h.skewNs = m_skewNs;
        h.jitterNs = m_madNs;
        h.cushionNs = cushionNs();
        return h;
    }

  private:
    // Plain >> rounds toward minus infinity and would bias a loop that lives
    // within a few nanoseconds of zero error.
    static int64_t shrTowardZero(int64_t v, uint8_t shift) {
        return v < 0 ? -((-v) >> shift) : (v >> shift);
    }

    static int64_t satAdd(int64_t a, int64_t b) {
        if (b > 0 && a > std::numeric_limits<int64_t>::max() - b)
            return std::numeric_limits<int64_t>::max();
        if (b < 0 && a < std::numeric_limits<int64_t>::min() - b)
            return std::numeric_limits<int64_t>::min();
        return a + b;
    }

    CadenceTuning m_tuning;
    int64_t m_offsetNs = 0;
    int64_t m_skewNs = 0;
    int64_t m_madNs = 0;
    uint64_t m_lastPtsNs = 0;
    bool m_hasLastPts = false;
    int64_t m_frameIntervalNs = 0;
    CadenceHealth m_health;
};

} // namespace artemis::streaming
