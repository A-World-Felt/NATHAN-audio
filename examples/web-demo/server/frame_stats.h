#pragma once

#include <cstddef>
#include <deque>

namespace web_demo {

struct FrameStatsSnapshot {
    std::size_t count = 0;
    double lastUs = 0.0;
    double meanUs = 0.0;
    double minUs = 0.0;
    double maxUs = 0.0;
    double p95Us = 0.0;
};

// Fenetre glissante des temps de traitement par frame — memes methodes que
// audio_benchmark.cpp/house_benchmark.cpp (moyenne, min/max, percentile par
// interpolation lineaire), mais bornee en continu (serveur toujours actif)
// plutot qu'accumulee sur une session de duree fixe.
class RollingFrameStats {
public:
    explicit RollingFrameStats(std::size_t windowSize);

    void addSample(double frameTimeUs);
    FrameStatsSnapshot snapshot() const;

private:
    std::size_t windowSize_;
    std::deque<double> samples_;
};

}  // namespace web_demo
