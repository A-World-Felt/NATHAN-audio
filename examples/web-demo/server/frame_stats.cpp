#include "frame_stats.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace web_demo {

namespace {
// sortedUs doit deja etre trie par ordre croissant.
double percentile(const std::vector<double>& sortedUs, double p) {
    if (sortedUs.empty()) return 0.0;
    std::size_t n = sortedUs.size();
    if (n == 1) return sortedUs[0];
    double rank = p * static_cast<double>(n - 1);
    std::size_t lower = static_cast<std::size_t>(std::floor(rank));
    std::size_t upper = static_cast<std::size_t>(std::ceil(rank));
    if (lower == upper) return sortedUs[lower];
    double frac = rank - static_cast<double>(lower);
    return sortedUs[lower] * (1.0 - frac) + sortedUs[upper] * frac;
}
}  // namespace

RollingFrameStats::RollingFrameStats(std::size_t windowSize) : windowSize_(windowSize) {}

void RollingFrameStats::addSample(double frameTimeUs) {
    samples_.push_back(frameTimeUs);
    while (samples_.size() > windowSize_) samples_.pop_front();
}

FrameStatsSnapshot RollingFrameStats::snapshot() const {
    FrameStatsSnapshot s;
    s.count = samples_.size();
    if (s.count == 0) return s;

    s.lastUs = samples_.back();

    double sum = 0.0;
    for (double v : samples_) sum += v;
    s.meanUs = sum / static_cast<double>(s.count);

    std::vector<double> sorted(samples_.begin(), samples_.end());
    std::sort(sorted.begin(), sorted.end());
    s.minUs = sorted.front();
    s.maxUs = sorted.back();
    s.p95Us = percentile(sorted, 0.95);

    return s;
}

}  // namespace web_demo
