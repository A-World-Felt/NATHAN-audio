#include "frame_stats.h"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {
bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }
}  // namespace

int main() {
    using namespace web_demo;

    RollingFrameStats stats(10);
    for (double v : {10.0, 20.0, 30.0, 40.0, 50.0}) stats.addSample(v);

    FrameStatsSnapshot s = stats.snapshot();
    assert(s.count == 5);
    assert(near(s.lastUs, 50.0));
    assert(near(s.meanUs, 30.0));
    assert(near(s.minUs, 10.0));
    assert(near(s.maxUs, 50.0));
    // p95 : rang = 0.95*(5-1) = 3.8 -> interpolation entre l'indice 3 (40)
    // et l'indice 4 (50), frac=0.8 -> 40*0.2 + 50*0.8 = 48.
    assert(near(s.p95Us, 48.0));

    // Fenetre glissante : windowSize=10, on pousse 12 echantillons -> les
    // 2 plus anciens (10, 20) doivent avoir disparu.
    RollingFrameStats windowed(10);
    for (int i = 1; i <= 12; ++i) windowed.addSample(static_cast<double>(i) * 10.0);
    FrameStatsSnapshot w = windowed.snapshot();
    assert(w.count == 10);
    assert(near(w.minUs, 30.0));   // 10 et 20 sont sortis
    assert(near(w.maxUs, 120.0));
    assert(near(w.lastUs, 120.0));

    std::printf("frame_stats_test : OK\n");
    return 0;
}
