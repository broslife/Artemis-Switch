#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>

// Judder ruler: counts how many display slots (draw ticks, one per vsync)
// pass between two consecutive NEW frames that reach the screen.
// Idea and bucket layout adapted from the Punktfunk project (MIT / Apache-2.0),
// clients/android/native/src/present/ (SlotIntervals).
namespace artemis::streaming {

struct SlotIntervalCounts {
    uint64_t n0 = 0;     // two new frames in the same slot
    uint64_t n1 = 0;     // one slot apart (ideal when stream fps == display Hz)
    uint64_t n2 = 0;     // two slots apart
    uint64_t n3plus = 0; // three or more slots apart

    [[nodiscard]] uint64_t total() const { return n0 + n1 + n2 + n3plus; }

    // Share of intervals that are NOT the most common one, in per-mille.
    // A steady 30 fps source on a 60 Hz display is all n2, so it reads 0.
    [[nodiscard]] double judderPermille() const {
        const uint64_t all = total();
        if (all == 0)
            return 0.0;
        const uint64_t mode = std::max(std::max(n0, n1), std::max(n2, n3plus));
        return 1000.0 * static_cast<double>(all - mode) /
               static_cast<double>(all);
    }

    // Counts accumulated since an earlier snapshot (per-second deltas).
    [[nodiscard]] SlotIntervalCounts since(const SlotIntervalCounts& prev) const {
        return {n0 - prev.n0, n1 - prev.n1, n2 - prev.n2, n3plus - prev.n3plus};
    }
};

// Written by the render thread, read by the overlay and the session log.
class SlotIntervals {
  public:
    // Call once per draw tick. newFrame is true when the tick showed a frame
    // that was not on screen at the previous tick.
    void onTick(bool newFrame) {
        if (!newFrame) {
            if (m_started)
                ++m_slots_since_new;
            return;
        }
        if (m_started) {
            // The tick that shows the new frame is one slot after the last tick.
            const uint32_t gap = m_slots_since_new + 1;
            if (gap == 1)
                m_n1.fetch_add(1, std::memory_order_relaxed);
            else if (gap == 2)
                m_n2.fetch_add(1, std::memory_order_relaxed);
            else
                m_n3plus.fetch_add(1, std::memory_order_relaxed);
        }
        m_started = true;
        m_slots_since_new = 0;
    }

    // Two new frames reported inside one slot (renderer drew twice per tick).
    void onSameSlotFrame() {
        if (m_started)
            m_n0.fetch_add(1, std::memory_order_relaxed);
    }

    void reset() {
        m_n0 = 0;
        m_n1 = 0;
        m_n2 = 0;
        m_n3plus = 0;
        m_started = false;
        m_slots_since_new = 0;
    }

    [[nodiscard]] SlotIntervalCounts counts() const {
        return {m_n0.load(std::memory_order_relaxed),
                m_n1.load(std::memory_order_relaxed),
                m_n2.load(std::memory_order_relaxed),
                m_n3plus.load(std::memory_order_relaxed)};
    }

  private:
    std::atomic<uint64_t> m_n0{0};
    std::atomic<uint64_t> m_n1{0};
    std::atomic<uint64_t> m_n2{0};
    std::atomic<uint64_t> m_n3plus{0};
    // Render thread only.
    bool m_started = false;
    uint32_t m_slots_since_new = 0;
};

} // namespace artemis::streaming
