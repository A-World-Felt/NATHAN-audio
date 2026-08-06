// Verification par assert() — pas de framework de test dans ce repo (voir
// docs/superpowers/specs/2026-08-06-web-audio-demo-design.md, "Verification").
#include "spatial_math.h"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {
bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }
}  // namespace

int main() {
    using namespace web_demo;

    // distanceGain : ref=1, max=20, rolloff=1 (constantes du projet).
    assert(near(distanceGain(1.0f, 1.0f, 20.0f, 1.0f), 1.0f));
    assert(near(distanceGain(2.0f, 1.0f, 20.0f, 1.0f), 0.5f));
    assert(near(distanceGain(20.0f, 1.0f, 20.0f, 1.0f), 0.05f));
    assert(near(distanceGain(50.0f, 1.0f, 20.0f, 1.0f), 0.05f));  // clamp a max=20

    // distanceMeters
    assert(near(distanceMeters(3.0f, 4.0f), 5.0f));
    assert(near(distanceMeters(0.0f, 0.0f), 0.0f));

    // azimuthDegrees : avant=0, droite=+90, derriere=180, gauche=-90.
    assert(near(azimuthDegrees(0.0f, -5.0f), 0.0f));
    assert(near(azimuthDegrees(5.0f, 0.0f), 90.0f));
    assert(near(azimuthDegrees(0.0f, 5.0f), 180.0f));
    assert(near(azimuthDegrees(-5.0f, 0.0f), -90.0f));

    std::printf("spatial_math_test : OK\n");
    return 0;
}
