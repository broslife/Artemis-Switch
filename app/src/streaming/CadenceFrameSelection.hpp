#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>

// Tick-based frame choice for the cadence pacing mode. One call per draw tick:
// show the newest frame whose due time has passed, drop older due frames (the
// newer one wins), and repeat the last image when nothing is due.
// Adapted from Punktfunk pace_slot / SlotClock (MIT / Apache-2.0),
// crates/core/punktfunk-core/src/phase.rs. The Switch gives no vsync
// timestamps, so the draw tick stands in for the display slot.
namespace artemis::streaming {

// Frame must expose `int64_t dueNs` (client clock domain, same as nowNs).
// On true, the frame to show is queue.front().
template <class Frame, class DropFrame>
    requires std::invocable<DropFrame&, Frame&>
bool selectCadenceFrame(std::deque<Frame>& queue, int64_t nowNs,
                        DropFrame&& dropFrame) {
    // Leading frames whose due time has passed. Due times follow the source
    // cadence, so they are in order and the first frame not due ends the scan.
    std::size_t due = 0;
    for (const Frame& f : queue) {
        if (f.dueNs > nowNs)
            break;
        ++due;
    }
    if (due == 0)
        return false;

    while (due > 1) {
        dropFrame(queue.front());
        queue.pop_front();
        --due;
    }
    return true;
}

} // namespace artemis::streaming
