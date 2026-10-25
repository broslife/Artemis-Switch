#include "SlotIntervals.hpp"

#include <cassert>
#include <cmath>

using artemis::streaming::SlotIntervalCounts;
using artemis::streaming::SlotIntervals;

int main() {
    // Perfect 60 fps on 60 Hz: a new frame every tick, no judder.
    {
        SlotIntervals s;
        for (int i = 0; i < 101; ++i)
            s.onTick(true);
        const auto c = s.counts();
        assert(c.n1 == 100 && c.n0 == 0 && c.n2 == 0 && c.n3plus == 0);
        assert(c.judderPermille() == 0.0);
    }

    // Steady 30 fps on 60 Hz: all n2, still zero judder (mode-based).
    {
        SlotIntervals s;
        for (int i = 0; i < 101; ++i) {
            s.onTick(true);
            s.onTick(false);
        }
        const auto c = s.counts();
        assert(c.n2 == 100 && c.n1 == 0);
        assert(c.judderPermille() == 0.0);
    }

    // One repeated tick among 10 intervals: 1 n2 vs 9 n1 -> 100 per-mille.
    {
        SlotIntervals s;
        s.onTick(true);
        for (int i = 0; i < 9; ++i)
            s.onTick(true);
        s.onTick(false);
        s.onTick(true);
        const auto c = s.counts();
        assert(c.n1 == 9 && c.n2 == 1);
        assert(std::abs(c.judderPermille() - 100.0) < 1e-9);
    }

    // Long gap lands in n3+; the first frame has no predecessor.
    {
        SlotIntervals s;
        s.onTick(false);
        s.onTick(true);
        s.onTick(false);
        s.onTick(false);
        s.onTick(false);
        s.onTick(true);
        const auto c = s.counts();
        assert(c.total() == 1 && c.n3plus == 1);
    }

    // Empty counts and per-second deltas.
    {
        assert(SlotIntervalCounts{}.judderPermille() == 0.0);
        const SlotIntervalCounts a{0, 10, 2, 1};
        const SlotIntervalCounts b{1, 25, 2, 4};
        const auto d = b.since(a);
        assert(d.n0 == 1 && d.n1 == 15 && d.n2 == 0 && d.n3plus == 3);
    }

    // Same-slot frames count as n0 and reset clears everything.
    {
        SlotIntervals s;
        s.onTick(true);
        s.onSameSlotFrame();
        assert(s.counts().n0 == 1);
        s.reset();
        assert(s.counts().total() == 0);
    }

    return 0;
}
