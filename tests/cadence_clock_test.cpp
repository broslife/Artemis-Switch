#include "CadenceClock.hpp"
#include "CadenceFrameSelection.hpp"
#include "SlotIntervals.hpp"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <vector>

using artemis::streaming::CadenceClock;
using artemis::streaming::SlotIntervals;
using artemis::streaming::selectCadenceFrame;

namespace {

constexpr int64_t kPeriod = 16'666'667; // 60 Hz
constexpr uint64_t kPts0 = 1'000'000'000;
constexpr int64_t kDomain = 123'456'789'000; // host clock vs client clock
constexpr int64_t kDelay = 4'000'000;        // transport + decode

// Deterministic LCG, spread in [-spread, spread); no OS randomness in tests.
struct Lcg {
    uint64_t s;
    int64_t noise(int64_t spread) {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        if (spread == 0)
            return 0;
        return static_cast<int64_t>((s >> 33) % (2 * spread)) - spread;
    }
};

uint64_t ptsAt(int64_t k) { return kPts0 + static_cast<uint64_t>(k * kPeriod); }

CadenceClock settled(int n, int64_t spread) {
    CadenceClock c;
    Lcg rng{7};
    for (int k = 0; k < n; ++k) {
        const int64_t ready = static_cast<int64_t>(ptsAt(k)) + kDomain + kDelay +
                              rng.noise(spread);
        c.dueNs(ptsAt(k), ready, kPeriod);
    }
    return c;
}

struct SimFrame {
    int idx;
    int64_t dueNs;
};

// Judder (per-mille) of showing the newest arrived frame at each tick
// (arrival) versus the cadence rule. Ticks sit at an arbitrary phase.
void simulate(int64_t jitterNs, double phase, uint64_t seed, double& arrival,
              double& cadence) {
    constexpr int kFrames = 3000;
    constexpr int kWarmup = 100;
    Lcg rng{seed};

    std::vector<int64_t> ready(kFrames);
    std::vector<int64_t> due(kFrames);
    CadenceClock clock;
    int64_t last = 0;
    for (int k = 0; k < kFrames; ++k) {
        int64_t r = static_cast<int64_t>(ptsAt(k)) + kDomain + kDelay +
                    rng.noise(jitterNs);
        if (r <= last)
            r = last + 1; // an in-order decoder never reorders
        last = r;
        ready[k] = r;
        due[k] = clock.dueNs(ptsAt(k), r, kPeriod);
    }

    const int64_t tick0 = static_cast<int64_t>(kPts0) + kDomain + kDelay +
                          static_cast<int64_t>(phase * kPeriod) + 20 * kPeriod;

    for (int mode = 0; mode < 2; ++mode) {
        SlotIntervals slots;
        std::deque<SimFrame> queue;
        int next = 0;
        int shown = -1;
        for (int i = 0; i < kFrames - 40; ++i) {
            const int64_t t = tick0 + i * kPeriod + rng.noise(300'000);
            while (next < kFrames && ready[next] <= t) {
                queue.push_back({next, due[next]});
                ++next;
            }
            int pick = -1;
            if (mode == 0) {
                if (!queue.empty()) {
                    pick = queue.back().idx;
                    queue.clear();
                }
            } else if (selectCadenceFrame(queue, t, [](SimFrame&) {})) {
                pick = queue.front().idx;
                queue.pop_front();
            }
            const bool fresh = pick > shown;
            if (fresh)
                shown = pick;
            if (i >= kWarmup)
                slots.onTick(fresh);
        }
        (mode == 0 ? arrival : cadence) = slots.counts().judderPermille();
    }
}

struct Due {
    int64_t dueNs;
};

} // namespace

int main() {
    // The loop converges on the true transport delay (clock domain absorbed).
    {
        const CadenceClock c = settled(400, 1'000'000);
        const int64_t err = c.health().offsetNs - (kDomain + kDelay);
        assert(std::llabs(err) < 500'000);
        assert(c.health().reanchors == 1); // only the cold start anchors
    }

    // A long gap and a backwards timestamp both re-anchor.
    {
        CadenceClock c = settled(400, 200'000);
        const uint64_t anchors = c.health().reanchors;
        const uint64_t far = ptsAt(400) + 2'000'000'000ull;
        const int64_t ready = static_cast<int64_t>(far) + kDomain + kDelay;
        c.dueNs(far, ready, kPeriod);
        assert(c.health().reanchors == anchors + 1);
        assert(c.health().offsetNs == ready - static_cast<int64_t>(far));

        CadenceClock b = settled(400, 200'000);
        const uint64_t back = ptsAt(200);
        b.dueNs(back, static_cast<int64_t>(back) + kDomain + kDelay, kPeriod);
        assert(b.health().reanchors == anchors + 1);
    }

    // A frame 30 ms late reads as already due and is counted, not clamped.
    {
        CadenceClock c = settled(400, 200'000);
        const int64_t ready =
            static_cast<int64_t>(ptsAt(400)) + kDomain + kDelay + 30'000'000;
        const int64_t due = c.dueNs(ptsAt(400), ready, kPeriod);
        assert(due < ready);
        assert(c.health().late == 1);
    }

    // The cushion never exceeds one frame interval, even with huge jitter.
    {
        CadenceClock c;
        Lcg rng{17};
        for (int k = 0; k < 500; ++k) {
            const int64_t ready = static_cast<int64_t>(ptsAt(k)) + kDomain +
                                  kDelay + rng.noise(40'000'000);
            c.dueNs(ptsAt(k), ready, kPeriod);
            assert(c.cushionNs() <= kPeriod);
        }
    }

    // A constant shift of the client clock moves every due time by the same
    // constant (callers feed one domain; no conversion in the path).
    {
        constexpr int64_t kShift = 987'654'321'000;
        CadenceClock a;
        CadenceClock b;
        Lcg ra{11};
        Lcg rb{11};
        for (int k = 0; k < 300; ++k) {
            const int64_t base = static_cast<int64_t>(ptsAt(k)) + kDomain +
                                 kDelay;
            const int64_t da = a.dueNs(ptsAt(k), base + ra.noise(2'000'000),
                                       kPeriod);
            const int64_t db = b.dueNs(
                ptsAt(k), base + kShift + rb.noise(2'000'000), kPeriod);
            assert(db - da == kShift);
        }
    }

    // Timestamps are never smoothed: an irregular source stays irregular.
    {
        CadenceClock c;
        Lcg rng{13};
        uint64_t pts = kPts0;
        std::vector<int64_t> dues;
        std::vector<int64_t> ptss;
        for (int k = 0; k < 300; ++k) {
            pts += static_cast<uint64_t>(k % 2 == 0 ? kPeriod / 2
                                                    : kPeriod * 3 / 2);
            const int64_t ready = static_cast<int64_t>(pts) + kDomain + kDelay +
                                  rng.noise(500'000);
            ptss.push_back(static_cast<int64_t>(pts));
            dues.push_back(c.dueNs(pts, ready, kPeriod));
        }
        for (size_t i = 200; i < dues.size(); ++i) {
            const int64_t dDue = dues[i] - dues[i - 1];
            const int64_t dPts = ptss[i] - ptss[i - 1];
            assert(std::llabs(dDue - dPts) < 200'000);
        }
    }

    // Frame choice: nothing due holds; several due keep only the newest.
    {
        std::deque<Due> q{{1'000}, {2'000}, {9'000}};
        int dropped = 0;
        assert(!selectCadenceFrame(q, 500, [&](Due&) { ++dropped; }));
        assert(q.size() == 3 && dropped == 0);

        assert(selectCadenceFrame(q, 2'500, [&](Due&) { ++dropped; }));
        assert(dropped == 1);
        assert(q.size() == 2 && q.front().dueNs == 2'000);

        std::deque<Due> empty;
        assert(!selectCadenceFrame(empty, 1, [](Due&) {}));
    }

    // On a jittery link the cadence rule is much steadier than showing the
    // newest arrived frame, whatever the phase of the display ticks.
    {
        double arrivalSum = 0.0;
        double cadenceSum = 0.0;
        double cadenceWorst = 0.0;
        constexpr int kPhases = 10;
        for (int p = 0; p < kPhases; ++p) {
            double a = 0.0;
            double c = 0.0;
            simulate(4'000'000, p / 10.0, 1, a, c);
            arrivalSum += a;
            cadenceSum += c;
            if (c > cadenceWorst)
                cadenceWorst = c;
        }
        assert(cadenceSum / kPhases < (arrivalSum / kPhases) / 2.0);
        assert(cadenceWorst < 100.0);
    }

    return 0;
}
