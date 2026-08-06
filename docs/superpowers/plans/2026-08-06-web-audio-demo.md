# Démo web du pipeline audio HRTF — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a two-process local demo — a new C++ executable (`web_demo_server`) that reuses the existing production HRTF module and audio loaders to serve live spatial-audio state over HTTP, and a Vite/TypeScript web page that renders it (draggable player, 4 sprites, vectors, profile selector, live metrics, MP3 decode panel) — so the real audio engine, not a reimplementation, drives what's shown.

**Architecture:** `examples/web-demo/server/` links the existing `nathan_hrtf` (production profile catalog/switching) and `nathan_support` (MP3/WAV loaders) CMake libraries, runs a ~66 Hz OpenAL update loop on the main thread, and serves `GET/POST /api/*` from a second thread via a vendored `httplib.h`, synchronized through one mutex-protected `SharedState`. `examples/web-demo/client/` polls `/api/state`, posts mouse-driven player position and profile selection, and renders everything it receives — it does no audio DSP of its own.

**Tech Stack:** C++17 (existing toolchain: MSVC + vcpkg, OpenAL Soft, libmysofa, nlohmann_json), cpp-httplib (vendored single header), Vite + TypeScript (no framework), native DOM + inline SVG.

## Global Constraints

- Reuse existing code, do not reimplement: profile discovery/switching from `src/hrtf/` (namespace `nathan::hrtf`), audio loading from `benchmarks/src/mp3_loader.h`/`wav_loader.h` (global namespace). Spec: `docs/superpowers/specs/2026-08-06-web-audio-demo-design.md`.
- Distance model: OpenAL's `AL_INVERSE_DISTANCE_CLAMPED`, `AL_REFERENCE_DISTANCE=1.0f`, `AL_MAX_DISTANCE=20.0f`, `AL_ROLLOFF_FACTOR=1.0f` — same constants as `benchmarks/spatial_tests.cpp`.
- Coordinate convention (from `OPENAL_SOFT_NATHAN.md` §4): +X = right, −Z = front, +Z = behind, 1 unit = 1 metre. Azimuth: `atan2(relX, -relZ)` in degrees → 0° front, +90° right, ±180° behind, −90° left.
- 4 fixed sources: `avant` (0,−5, MP3), `droite` (5,0, WAV), `arriere` (0,5, WAV), `gauche` (−5,0, WAV) — exact assets `assets/test-audio.mp3`, `assets/test-audio2.wav`, `assets/test-audio3.wav`, `assets/test-audio4.wav`.
- New code lives in `examples/web-demo/`, never in `benchmarks/` (frozen archive per `benchmarks/README.md`: "ne pas ajouter de nouveau développement ici") and never inside `src/`/`tools/` (production HRTF module, unrelated scope).
- The demo must never call `nathan::hrtf::saveSettings` — it must not modify the real user's persisted `settings.json`. Reading it at startup via `resolveStartupProfile` (read-only) is fine.
- After `active_profile::applyProfile` returns, every previously-created OpenAL buffer/source ID is invalid (the device was destroyed and reopened) — never call `alDeleteSources`/`alDeleteBuffers` on pre-switch IDs; only create fresh ones.
- HTTP API base URL default `http://127.0.0.1:8787`; CORS `Access-Control-Allow-Origin: *` on every response, `OPTIONS` handled explicitly (preflight triggered by `Content-Type: application/json`).
- No automated test framework is introduced for the C++ side beyond plain `assert()`-based executables (the repo has none today; matches its existing "validate by running it" culture, documented explicitly in the spec's "Vérification" section).

---

## File Structure

```
examples/web-demo/
├── README.md
├── server/
│   ├── CMakeLists.txt
│   ├── httplib.h              (vendored, unmodified)
│   ├── spatial_math.h / .cpp   (+ spatial_math_test.cpp)
│   ├── frame_stats.h / .cpp    (+ frame_stats_test.cpp)
│   ├── state_json.h / .cpp     (+ state_json_test.cpp)
│   ├── shared_state.h
│   └── main.cpp
└── client/
    ├── package.json, tsconfig.json, index.html, .env.example
    └── src/
        ├── api.ts
        ├── scene.ts
        ├── metrics.ts
        ├── mp3panel.ts
        ├── profileSelector.ts
        ├── main.ts
        └── style.css
```

Root `CMakeLists.txt` gains one line: `add_subdirectory(examples/web-demo/server)`.

---

### Task 1: Server scaffold — vendored httplib, CMake target, `/api/ping`

**Files:**
- Create: `examples/web-demo/server/httplib.h`
- Create: `examples/web-demo/server/CMakeLists.txt`
- Create: `examples/web-demo/server/main.cpp`
- Modify: `CMakeLists.txt` (root)

**Interfaces:**
- Produces: a running `web_demo_server` executable, HTTP listener on `127.0.0.1:8787`.

- [ ] **Step 1: Vendor httplib.h**

```bash
curl -sS -o examples/web-demo/server/httplib.h https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
```

Verify it downloaded (non-trivial size, starts with the httplib.h license header):

```bash
head -5 examples/web-demo/server/httplib.h
wc -l examples/web-demo/server/httplib.h
```

Expected: `httplib.h` / `Copyright (c) ... Yuji Hirose ... MIT License`, several thousand lines.

- [ ] **Step 2: Write `examples/web-demo/server/CMakeLists.txt`**

```cmake
# Demo web (vitrine) : ni code de production (src/, tools/) ni prototype
# d'exploration (benchmarks/) — voir docs/superpowers/specs/2026-08-06-web-audio-demo-design.md.

add_executable(web_demo_server main.cpp)
target_include_directories(web_demo_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(web_demo_server PRIVATE ws2_32)
endif()
```

- [ ] **Step 3: Write `examples/web-demo/server/main.cpp`**

```cpp
// Démo web — serveur : voir docs/superpowers/specs/2026-08-06-web-audio-demo-design.md.
#include "httplib.h"

#include <cstdio>

int main() {
    httplib::Server svr;
    svr.set_default_headers({{"Access-Control-Allow-Origin", "*"}});

    svr.Get("/api/ping", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"ok":true})", "application/json");
    });

    std::printf("web_demo_server : ecoute sur http://127.0.0.1:8787\n");
    std::fflush(stdout);
    svr.listen("127.0.0.1", 8787);
    return 0;
}
```

- [ ] **Step 4: Wire into the root build**

Modify `CMakeLists.txt` (root), after the existing `add_subdirectory(benchmarks)` line, add:

```cmake

# Demo web (vitrine) : reutilise src/hrtf + benchmarks/src via une API HTTP
# locale. Ni code de production ni prototype d'exploration — voir
# docs/superpowers/specs/2026-08-06-web-audio-demo-design.md.
add_subdirectory(examples/web-demo/server)
```

- [ ] **Step 5: Build and verify**

```bash
cmake --build build --target web_demo_server
```

Expected: builds cleanly, no errors, alongside the existing targets.

- [ ] **Step 6: Run and curl it**

Run `.\build\Debug\web_demo_server.exe` (Windows) in one terminal, then in another:

```bash
curl -s http://127.0.0.1:8787/api/ping
```

Expected: `{"ok":true}`. Stop the server (Ctrl+C) before continuing.

- [ ] **Step 7: Commit**

```bash
git add examples/web-demo/server/httplib.h examples/web-demo/server/CMakeLists.txt examples/web-demo/server/main.cpp CMakeLists.txt
git commit -m "feat(web-demo): scaffold server with vendored httplib and /api/ping"
```

---

### Task 2: `spatial_math` — distance/gain/azimuth (TDD)

**Files:**
- Create: `examples/web-demo/server/spatial_math.h`
- Create: `examples/web-demo/server/spatial_math.cpp`
- Create: `examples/web-demo/server/spatial_math_test.cpp`
- Modify: `examples/web-demo/server/CMakeLists.txt`

**Interfaces:**
- Produces: `web_demo::distanceGain(float,float,float,float)`, `web_demo::distanceMeters(float,float)`, `web_demo::azimuthDegrees(float,float)` — used by Task 5 (`main.cpp`) to fill the per-source fields consumed by Task 4's `state_json`.

- [ ] **Step 1: Write `spatial_math.h`**

```cpp
#pragma once

namespace web_demo {

// Gain lineaire pour AL_INVERSE_DISTANCE_CLAMPED (formule publique OpenAL),
// avec les memes constantes que AL_REFERENCE_DISTANCE/AL_MAX_DISTANCE/
// AL_ROLLOFF_FACTOR passees a OpenAL — donc la meme valeur que celle
// qu'OpenAL applique reellement, pas une approximation.
float distanceGain(float distanceMeters, float referenceDistance, float maxDistance,
                    float rolloffFactor);

// Distance euclidienne 2D entre l'origine et le point (relX, relZ).
float distanceMeters(float relX, float relZ);

// Azimut en degres. Convention identique a OPENAL_SOFT_NATHAN.md #4 :
// 0 = avant (-Z), +90 = droite (+X), +-180 = derriere, -90 = gauche.
float azimuthDegrees(float relX, float relZ);

}  // namespace web_demo
```

- [ ] **Step 2: Write `spatial_math_test.cpp` (the failing test)**

```cpp
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
```

- [ ] **Step 3: Add the test target to CMake and confirm it fails to build**

Rewrite `examples/web-demo/server/CMakeLists.txt`:

```cmake
# Demo web (vitrine) : ni code de production (src/, tools/) ni prototype
# d'exploration (benchmarks/) — voir docs/superpowers/specs/2026-08-06-web-audio-demo-design.md.

add_executable(web_demo_server main.cpp)
target_include_directories(web_demo_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(web_demo_server PRIVATE ws2_32)
endif()

add_executable(spatial_math_test spatial_math_test.cpp spatial_math.cpp)
target_include_directories(spatial_math_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
```

Run:

```bash
cmake --build build --target spatial_math_test
```

Expected: **FAIL** — `spatial_math.cpp` doesn't exist yet (linker/compiler error: no such file).

- [ ] **Step 4: Write `spatial_math.cpp` (minimal implementation)**

```cpp
#include "spatial_math.h"

#include <algorithm>
#include <cmath>

namespace web_demo {

float distanceGain(float distanceMeters, float referenceDistance, float maxDistance,
                    float rolloffFactor) {
    float d = std::clamp(distanceMeters, referenceDistance, maxDistance);
    return referenceDistance / (referenceDistance + rolloffFactor * (d - referenceDistance));
}

float distanceMeters(float relX, float relZ) {
    return std::sqrt(relX * relX + relZ * relZ);
}

float azimuthDegrees(float relX, float relZ) {
    constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
    return std::atan2(relX, -relZ) * kRadToDeg;
}

}  // namespace web_demo
```

- [ ] **Step 5: Build and run, verify it passes**

```bash
cmake --build build --target spatial_math_test
./build/Debug/spatial_math_test.exe
```

Expected: prints `spatial_math_test : OK`, exit code 0.

- [ ] **Step 6: Commit**

```bash
git add examples/web-demo/server/spatial_math.h examples/web-demo/server/spatial_math.cpp examples/web-demo/server/spatial_math_test.cpp examples/web-demo/server/CMakeLists.txt
git commit -m "feat(web-demo): add spatial_math (distance/gain/azimuth) with tests"
```

---

### Task 3: `frame_stats` — rolling per-frame timing stats (TDD)

**Files:**
- Create: `examples/web-demo/server/frame_stats.h`
- Create: `examples/web-demo/server/frame_stats.cpp`
- Create: `examples/web-demo/server/frame_stats_test.cpp`
- Modify: `examples/web-demo/server/CMakeLists.txt`

**Interfaces:**
- Produces: `web_demo::FrameStatsSnapshot{count,lastUs,meanUs,minUs,maxUs,p95Us}`, `web_demo::RollingFrameStats(size_t windowSize)` with `.addSample(double)` and `.snapshot() const -> FrameStatsSnapshot` — used by Task 5/6 (`main.cpp`) and consumed by Task 4 (`state_json.h` includes this header).

- [ ] **Step 1: Write `frame_stats.h`**

```cpp
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
```

- [ ] **Step 2: Write `frame_stats_test.cpp` (the failing test)**

```cpp
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
```

- [ ] **Step 3: Add the test target and confirm it fails to build**

Append to `examples/web-demo/server/CMakeLists.txt` (after the `spatial_math_test` block):

```cmake

add_executable(frame_stats_test frame_stats_test.cpp frame_stats.cpp)
target_include_directories(frame_stats_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
```

Run `cmake --build build --target frame_stats_test`. Expected: **FAIL** (`frame_stats.cpp` missing).

- [ ] **Step 4: Write `frame_stats.cpp`**

```cpp
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
```

- [ ] **Step 5: Build and run, verify it passes**

```bash
cmake --build build --target frame_stats_test
./build/Debug/frame_stats_test.exe
```

Expected: `frame_stats_test : OK`, exit code 0.

- [ ] **Step 6: Commit**

```bash
git add examples/web-demo/server/frame_stats.h examples/web-demo/server/frame_stats.cpp examples/web-demo/server/frame_stats_test.cpp examples/web-demo/server/CMakeLists.txt
git commit -m "feat(web-demo): add rolling frame-time stats with tests"
```

---

### Task 4: `state_json` — build/parse the API's JSON payloads (TDD)

**Files:**
- Create: `examples/web-demo/server/state_json.h`
- Create: `examples/web-demo/server/state_json.cpp`
- Create: `examples/web-demo/server/state_json_test.cpp`
- Modify: `examples/web-demo/server/CMakeLists.txt`
- Modify: `CMakeLists.txt` (root) — none needed, `nlohmann_json` is already `find_package`d at root.

**Interfaces:**
- Consumes: `web_demo::FrameStatsSnapshot` (Task 3).
- Produces: `web_demo::SourceState`, `web_demo::ProfileState`, `web_demo::HrtfState`, `web_demo::Mp3State`, `web_demo::DemoState`, `web_demo::PlayerRequest`, `web_demo::buildStateJson(const DemoState&) -> std::string`, `web_demo::buildProfilesJson(const std::vector<std::string>&, const std::string&) -> std::string`, `web_demo::parsePlayerBody(const std::string&, PlayerRequest&) -> bool`, `web_demo::parseProfileBody(const std::string&, std::string&) -> bool` — all used by Task 5/6 (`main.cpp`) to build API responses and parse requests, matching the exact JSON shape in the spec.

- [ ] **Step 1: Write `state_json.h`**

```cpp
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "frame_stats.h"

namespace web_demo {

struct SourceState {
    std::string id;
    std::string label;
    std::string asset;
    std::string format;  // "mp3" ou "wav"
    float posX = 0.0f;
    float posZ = 0.0f;
    float distanceM = 0.0f;
    float azimuthDeg = 0.0f;
    float gain = 0.0f;
};

struct ProfileState {
    std::string active;
    std::string defaultId;
    bool usedFallback = false;
    std::string fallbackReason;
};

struct HrtfState {
    std::string status;
    std::string renderer;
    std::string version;
    std::string vendor;
    int sampleRateHz = 0;
};

struct Mp3State {
    std::string path;
    std::size_t fileSizeBytes = 0;
    int sampleRateHz = 0;
    int channels = 0;
    double durationSec = 0.0;
    std::size_t totalSamples = 0;
};

struct DemoState {
    float playerX = 0.0f;
    float playerZ = 0.0f;
    ProfileState profile;
    HrtfState hrtf;
    std::vector<SourceState> sources;
    FrameStatsSnapshot frameStats;
    double frameBudgetMs = 15.0;
    double fpsReal = 0.0;
    std::size_t framesProcessed = 0;
    Mp3State mp3;
};

// Forme exacte documentee dans docs/superpowers/specs/2026-08-06-web-audio-demo-design.md
// (section "API HTTP", GET /api/state).
std::string buildStateJson(const DemoState& state);

// { "profiles": [...], "active": "..." }
std::string buildProfilesJson(const std::vector<std::string>& profiles, const std::string& activeId);

struct PlayerRequest {
    float x = 0.0f;
    float z = 0.0f;
};

// Retourne false si le corps n'est pas un JSON valide avec x et z numeriques.
bool parsePlayerBody(const std::string& body, PlayerRequest& out);

// Retourne false si le corps n'est pas un JSON valide avec un champ "id" string.
bool parseProfileBody(const std::string& body, std::string& outId);

}  // namespace web_demo
```

- [ ] **Step 2: Write `state_json_test.cpp` (the failing test)**

```cpp
#include "state_json.h"

#include <cassert>
#include <cstdio>

#include <nlohmann/json.hpp>

int main() {
    using namespace web_demo;
    using json = nlohmann::json;

    DemoState state;
    state.playerX = 1.5f;
    state.playerZ = -0.5f;
    state.profile = {"subject_003", "subject_003", false, ""};
    state.hrtf = {"Enabled", "OpenAL Soft", "1.1 ALSOFT 1.24.3", "OpenAL Community", 44100};
    state.sources.push_back({"avant", "Avant", "assets/test-audio.mp3", "mp3",
                              0.0f, -5.0f, 4.6f, -12.3f, 0.83f});
    state.frameStats = {120, 12.4, 14.1, 8.2, 61.5, 22.0};
    state.frameBudgetMs = 15.0;
    state.fpsReal = 66.2;
    state.framesProcessed = 48213;
    state.mp3 = {"assets/test-audio.mp3", 179837, 44100, 1, 4.08, 179928};

    json j = json::parse(buildStateJson(state));
    assert(j["player"]["x"].get<float>() == 1.5f);
    assert(j["player"]["z"].get<float>() == -0.5f);
    assert(j["profile"]["active"] == "subject_003");
    assert(j["profile"]["usedFallback"] == false);
    assert(j["hrtf"]["sampleRateHz"] == 44100);
    assert(j["sources"].size() == 1);
    assert(j["sources"][0]["id"] == "avant");
    assert(j["sources"][0]["format"] == "mp3");
    assert(j["metrics"]["framesProcessed"] == 48213);
    assert(j["metrics"]["sourcesSimultaneous"] == 1);
    assert(j["metrics"]["meetsRAud03"] == false);  // 1 source < 4
    assert(j["mp3"]["fileSizeBytes"] == 179837);
    assert(j["mp3"]["channels"] == 1);

    // 4 sources -> meetsRAud03 doit passer a true.
    state.sources.push_back({"droite", "Droite", "assets/test-audio2.wav", "wav", 5, 0, 5, 90, 0.2f});
    state.sources.push_back({"arriere", "Arriere", "assets/test-audio3.wav", "wav", 0, 5, 5, 180, 0.2f});
    state.sources.push_back({"gauche", "Gauche", "assets/test-audio4.wav", "wav", -5, 0, 5, -90, 0.2f});
    json j4 = json::parse(buildStateJson(state));
    assert(j4["metrics"]["sourcesSimultaneous"] == 4);
    assert(j4["metrics"]["meetsRAud03"] == true);

    json profilesJson = json::parse(buildProfilesJson({"subject_003", "subject_008"}, "subject_003"));
    assert(profilesJson["profiles"].size() == 2);
    assert(profilesJson["active"] == "subject_003");

    PlayerRequest pr;
    assert(parsePlayerBody(R"({"x":1.2,"z":-0.4})", pr));
    assert(pr.x == 1.2f && pr.z == -0.4f);
    assert(!parsePlayerBody(R"({"x":1.2})", pr));       // z manquant
    assert(!parsePlayerBody("not json", pr));            // JSON invalide
    assert(!parsePlayerBody(R"({"x":"a","z":1})", pr));  // x pas numerique

    std::string id;
    assert(parseProfileBody(R"({"id":"subject_040"})", id));
    assert(id == "subject_040");
    assert(!parseProfileBody(R"({})", id));               // id manquant
    assert(!parseProfileBody(R"({"id":42})", id));         // id pas une chaine

    std::printf("state_json_test : OK\n");
    return 0;
}
```

- [ ] **Step 3: Add the test target and confirm it fails to build**

Append to `examples/web-demo/server/CMakeLists.txt`:

```cmake

add_executable(state_json_test state_json_test.cpp state_json.cpp frame_stats.cpp)
target_include_directories(state_json_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(state_json_test PRIVATE nlohmann_json::nlohmann_json)
```

Run `cmake --build build --target state_json_test`. Expected: **FAIL** (`state_json.cpp` missing).

- [ ] **Step 4: Write `state_json.cpp`**

```cpp
#include "state_json.h"

#include <nlohmann/json.hpp>

namespace web_demo {

using json = nlohmann::json;

std::string buildStateJson(const DemoState& state) {
    json sourcesJson = json::array();
    for (const auto& s : state.sources) {
        sourcesJson.push_back({
            {"id", s.id},
            {"label", s.label},
            {"asset", s.asset},
            {"format", s.format},
            {"pos", {{"x", s.posX}, {"z", s.posZ}}},
            {"distanceM", s.distanceM},
            {"azimuthDeg", s.azimuthDeg},
            {"gain", s.gain},
        });
    }

    double cpuBudgetPercent = state.frameStats.meanUs > 0.0
        ? (state.frameStats.meanUs / (state.frameBudgetMs * 1000.0)) * 100.0
        : 0.0;

    json j = {
        {"player", {{"x", state.playerX}, {"z", state.playerZ}}},
        {"profile", {
            {"active", state.profile.active},
            {"default", state.profile.defaultId},
            {"usedFallback", state.profile.usedFallback},
            {"fallbackReason", state.profile.fallbackReason},
        }},
        {"hrtf", {
            {"status", state.hrtf.status},
            {"renderer", state.hrtf.renderer},
            {"version", state.hrtf.version},
            {"vendor", state.hrtf.vendor},
            {"sampleRateHz", state.hrtf.sampleRateHz},
        }},
        {"sources", sourcesJson},
        {"metrics", {
            {"framesProcessed", state.framesProcessed},
            {"frameTimeUs", {
                {"last", state.frameStats.lastUs},
                {"mean", state.frameStats.meanUs},
                {"min", state.frameStats.minUs},
                {"max", state.frameStats.maxUs},
                {"p95", state.frameStats.p95Us},
            }},
            {"frameBudgetMs", state.frameBudgetMs},
            {"fpsReal", state.fpsReal},
            {"cpuBudgetPercent", cpuBudgetPercent},
            {"sourcesSimultaneous", state.sources.size()},
            {"meetsRAud03", state.sources.size() >= 4},
        }},
        {"mp3", {
            {"path", state.mp3.path},
            {"fileSizeBytes", state.mp3.fileSizeBytes},
            {"sampleRateHz", state.mp3.sampleRateHz},
            {"channels", state.mp3.channels},
            {"durationSec", state.mp3.durationSec},
            {"totalSamples", state.mp3.totalSamples},
        }},
    };
    return j.dump();
}

std::string buildProfilesJson(const std::vector<std::string>& profiles, const std::string& activeId) {
    json j = {
        {"profiles", profiles},
        {"active", activeId},
    };
    return j.dump();
}

bool parsePlayerBody(const std::string& body, PlayerRequest& out) {
    json j;
    try {
        j = json::parse(body);
    } catch (const json::parse_error&) {
        return false;
    }
    if (!j.is_object() || !j.contains("x") || !j.contains("z")) return false;
    if (!j["x"].is_number() || !j["z"].is_number()) return false;
    out.x = j["x"].get<float>();
    out.z = j["z"].get<float>();
    return true;
}

bool parseProfileBody(const std::string& body, std::string& outId) {
    json j;
    try {
        j = json::parse(body);
    } catch (const json::parse_error&) {
        return false;
    }
    if (!j.is_object() || !j.contains("id") || !j["id"].is_string()) return false;
    outId = j["id"].get<std::string>();
    return true;
}

}  // namespace web_demo
```

- [ ] **Step 5: Build and run, verify it passes**

```bash
cmake --build build --target state_json_test
./build/Debug/state_json_test.exe
```

Expected: `state_json_test : OK`, exit code 0.

- [ ] **Step 6: Commit**

```bash
git add examples/web-demo/server/state_json.h examples/web-demo/server/state_json.cpp examples/web-demo/server/state_json_test.cpp examples/web-demo/server/CMakeLists.txt
git commit -m "feat(web-demo): add state_json build/parse with tests"
```

---

### Task 5: Audio engine bootstrap (no HTTP yet) — reuse `src/hrtf` + `benchmarks/src`

**Files:**
- Modify: `examples/web-demo/server/main.cpp` (replace the Task 1 placeholder body)
- Modify: `examples/web-demo/server/CMakeLists.txt`

**Interfaces:**
- Consumes: `nathan::hrtf::{HrtfProfile, discoverProfiles, HrtfContext, HrtfStatus, toString, applyProfile, openalHrtfDirectory, nathanConfigDirectory, resolveStartupProfile, StartupResolution}` (`src/hrtf/*.h`); `Mp3Audio, load_mp3, makeBufferFromMp3` (`benchmarks/src/mp3_loader.h`); `WavAudio, load_wav_mono16` (`benchmarks/src/wav_loader.h`); `web_demo::{distanceGain, distanceMeters, azimuthDegrees}` (Task 2); `web_demo::RollingFrameStats` (Task 3).
- Produces: this task has no library surface for later tasks to call — it's `main()` itself. Task 6 extends this same file (documented below) and depends on the `SourceDef`, `kSourceDefs`, `LiveSource`, `createLiveSources` names defined here.

- [ ] **Step 1: Link the reused libraries and add real sources to the build**

Rewrite `examples/web-demo/server/CMakeLists.txt`:

```cmake
# Demo web (vitrine) : ni code de production (src/, tools/) ni prototype
# d'exploration (benchmarks/) — voir docs/superpowers/specs/2026-08-06-web-audio-demo-design.md.

add_executable(web_demo_server
    main.cpp
    spatial_math.cpp
    frame_stats.cpp
    state_json.cpp
)
target_include_directories(web_demo_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(web_demo_server PRIVATE nathan_hrtf nathan_support nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(web_demo_server PRIVATE ws2_32)
endif()

add_executable(spatial_math_test spatial_math_test.cpp spatial_math.cpp)
target_include_directories(spatial_math_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(frame_stats_test frame_stats_test.cpp frame_stats.cpp)
target_include_directories(frame_stats_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(state_json_test state_json_test.cpp state_json.cpp frame_stats.cpp)
target_include_directories(state_json_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(state_json_test PRIVATE nlohmann_json::nlohmann_json)
```

Note: `web_demo_server` must include both `${CMAKE_SOURCE_DIR}/benchmarks` (for `benchmarks/src/mp3_loader.h` style includes) and `${CMAKE_SOURCE_DIR}/src` (for `hrtf/*.h` style includes) — both come transitively from `nathan_support`'s and `nathan_hrtf`'s `PUBLIC target_include_directories` (`benchmarks/CMakeLists.txt` sets `src` relative to `benchmarks/`, `src/CMakeLists.txt` sets `${CMAKE_CURRENT_SOURCE_DIR}` i.e. `src/`), so no extra include dirs are needed here — confirm this in Step 4 by successfully including `"hrtf/profile_catalog.h"` and `"mp3_loader.h"` (not `"src/mp3_loader.h"` — the include root is already `benchmarks/src/`, per `nathan_support`'s `target_include_directories(nathan_support PUBLIC src)` relative to `benchmarks/CMakeLists.txt`).

- [ ] **Step 2: Replace `examples/web-demo/server/main.cpp` with the engine bootstrap**

```cpp
// Démo web — serveur : voir docs/superpowers/specs/2026-08-06-web-audio-demo-design.md.
#include "httplib.h"

#include <AL/al.h>
#include <AL/alc.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "hrtf/active_profile.h"
#include "hrtf/config_paths.h"
#include "hrtf/hrtf_context.h"
#include "hrtf/openal_paths.h"
#include "hrtf/profile_catalog.h"
#include "hrtf/profile_settings.h"

#include "mp3_loader.h"
#include "wav_loader.h"

#include "frame_stats.h"
#include "spatial_math.h"
#include "state_json.h"

namespace {

using namespace nathan::hrtf;

constexpr auto kFrameDuration = std::chrono::milliseconds(15);  // ~66 FPS, meme budget que les benchmarks existants
constexpr float kReferenceDistance = 1.0f;
constexpr float kMaxDistance = 20.0f;
constexpr float kRolloffFactor = 1.0f;
constexpr std::size_t kNumSources = 4;
constexpr std::size_t kFrameStatsWindow = 300;  // ~5 s a 66 FPS

struct SourceDef {
    std::string id;
    std::string label;
    std::string assetPath;
    std::string format;  // "mp3" ou "wav"
    float worldX;
    float worldZ;
};

const std::array<SourceDef, kNumSources> kSourceDefs = {{
    {"avant", "Avant", "assets/test-audio.mp3", "mp3", 0.0f, -5.0f},
    {"droite", "Droite", "assets/test-audio2.wav", "wav", 5.0f, 0.0f},
    {"arriere", "Arriere", "assets/test-audio3.wav", "wav", 0.0f, 5.0f},
    {"gauche", "Gauche", "assets/test-audio4.wav", "wav", -5.0f, 0.0f},
}};

// buffer OpenAL + source OpenAL pour un des 4 sons ; helper local, meme
// pattern que makeBufferFromWav dans benchmarks/house_benchmark.cpp.
struct LiveSource {
    ALuint buffer = 0;
    ALuint source = 0;
};

ALuint makeBufferFromWav(const WavAudio& wav) {
    ALuint buffer = 0;
    alGenBuffers(1, &buffer);
    std::vector<int16_t> pcm(wav.samples.size());
    for (size_t i = 0; i < wav.samples.size(); ++i) {
        pcm[i] = static_cast<int16_t>(std::lround(wav.samples[i] * 32767.0f));
    }
    alBufferData(buffer, AL_FORMAT_MONO16, pcm.data(),
                 static_cast<ALsizei>(pcm.size() * sizeof(int16_t)),
                 static_cast<ALsizei>(wav.sampleRate));
    return buffer;
}

// Cree buffers+sources fraiches pour le contexte OpenAL courant. A appeler
// au demarrage et apres chaque bascule de profil (voir active_profile.h :
// applyProfile detruit le device/contexte precedent, donc tout ID cree
// avant est deja invalide et NE DOIT PAS etre libere explicitement).
std::array<LiveSource, kNumSources> createLiveSources(const Mp3Audio& mp3Cache,
                                                       const std::array<WavAudio, kNumSources - 1>& wavCache) {
    std::array<LiveSource, kNumSources> live{};

    live[0].buffer = makeBufferFromMp3(mp3Cache);
    for (std::size_t i = 0; i < wavCache.size(); ++i) {
        live[i + 1].buffer = makeBufferFromWav(wavCache[i]);
    }

    for (auto& l : live) {
        alGenSources(1, &l.source);
        alSourcei(l.source, AL_BUFFER, static_cast<ALint>(l.buffer));
        alSourcei(l.source, AL_LOOPING, AL_TRUE);
        alSourcef(l.source, AL_REFERENCE_DISTANCE, kReferenceDistance);
        alSourcef(l.source, AL_MAX_DISTANCE, kMaxDistance);
        alSourcef(l.source, AL_ROLLOFF_FACTOR, kRolloffFactor);
        alSourcePlay(l.source);
    }
    return live;
}

}  // namespace

int main() {
    namespace fs = std::filesystem;

    const fs::path hrtfAssetsDir = "assets/hrtf";
    std::vector<HrtfProfile> catalog;
    try {
        catalog = discoverProfiles(hrtfAssetsDir);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur de decouverte des profils HRTF : %s\n", e.what());
        return 1;
    }
    if (catalog.empty()) {
        std::fprintf(stderr, "Aucun profil .sofa trouve dans %s\n", hrtfAssetsDir.string().c_str());
        return 1;
    }

    StartupResolution startup;
    try {
        startup = resolveStartupProfile(catalog, nathanConfigDirectory() / "settings.json");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur de resolution du profil de demarrage : %s\n", e.what());
        return 1;
    }
    if (startup.usedFallback) {
        std::printf("Repli sur profil par defaut : %s (%s)\n", startup.profile.id.c_str(),
                    startup.fallbackReason.c_str());
    }

    HrtfContext context;
    try {
        applyProfile(startup.profile, openalHrtfDirectory(), context);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur d'activation du profil HRTF : %s\n", e.what());
        return 1;
    }
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

    Mp3Audio mp3Cache;
    std::array<WavAudio, kNumSources - 1> wavCache;
    try {
        mp3Cache = load_mp3(kSourceDefs[0].assetPath);
        for (std::size_t i = 0; i < wavCache.size(); ++i) {
            wavCache[i] = load_wav_mono16(kSourceDefs[i + 1].assetPath);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur de chargement audio : %s\n", e.what());
        return 1;
    }

    std::array<LiveSource, kNumSources> live = createLiveSources(mp3Cache, wavCache);

    std::printf("=== NATHAN - Demo web (profil %s, HRTF: %s) ===\n", startup.profile.id.c_str(),
                toString(context.status()).c_str());
    std::printf("Boucle audio demarree (pas encore d'API HTTP — voir Task 6).\n");
    std::fflush(stdout);

    web_demo::RollingFrameStats frameStats(kFrameStatsWindow);
    std::size_t framesProcessed = 0;

    // Position joueur temporaire : un cercle de rayon 3m, pour verifier a
    // l'oreille que le rendu spatial reagit bien, avant de la piloter par
    // HTTP (Task 6).
    auto sessionStart = std::chrono::steady_clock::now();

    for (int i = 0; i < 400; ++i) {  // ~6 s a 15 ms/frame, largement assez pour verifier a l'oreille
        auto frameStart = std::chrono::steady_clock::now();
        double elapsedSec = std::chrono::duration<double>(frameStart - sessionStart).count();

        auto measureStart = std::chrono::high_resolution_clock::now();

        float playerX = 3.0f * static_cast<float>(std::sin(elapsedSec));
        float playerZ = -3.0f * static_cast<float>(std::cos(elapsedSec));

        for (std::size_t i2 = 0; i2 < kNumSources; ++i2) {
            float relX = kSourceDefs[i2].worldX - playerX;
            float relZ = kSourceDefs[i2].worldZ - playerZ;
            alSource3f(live[i2].source, AL_POSITION, relX, 0.0f, relZ);
        }

        auto measureEnd = std::chrono::high_resolution_clock::now();
        double frameUs = std::chrono::duration<double, std::micro>(measureEnd - measureStart).count();
        frameStats.addSample(frameUs);
        ++framesProcessed;

        std::this_thread::sleep_until(frameStart + kFrameDuration);
    }

    std::printf("Frames traitees : %zu | Temps moyen : %.1f us\n", framesProcessed,
                frameStats.snapshot().meanUs);

    context.close();
    return 0;
}
```

- [ ] **Step 3: Build**

```bash
cmake --build build --target web_demo_server
```

Expected: builds cleanly. If an include path fails, check `benchmarks/CMakeLists.txt` (`target_include_directories(nathan_support PUBLIC src)`, so `benchmarks/src/mp3_loader.h` is included as `"mp3_loader.h"`) and `src/CMakeLists.txt` (`target_include_directories(nathan_hrtf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})`, so `src/hrtf/profile_catalog.h` is included as `"hrtf/profile_catalog.h"`) — both match what Step 2's `#include`s already use, so this should succeed without extra `target_include_directories` calls.

- [ ] **Step 4: Run and verify by ear**

From the repo root (the loaders use relative paths):

```bash
./build/Debug/web_demo_server.exe
```

Expected: prints the startup banner with a real profile id and `HRTF: Enabled`, then runs for about 6 seconds. With headphones on, the sound should audibly move in a smooth circle around you (this is the temporary hardcoded position, not yet driven by the browser). Then it prints the frame count and mean processing time (microseconds — should be well under the 15 000 µs budget) and exits cleanly.

- [ ] **Step 5: Commit**

```bash
git add examples/web-demo/server/main.cpp examples/web-demo/server/CMakeLists.txt
git commit -m "feat(web-demo): bootstrap real audio engine (profile + 4 sources, no HTTP yet)"
```

---

### Task 6: HTTP API — state, player, profile switch (real hot-swap)

**Files:**
- Create: `examples/web-demo/server/shared_state.h`
- Modify: `examples/web-demo/server/main.cpp`

**Interfaces:**
- Consumes: everything from Task 5 (`SourceDef`, `kSourceDefs`, `LiveSource`, `createLiveSources`), Task 4 (`state_json.h` types/functions), `nathan::hrtf::{HrtfProfile, applyProfile, ...}`.
- Produces: the running HTTP API described in the spec (`GET /api/profiles`, `GET /api/state`, `POST /api/player`, `POST /api/profile`) — this is the last server task; nothing downstream depends on new C++ names beyond this point.

- [ ] **Step 1: Write `shared_state.h`**

```cpp
#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>

namespace web_demo {

struct ProfileSwitchRequest {
    std::string requestedId;
    bool completed = false;
    bool success = false;
    std::string errorMessage;
};

// Etat partage entre le thread HTTP (httplib) et la boucle audio du thread
// principal. Le thread HTTP n'appelle JAMAIS OpenAL directement : il depose
// des demandes ici, la boucle principale les applique (voir
// docs/superpowers/specs/2026-08-06-web-audio-demo-design.md, "Boucle
// principale et threading").
struct SharedState {
    std::mutex mutex;
    std::condition_variable profileSwitchDone;

    // Ecrit par le thread HTTP, lu par la boucle principale.
    float requestedPlayerX = 0.0f;
    float requestedPlayerZ = 0.0f;
    std::optional<ProfileSwitchRequest> pendingProfileSwitch;

    // Ecrit par la boucle principale, lu par le thread HTTP.
    std::string latestStateJson;
    std::vector<std::string> catalogIds;   // immuable apres le demarrage
    std::string activeProfileId;

    bool shuttingDown = false;
};

}  // namespace web_demo
```

- [ ] **Step 2: Rewrite `examples/web-demo/server/main.cpp`'s `main()` to wire HTTP onto the loop from Task 5**

Keep every declaration above `int main()` from Task 5 unchanged (namespace block with `SourceDef`, `kSourceDefs`, `LiveSource`, `makeBufferFromWav`, `createLiveSources`), add these two includes near the top:

```cpp
#include <atomic>
#include <csignal>

#include "shared_state.h"
```

Then replace the body of `int main()` from the line `std::array<LiveSource, kNumSources> live = createLiveSources(mp3Cache, wavCache);` onward (i.e. keep catalog discovery, startup resolution, `applyProfile`, `alDistanceModel`, and audio loading exactly as in Task 5) with:

```cpp
    std::array<LiveSource, kNumSources> live = createLiveSources(mp3Cache, wavCache);

    std::error_code sizeErr;
    std::uintmax_t mp3FileSize = fs::file_size(kSourceDefs[0].assetPath, sizeErr);

    web_demo::SharedState shared;
    for (const auto& p : catalog) shared.catalogIds.push_back(p.id);
    shared.activeProfileId = startup.profile.id;

    web_demo::ProfileState profileState{startup.profile.id, startup.profile.id,
                                        startup.usedFallback, startup.fallbackReason};

    httplib::Server svr;
    svr.set_default_headers({{"Access-Control-Allow-Origin", "*"}});
    svr.Options(R"(/api/.*)", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
    });

    svr.Get("/api/profiles", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(shared.mutex);
        res.set_content(web_demo::buildProfilesJson(shared.catalogIds, shared.activeProfileId),
                        "application/json");
    });

    svr.Get("/api/state", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(shared.mutex);
        res.set_content(shared.latestStateJson, "application/json");
    });

    svr.Post("/api/player", [&](const httplib::Request& req, httplib::Response& res) {
        web_demo::PlayerRequest pr;
        if (!web_demo::parsePlayerBody(req.body, pr)) {
            res.status = 400;
            res.set_content(R"({"error":"corps invalide, attendu {\"x\":number,\"z\":number}"})",
                            "application/json");
            return;
        }
        std::lock_guard<std::mutex> lock(shared.mutex);
        shared.requestedPlayerX = std::clamp(pr.x, -10.0f, 10.0f);
        shared.requestedPlayerZ = std::clamp(pr.z, -10.0f, 10.0f);
        res.status = 204;
    });

    svr.Post("/api/profile", [&](const httplib::Request& req, httplib::Response& res) {
        std::string id;
        if (!web_demo::parseProfileBody(req.body, id)) {
            res.status = 400;
            res.set_content(R"({"error":"corps invalide, attendu {\"id\":string}"})", "application/json");
            return;
        }

        bool found = false;
        for (const auto& p : catalog) found = found || (p.id == id);
        if (!found) {
            res.status = 404;
            res.set_content(nlohmann::json{{"error", "Profil '" + id + "' introuvable dans le catalogue."}}.dump(),
                            "application/json");
            return;
        }

        std::unique_lock<std::mutex> lock(shared.mutex);
        if (shared.pendingProfileSwitch.has_value() && !shared.pendingProfileSwitch->completed) {
            res.status = 409;
            res.set_content(R"({"error":"un changement de profil est deja en cours"})", "application/json");
            return;
        }
        shared.pendingProfileSwitch = web_demo::ProfileSwitchRequest{id, false, false, ""};
        shared.profileSwitchDone.wait(lock, [&] { return shared.pendingProfileSwitch->completed; });

        if (!shared.pendingProfileSwitch->success) {
            res.status = 500;
            res.set_content(nlohmann::json{{"error", shared.pendingProfileSwitch->errorMessage}}.dump(),
                            "application/json");
            return;
        }
        res.set_content(shared.latestStateJson, "application/json");
    });

    std::atomic<bool> running{true};
    std::thread httpThread([&] { svr.listen("127.0.0.1", 8787); });

    std::signal(SIGINT, [](int) { std::exit(0); });  // Ctrl+C : sortie simple, cf. plan Task 6 note

    std::printf("=== NATHAN - Demo web (profil %s, HRTF: %s) ===\n", startup.profile.id.c_str(),
                toString(context.status()).c_str());
    std::printf("API HTTP : http://127.0.0.1:8787 (Ctrl+C pour quitter)\n");
    std::fflush(stdout);

    web_demo::RollingFrameStats frameStats(kFrameStatsWindow);
    std::size_t framesProcessed = 0;
    double emaFps = 1000.0 / static_cast<double>(kFrameDuration.count());
    auto frameStartPrev = std::chrono::steady_clock::now();

    while (running) {
        auto frameStart = std::chrono::steady_clock::now();
        double periodSec = std::chrono::duration<double>(frameStart - frameStartPrev).count();
        frameStartPrev = frameStart;
        if (periodSec > 0.0) emaFps = emaFps * 0.9 + (1.0 / periodSec) * 0.1;

        float playerX, playerZ;
        std::optional<std::string> switchId;
        {
            std::lock_guard<std::mutex> lock(shared.mutex);
            playerX = shared.requestedPlayerX;
            playerZ = shared.requestedPlayerZ;
            if (shared.pendingProfileSwitch.has_value() && !shared.pendingProfileSwitch->completed) {
                switchId = shared.pendingProfileSwitch->requestedId;
            }
        }

        if (switchId.has_value()) {
            bool success = true;
            std::string errorMessage;
            const HrtfProfile* target = nullptr;
            for (const auto& p : catalog) {
                if (p.id == *switchId) target = &p;
            }
            try {
                // ATTENTION (active_profile.h) : applyProfile detruit le
                // device/contexte precedent — `live` devient invalide des
                // cet appel, on ne doit jamais appeler alDelete* dessus.
                applyProfile(*target, openalHrtfDirectory(), context);
                alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
                live = createLiveSources(mp3Cache, wavCache);
                profileState = {*switchId, startup.profile.id, false, ""};
            } catch (const std::exception& e) {
                success = false;
                errorMessage = e.what();
            }

            std::lock_guard<std::mutex> lock(shared.mutex);
            shared.pendingProfileSwitch->success = success;
            shared.pendingProfileSwitch->errorMessage = errorMessage;
            shared.pendingProfileSwitch->completed = true;
            if (success) shared.activeProfileId = *switchId;
            shared.profileSwitchDone.notify_all();
        }

        auto measureStart = std::chrono::high_resolution_clock::now();

        web_demo::DemoState state;
        state.playerX = playerX;
        state.playerZ = playerZ;
        state.profile = profileState;
        state.hrtf = {toString(context.status()), std::string(alGetString(AL_RENDERER)),
                      std::string(alGetString(AL_VERSION)), std::string(alGetString(AL_VENDOR)), 0};
        ALCint freq = 0;
        alcGetIntegerv(context.device(), ALC_FREQUENCY, 1, &freq);
        state.hrtf.sampleRateHz = freq;

        for (std::size_t i = 0; i < kNumSources; ++i) {
            float relX = kSourceDefs[i].worldX - playerX;
            float relZ = kSourceDefs[i].worldZ - playerZ;
            alSource3f(live[i].source, AL_POSITION, relX, 0.0f, relZ);

            web_demo::SourceState s;
            s.id = kSourceDefs[i].id;
            s.label = kSourceDefs[i].label;
            s.asset = kSourceDefs[i].assetPath;
            s.format = kSourceDefs[i].format;
            s.posX = kSourceDefs[i].worldX;
            s.posZ = kSourceDefs[i].worldZ;
            s.distanceM = web_demo::distanceMeters(relX, relZ);
            s.azimuthDeg = web_demo::azimuthDegrees(relX, relZ);
            s.gain = web_demo::distanceGain(s.distanceM, kReferenceDistance, kMaxDistance, kRolloffFactor);
            state.sources.push_back(s);
        }

        auto measureEnd = std::chrono::high_resolution_clock::now();
        double frameUs = std::chrono::duration<double, std::micro>(measureEnd - measureStart).count();
        frameStats.addSample(frameUs);
        ++framesProcessed;

        state.frameStats = frameStats.snapshot();
        state.frameBudgetMs = std::chrono::duration<double, std::milli>(kFrameDuration).count();
        state.fpsReal = emaFps;
        state.framesProcessed = framesProcessed;
        state.mp3 = {kSourceDefs[0].assetPath, static_cast<std::size_t>(mp3FileSize), mp3Cache.sampleRate,
                    static_cast<int>(mp3Cache.channels),
                    static_cast<double>(mp3Cache.samples.size() / mp3Cache.channels) / mp3Cache.sampleRate,
                    mp3Cache.samples.size() / mp3Cache.channels};

        {
            std::lock_guard<std::mutex> lock(shared.mutex);
            shared.latestStateJson = web_demo::buildStateJson(state);
        }

        std::this_thread::sleep_until(frameStart + kFrameDuration);
    }

    svr.stop();
    httpThread.join();
    context.close();
    return 0;
```

Add `#include <nlohmann/json.hpp>` and `#include <algorithm>` (for `std::clamp`) near the top of `main.cpp` alongside the other includes.

Note on shutdown: the `std::signal(SIGINT, ...)` handler above calls `std::exit(0)` directly rather than coordinating a clean `svr.stop()`/thread join — this is a deliberate, documented simplification for a local demo tool (not perfectly signal-safe, but the same pragmatic level of rigor as the rest of this codebase's prototypes). OS-level cleanup reclaims the socket and audio device on process exit. If a cleaner shutdown is wanted later, replace the handler with one that only sets `running = false` and have the main loop call `svr.stop()` once it exits, but that is out of scope for today's demo.

- [ ] **Step 3: Build**

```bash
cmake --build build --target web_demo_server
```

Expected: builds cleanly.

- [ ] **Step 4: Verify via curl (server running in another terminal)**

```bash
curl -s http://127.0.0.1:8787/api/profiles
curl -s http://127.0.0.1:8787/api/state
curl -s -X POST http://127.0.0.1:8787/api/player -H "Content-Type: application/json" -d '{"x":2,"z":-1}'
curl -s http://127.0.0.1:8787/api/state   # confirm "player":{"x":2,"z":-1} and updated distance/azimuth/gain per source
curl -s -X POST http://127.0.0.1:8787/api/profile -H "Content-Type: application/json" -d '{"id":"subject_008"}'
curl -s http://127.0.0.1:8787/api/state   # confirm "profile":{"active":"subject_008",...}
curl -s -X POST http://127.0.0.1:8787/api/profile -H "Content-Type: application/json" -d '{"id":"does_not_exist"}'
```

Expected: valid JSON matching the spec's shape at each step; the last call returns `404` with an `error` message. With headphones on, confirm by ear: the `POST /api/player` call should audibly move the sound, and the `POST /api/profile` call should audibly change the HRTF rendering (no crash, sound resumes).

- [ ] **Step 5: Commit**

```bash
git add examples/web-demo/server/shared_state.h examples/web-demo/server/main.cpp
git commit -m "feat(web-demo): serve live state + player/profile control over HTTP"
```

---

### Task 7: Client scaffold (Vite + TypeScript)

**Files:**
- Create: `examples/web-demo/client/package.json`
- Create: `examples/web-demo/client/tsconfig.json`
- Create: `examples/web-demo/client/index.html`
- Create: `examples/web-demo/client/.env.example`
- Create: `examples/web-demo/client/src/style.css` (base tokens only — full layout comes in Task 12)
- Create: `examples/web-demo/client/src/main.ts` (placeholder)
- Create: `examples/web-demo/client/src/vite-env.d.ts`

**Interfaces:**
- Produces: a Vite dev server serving a placeholder page — later tasks add real modules under `src/`.

- [ ] **Step 1: Write `package.json`**

```json
{
  "name": "nathan-audio-web-demo",
  "private": true,
  "version": "0.0.0",
  "description": "Demo web du pipeline audio HRTF NATHAN — client d'affichage du moteur C++.",
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "vite build",
    "typecheck": "tsc --noEmit"
  },
  "devDependencies": {
    "typescript": "^5.6.0",
    "vite": "^8.2.0"
  }
}
```

- [ ] **Step 2: Write `tsconfig.json`**

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "ESNext",
    "moduleResolution": "Bundler",
    "strict": true,
    "noUnusedLocals": true,
    "noUnusedParameters": true,
    "skipLibCheck": true,
    "types": ["vite/client"]
  },
  "include": ["src"]
}
```

- [ ] **Step 3: Write `src/vite-env.d.ts`**

```ts
/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly VITE_SERVER_URL?: string;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}
```

- [ ] **Step 4: Write `.env.example`**

```
# URL du serveur web_demo_server (voir examples/web-demo/server/). Copier en .env pour surcharger.
VITE_SERVER_URL=http://127.0.0.1:8787
```

- [ ] **Step 5: Write `src/style.css` (base tokens)**

```css
/*
 * Token system — palette sombre technique propre a cette demo (pas celle de
 * la demo agent-core, cf. docs/superpowers/specs/2026-08-06-web-audio-demo-design.md).
 */
:root {
  --ink: #0f1216;
  --ink-raised: #171b21;
  --paper: #e7e9ee;
  --accent: #4fd1c5;
  --accent-strong: #7ee8db;
  --mute: #8891a3;
  --alert: #ff6b6b;
}

* { box-sizing: border-box; }

html, body { height: 100%; }

body {
  margin: 0;
  background: var(--ink);
  color: var(--paper);
  font: 16px/1.5 system-ui, -apple-system, "Segoe UI", sans-serif;
}
```

- [ ] **Step 6: Write placeholder `src/main.ts`**

```ts
document.querySelector<HTMLDivElement>("#app")!.innerHTML = `
  <p style="padding: 2rem; font-family: ui-monospace, monospace;">
    demo web NATHAN-audio — squelette (Task 7). Suite dans Task 8+.
  </p>
`;
```

- [ ] **Step 7: Write `index.html`**

```html
<!doctype html>
<html lang="fr">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>NATHAN-audio — démo HRTF</title>
    <link rel="stylesheet" href="/src/style.css" />
  </head>
  <body>
    <div id="app"></div>
    <script type="module" src="/src/main.ts"></script>
  </body>
</html>
```

- [ ] **Step 8: Install and run**

```bash
cd examples/web-demo/client
npm install
npm run dev
```

Expected: Vite prints a local URL (e.g. `http://localhost:5173`); opening it shows the placeholder text. Stop with Ctrl+C.

- [ ] **Step 9: Commit**

```bash
cd ../..
git add examples/web-demo/client/package.json examples/web-demo/client/package-lock.json examples/web-demo/client/tsconfig.json examples/web-demo/client/index.html examples/web-demo/client/.env.example examples/web-demo/client/src/style.css examples/web-demo/client/src/main.ts examples/web-demo/client/src/vite-env.d.ts
git commit -m "feat(web-demo): scaffold Vite/TS client"
```

---

### Task 8: `api.ts` — typed client + polling loop

**Files:**
- Create: `examples/web-demo/client/src/api.ts`

**Interfaces:**
- Consumes: the HTTP API from Task 6 (exact JSON shapes).
- Produces: TypeScript types `SourceState`, `DemoState`, `ProfilesResponse`; functions `fetchState()`, `fetchProfiles()`, `postPlayer(x, z)`, `postProfile(id)`, `startPolling(onState, onUnreachable, intervalMs?)` — used by Task 9 (scene), Task 10 (metrics/mp3 panel), Task 11 (profile selector), Task 12 (main.ts wiring).

- [ ] **Step 1: Write `src/api.ts`**

```ts
export interface SourceState {
  id: string;
  label: string;
  asset: string;
  format: "mp3" | "wav";
  pos: { x: number; z: number };
  distanceM: number;
  azimuthDeg: number;
  gain: number;
}

export interface DemoState {
  player: { x: number; z: number };
  profile: { active: string; default: string; usedFallback: boolean; fallbackReason: string };
  hrtf: { status: string; renderer: string; version: string; vendor: string; sampleRateHz: number };
  sources: SourceState[];
  metrics: {
    framesProcessed: number;
    frameTimeUs: { last: number; mean: number; min: number; max: number; p95: number };
    frameBudgetMs: number;
    fpsReal: number;
    cpuBudgetPercent: number;
    sourcesSimultaneous: number;
    meetsRAud03: boolean;
  };
  mp3: {
    path: string;
    fileSizeBytes: number;
    sampleRateHz: number;
    channels: number;
    durationSec: number;
    totalSamples: number;
  };
}

export interface ProfilesResponse {
  profiles: string[];
  active: string;
}

export interface ProfileSwitchError {
  ok: false;
  error: string;
}

export interface ProfileSwitchOk {
  ok: true;
  state: DemoState;
}

const SERVER_URL = import.meta.env.VITE_SERVER_URL ?? "http://127.0.0.1:8787";

export async function fetchState(): Promise<DemoState> {
  const res = await fetch(`${SERVER_URL}/api/state`);
  if (!res.ok) throw new Error(`GET /api/state -> ${res.status}`);
  return (await res.json()) as DemoState;
}

export async function fetchProfiles(): Promise<ProfilesResponse> {
  const res = await fetch(`${SERVER_URL}/api/profiles`);
  if (!res.ok) throw new Error(`GET /api/profiles -> ${res.status}`);
  return (await res.json()) as ProfilesResponse;
}

export async function postPlayer(x: number, z: number): Promise<void> {
  await fetch(`${SERVER_URL}/api/player`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ x, z }),
  });
  // Fire-and-forget par conception (cf. spec) : on ignore la reponse, la
  // prochaine boucle de polling /api/state refletera la nouvelle position.
}

export async function postProfile(id: string): Promise<ProfileSwitchOk | ProfileSwitchError> {
  const res = await fetch(`${SERVER_URL}/api/profile`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ id }),
  });
  const body = await res.json();
  if (!res.ok) return { ok: false, error: (body as { error: string }).error };
  return { ok: true, state: body as DemoState };
}

// Interroge /api/state en boucle. onState recoit chaque snapshot reussi ;
// onReachability(false) est appele des la premiere erreur reseau (serveur
// injoignable), onReachability(true) des que ca redevient joignable.
// Retourne une fonction d'arret.
export function startPolling(
  onState: (state: DemoState) => void,
  onReachability: (reachable: boolean) => void,
  intervalMs = 60,
): () => void {
  let stopped = false;
  let wasReachable = true;

  async function tick() {
    if (stopped) return;
    try {
      const state = await fetchState();
      if (!wasReachable) {
        wasReachable = true;
        onReachability(true);
      }
      onState(state);
    } catch {
      if (wasReachable) {
        wasReachable = false;
        onReachability(false);
      }
    } finally {
      if (!stopped) setTimeout(tick, intervalMs);
    }
  }

  tick();
  return () => {
    stopped = true;
  };
}
```

- [ ] **Step 2: Verify against the running backend**

Temporarily append to `src/main.ts` (will be replaced in Task 12):

```ts
import { startPolling } from "./api";
startPolling(
  (state) => console.log("state", state),
  (reachable) => console.log("reachable?", reachable),
);
```

Run `.\build\Debug\web_demo_server.exe` in one terminal, `npm run dev` (in `examples/web-demo/client/`) in another, open the printed URL, open the browser devtools console.

Expected: `reachable? true` once, then a `state` log roughly every 60 ms with real numbers matching what `curl http://127.0.0.1:8787/api/state` returns. Stop the server process and confirm a `reachable? false` log appears within ~1 polling interval.

- [ ] **Step 3: Commit**

```bash
git add examples/web-demo/client/src/api.ts examples/web-demo/client/src/main.ts
git commit -m "feat(web-demo): typed API client with polling loop"
```

---

### Task 9: `scene.ts` — SVG scene (player, sprites, vectors, drag)

**Files:**
- Create: `examples/web-demo/client/src/scene.ts`

**Interfaces:**
- Consumes: `DemoState`, `SourceState`, `postPlayer` from `api.ts` (Task 8).
- Produces: `initScene(container: HTMLElement): { render(state: DemoState): void }` — used by Task 12 (`main.ts`).

- [ ] **Step 1: Write `src/scene.ts`**

```ts
import { postPlayer, type DemoState, type SourceState } from "./api";

const VIEW_MIN = -12;
const VIEW_MAX = 12;
const VIEW_SIZE = VIEW_MAX - VIEW_MIN;
const DRAG_POST_THROTTLE_MS = 50;

// Convention (docs/superpowers/specs/2026-08-06-web-audio-demo-design.md) :
// haut ecran = avant (-Z), droite ecran = droite (+X). Le SVG a Y vers le
// bas, donc screenY = -worldZ (inverse de Z) pour que "avant" reste en haut.
function worldToScreen(x: number, z: number): { sx: number; sy: number } {
  return { sx: x, sy: -z };
}

function svgEl<K extends keyof SVGElementTagNameMap>(tag: K): SVGElementTagNameMap[K] {
  return document.createElementNS("http://www.w3.org/2000/svg", tag);
}

export function initScene(container: HTMLElement): { render(state: DemoState): void } {
  const svg = svgEl("svg");
  svg.setAttribute("viewBox", `${VIEW_MIN} ${VIEW_MIN} ${VIEW_SIZE} ${VIEW_SIZE}`);
  svg.setAttribute("class", "scene-svg");
  svg.setAttribute("role", "img");
  svg.setAttribute("aria-label", "Scène spatiale : joueur, 4 sources sonores et vecteurs de position relative");

  const gridGroup = svgEl("g");
  gridGroup.setAttribute("class", "scene-grid");
  for (let i = VIEW_MIN; i <= VIEW_MAX; i++) {
    const isMajor = i % 5 === 0;
    const vLine = svgEl("line");
    vLine.setAttribute("x1", String(i));
    vLine.setAttribute("x2", String(i));
    vLine.setAttribute("y1", String(VIEW_MIN));
    vLine.setAttribute("y2", String(VIEW_MAX));
    vLine.setAttribute("class", isMajor ? "grid-line grid-line--major" : "grid-line");
    gridGroup.appendChild(vLine);

    const hLine = svgEl("line");
    hLine.setAttribute("x1", String(VIEW_MIN));
    hLine.setAttribute("x2", String(VIEW_MAX));
    hLine.setAttribute("y1", String(i));
    hLine.setAttribute("y2", String(i));
    hLine.setAttribute("class", isMajor ? "grid-line grid-line--major" : "grid-line");
    gridGroup.appendChild(hLine);
  }
  svg.appendChild(gridGroup);

  const vectorsGroup = svgEl("g");
  vectorsGroup.setAttribute("class", "scene-vectors");
  svg.appendChild(vectorsGroup);

  const spritesGroup = svgEl("g");
  spritesGroup.setAttribute("class", "scene-sprites");
  svg.appendChild(spritesGroup);

  const playerCircle = svgEl("circle");
  playerCircle.setAttribute("class", "scene-player");
  playerCircle.setAttribute("r", "0.5");
  playerCircle.setAttribute("tabindex", "0");
  playerCircle.setAttribute("role", "slider");
  playerCircle.setAttribute("aria-label", "Joueur — déplacer à la souris");
  svg.appendChild(playerCircle);

  container.appendChild(svg);

  let latestSources: SourceState[] = [];
  let dragging = false;
  let lastPostAt = 0;

  function clientToWorld(clientX: number, clientY: number): { x: number; z: number } {
    const rect = svg.getBoundingClientRect();
    const sx = VIEW_MIN + ((clientX - rect.left) / rect.width) * VIEW_SIZE;
    const sy = VIEW_MIN + ((clientY - rect.top) / rect.height) * VIEW_SIZE;
    return { x: sx, z: -sy };
  }

  function onPointerMove(ev: PointerEvent) {
    if (!dragging) return;
    const { x, z } = clientToWorld(ev.clientX, ev.clientY);
    const clampedX = Math.max(-10, Math.min(10, x));
    const clampedZ = Math.max(-10, Math.min(10, z));
    const { sx, sy } = worldToScreen(clampedX, clampedZ);
    playerCircle.setAttribute("cx", String(sx));
    playerCircle.setAttribute("cy", String(sy));

    const now = performance.now();
    if (now - lastPostAt >= DRAG_POST_THROTTLE_MS) {
      lastPostAt = now;
      void postPlayer(clampedX, clampedZ);
    }
  }

  playerCircle.addEventListener("pointerdown", (ev) => {
    dragging = true;
    playerCircle.setPointerCapture(ev.pointerId);
  });
  playerCircle.addEventListener("pointerup", (ev) => {
    dragging = false;
    playerCircle.releasePointerCapture(ev.pointerId);
  });
  svg.addEventListener("pointermove", onPointerMove);

  function render(state: DemoState) {
    latestSources = state.sources;

    if (!dragging) {
      const { sx, sy } = worldToScreen(state.player.x, state.player.z);
      playerCircle.setAttribute("cx", String(sx));
      playerCircle.setAttribute("cy", String(sy));
    }

    while (spritesGroup.firstChild) spritesGroup.removeChild(spritesGroup.firstChild);
    while (vectorsGroup.firstChild) vectorsGroup.removeChild(vectorsGroup.firstChild);

    const playerScreen = worldToScreen(state.player.x, state.player.z);

    for (const source of latestSources) {
      const { sx, sy } = worldToScreen(source.pos.x, source.pos.z);

      const vector = svgEl("line");
      vector.setAttribute("x1", String(playerScreen.sx));
      vector.setAttribute("y1", String(playerScreen.sy));
      vector.setAttribute("x2", String(sx));
      vector.setAttribute("y2", String(sy));
      vector.setAttribute("class", "scene-vector");
      vector.setAttribute("stroke-opacity", String(Math.max(0.25, source.gain)));
      vectorsGroup.appendChild(vector);

      const label = svgEl("text");
      label.setAttribute("x", String((playerScreen.sx + sx) / 2));
      label.setAttribute("y", String((playerScreen.sy + sy) / 2));
      label.setAttribute("class", "scene-vector-label");
      label.textContent = `${source.distanceM.toFixed(1)} m · ${source.azimuthDeg.toFixed(0)}°`;
      vectorsGroup.appendChild(label);

      const sprite = svgEl("circle");
      sprite.setAttribute("cx", String(sx));
      sprite.setAttribute("cy", String(sy));
      sprite.setAttribute("r", "0.4");
      sprite.setAttribute("class", "scene-sprite");
      spritesGroup.appendChild(sprite);

      const spriteLabel = svgEl("text");
      spriteLabel.setAttribute("x", String(sx));
      spriteLabel.setAttribute("y", String(sy - 0.6));
      spriteLabel.setAttribute("class", "scene-sprite-label");
      spriteLabel.textContent = `${source.label} (${source.format})`;
      spritesGroup.appendChild(spriteLabel);
    }
  }

  return { render };
}
```

- [ ] **Step 2: Wire it temporarily to verify visually**

Temporarily replace `src/main.ts` (still placeholder — Task 12 makes this permanent) with:

```ts
import { startPolling } from "./api";
import { initScene } from "./scene";

const app = document.querySelector<HTMLDivElement>("#app")!;
const sceneContainer = document.createElement("div");
sceneContainer.style.width = "600px";
sceneContainer.style.height = "600px";
app.appendChild(sceneContainer);

const scene = initScene(sceneContainer);
startPolling(
  (state) => scene.render(state),
  () => {},
);
```

Add minimal SVG sizing to `src/style.css`:

```css
.scene-svg { width: 100%; height: 100%; background: var(--ink-raised); }
.grid-line { stroke: color-mix(in srgb, var(--paper) 8%, transparent); stroke-width: 0.02; }
.grid-line--major { stroke: color-mix(in srgb, var(--paper) 16%, transparent); }
.scene-player { fill: var(--accent-strong); cursor: grab; }
.scene-sprite { fill: var(--accent); }
.scene-vector { stroke: var(--mute); stroke-width: 0.05; }
.scene-vector-label, .scene-sprite-label { fill: var(--paper); font-size: 0.4px; font-family: ui-monospace, monospace; text-anchor: middle; }
```

Run both processes (server + `npm run dev`), open the browser.

Expected: a grid with 4 dots (sprites, labeled) at the cardinal positions, one bright dot (player) at the center, thin lines with distance/azimuth labels between them. Dragging the player dot moves it and — check the server terminal or `curl /api/state` — updates `player` server-side, and the vectors/labels update live to match.

- [ ] **Step 3: Commit**

```bash
git add examples/web-demo/client/src/scene.ts examples/web-demo/client/src/main.ts examples/web-demo/client/src/style.css
git commit -m "feat(web-demo): SVG scene with draggable player and live vectors"
```

---

### Task 10: `metrics.ts` + `mp3panel.ts` — read-only panels

**Files:**
- Create: `examples/web-demo/client/src/metrics.ts`
- Create: `examples/web-demo/client/src/mp3panel.ts`

**Interfaces:**
- Consumes: `DemoState` (Task 8).
- Produces: `initMetrics(container: HTMLElement): { render(state: DemoState): void }`, `initMp3Panel(container: HTMLElement): { render(state: DemoState): void }` — used by Task 12 (`main.ts`).

- [ ] **Step 1: Write `src/metrics.ts`**

```ts
import type { DemoState } from "./api";

// Flash bref sur toute valeur qui change — meme principe que le flash
// .active de la demo agent-core, pour rester lisible a l'enregistrement
// sans voix off.
function setTextWithFlash(el: HTMLElement, text: string) {
  if (el.textContent === text) return;
  el.textContent = text;
  el.classList.remove("metric-flash");
  // force reflow pour rejouer l'animation meme si elle vient de tourner
  void el.offsetWidth;
  el.classList.add("metric-flash");
}

export function initMetrics(container: HTMLElement): { render(state: DemoState): void } {
  container.innerHTML = `
    <section aria-label="Métriques globales">
      <h2>Moteur audio</h2>
      <dl class="metric-list">
        <dt>Profil actif</dt><dd data-field="profile"></dd>
        <dt>Statut HRTF</dt><dd data-field="hrtfStatus"></dd>
        <dt>Renderer</dt><dd data-field="renderer"></dd>
        <dt>Fréquence</dt><dd data-field="sampleRate"></dd>
        <dt>Temps de traitement (moyenne / p95)</dt><dd data-field="frameTime"></dd>
        <dt>Budget par frame</dt><dd data-field="budget"></dd>
        <dt>FPS réel</dt><dd data-field="fps"></dd>
        <dt>Sources simultanées</dt><dd data-field="sources"></dd>
      </dl>
    </section>
    <section aria-label="Métriques par source">
      <h2>Sources</h2>
      <table class="metric-table">
        <thead><tr><th>Source</th><th>Distance</th><th>Azimut</th><th>Gain</th></tr></thead>
        <tbody data-field="sourceRows"></tbody>
      </table>
    </section>
  `;

  const fields = {
    profile: container.querySelector<HTMLElement>('[data-field="profile"]')!,
    hrtfStatus: container.querySelector<HTMLElement>('[data-field="hrtfStatus"]')!,
    renderer: container.querySelector<HTMLElement>('[data-field="renderer"]')!,
    sampleRate: container.querySelector<HTMLElement>('[data-field="sampleRate"]')!,
    frameTime: container.querySelector<HTMLElement>('[data-field="frameTime"]')!,
    budget: container.querySelector<HTMLElement>('[data-field="budget"]')!,
    fps: container.querySelector<HTMLElement>('[data-field="fps"]')!,
    sources: container.querySelector<HTMLElement>('[data-field="sources"]')!,
    sourceRows: container.querySelector<HTMLElement>('[data-field="sourceRows"]')!,
  };

  function render(state: DemoState) {
    setTextWithFlash(
      fields.profile,
      state.profile.usedFallback
        ? `${state.profile.active} (repli : ${state.profile.fallbackReason})`
        : state.profile.active,
    );
    setTextWithFlash(fields.hrtfStatus, state.hrtf.status);
    setTextWithFlash(fields.renderer, `${state.hrtf.renderer} ${state.hrtf.version}`);
    setTextWithFlash(fields.sampleRate, `${state.hrtf.sampleRateHz} Hz`);
    setTextWithFlash(
      fields.frameTime,
      `${state.metrics.frameTimeUs.mean.toFixed(1)} µs / ${state.metrics.frameTimeUs.p95.toFixed(1)} µs`,
    );
    setTextWithFlash(
      fields.budget,
      `${state.metrics.frameBudgetMs.toFixed(1)} ms (${state.metrics.cpuBudgetPercent.toFixed(2)} % utilisé)`,
    );
    setTextWithFlash(fields.fps, state.metrics.fpsReal.toFixed(1));
    setTextWithFlash(
      fields.sources,
      `${state.metrics.sourcesSimultaneous} — R-AUD-03 ${state.metrics.meetsRAud03 ? "OK" : "NON"}`,
    );

    fields.sourceRows.innerHTML = state.sources
      .map(
        (s) => `
          <tr>
            <td>${s.label}</td>
            <td>${s.distanceM.toFixed(1)} m</td>
            <td>${s.azimuthDeg.toFixed(0)}°</td>
            <td>${s.gain.toFixed(2)} (${(20 * Math.log10(Math.max(s.gain, 1e-4))).toFixed(1)} dB)</td>
          </tr>`,
      )
      .join("");
  }

  return { render };
}
```

- [ ] **Step 2: Write `src/mp3panel.ts`**

```ts
import type { DemoState } from "./api";

export function initMp3Panel(container: HTMLElement): { render(state: DemoState): void } {
  container.innerHTML = `
    <section aria-label="Décodage MP3">
      <h2>Décodage MP3</h2>
      <p class="mp3-note">
        Valeurs réelles décodées par <code>dr_mp3</code>
        (<code>benchmarks/src/mp3_loader.cpp</code>), pas une estimation.
      </p>
      <dl class="metric-list">
        <dt>Fichier</dt><dd data-field="path"></dd>
        <dt>Taille</dt><dd data-field="size"></dd>
        <dt>Fréquence décodée</dt><dd data-field="rate"></dd>
        <dt>Canaux</dt><dd data-field="channels"></dd>
        <dt>Durée</dt><dd data-field="duration"></dd>
        <dt>Échantillons décodés</dt><dd data-field="samples"></dd>
      </dl>
    </section>
  `;

  const fields = {
    path: container.querySelector<HTMLElement>('[data-field="path"]')!,
    size: container.querySelector<HTMLElement>('[data-field="size"]')!,
    rate: container.querySelector<HTMLElement>('[data-field="rate"]')!,
    channels: container.querySelector<HTMLElement>('[data-field="channels"]')!,
    duration: container.querySelector<HTMLElement>('[data-field="duration"]')!,
    samples: container.querySelector<HTMLElement>('[data-field="samples"]')!,
  };

  function render(state: DemoState) {
    fields.path.textContent = state.mp3.path;
    fields.size.textContent = `${(state.mp3.fileSizeBytes / 1024).toFixed(1)} Ko`;
    fields.rate.textContent = `${state.mp3.sampleRateHz} Hz`;
    fields.channels.textContent = String(state.mp3.channels);
    fields.duration.textContent = `${state.mp3.durationSec.toFixed(2)} s`;
    fields.samples.textContent = state.mp3.totalSamples.toLocaleString("fr-CA");
  }

  return { render };
}
```

- [ ] **Step 3: Verify visually**

Temporarily wire both into `main.ts` alongside the scene (same pattern as Task 9's Step 2 — append `initMetrics`/`initMp3Panel` calls and their `render` into the polling callback). Run both processes, confirm the panels populate with real numbers matching `curl /api/state`, and that dragging the player updates the per-source distance/azimuth/gain table live.

- [ ] **Step 4: Commit**

```bash
git add examples/web-demo/client/src/metrics.ts examples/web-demo/client/src/mp3panel.ts examples/web-demo/client/src/main.ts
git commit -m "feat(web-demo): metrics and MP3 decode panels"
```

---

### Task 11: `profileSelector.ts` — profile list + hot-swap

**Files:**
- Create: `examples/web-demo/client/src/profileSelector.ts`

**Interfaces:**
- Consumes: `fetchProfiles`, `postProfile` (Task 8).
- Produces: `initProfileSelector(container: HTMLElement): { render(activeId: string): void }` — used by Task 12 (`main.ts`).

- [ ] **Step 1: Write `src/profileSelector.ts`**

```ts
import { fetchProfiles, postProfile } from "./api";

export function initProfileSelector(container: HTMLElement): { render(activeId: string): void } {
  container.innerHTML = `
    <section aria-label="Sélecteur de profil HRTF">
      <h2>Catalogue de profils</h2>
      <p class="profile-hint" data-field="hint"></p>
      <ul class="profile-list" data-field="list" role="listbox" aria-label="Profils HRTF disponibles"></ul>
    </section>
  `;

  const list = container.querySelector<HTMLUListElement>('[data-field="list"]')!;
  const hint = container.querySelector<HTMLElement>('[data-field="hint"]')!;
  let activeId = "";

  function paintActive() {
    for (const li of Array.from(list.children)) {
      const el = li as HTMLLIElement;
      el.classList.toggle("profile-active", el.dataset.id === activeId);
      el.setAttribute("aria-selected", String(el.dataset.id === activeId));
    }
  }

  async function onSelect(id: string) {
    hint.textContent = `Bascule vers ${id}…`;
    const result = await postProfile(id);
    if (!result.ok) {
      hint.textContent = `Échec de bascule vers ${id} : ${result.error}`;
      return;
    }
    activeId = id;
    paintActive();
    hint.textContent = `Profil actif : ${id}`;
  }

  fetchProfiles()
    .then(({ profiles, active }) => {
      activeId = active;
      list.innerHTML = profiles
        .map((id) => `<li role="option" data-id="${id}" tabindex="0">${id}</li>`)
        .join("");
      for (const li of Array.from(list.children)) {
        li.addEventListener("click", () => void onSelect((li as HTMLLIElement).dataset.id!));
      }
      paintActive();
      hint.textContent = `Profil actif : ${activeId}`;
    })
    .catch(() => {
      hint.textContent = "Catalogue de profils indisponible — le serveur audio répond-il ?";
    });

  function render(newActiveId: string) {
    if (newActiveId !== activeId) {
      activeId = newActiveId;
      paintActive();
    }
  }

  return { render };
}
```

- [ ] **Step 2: Verify visually**

Wire temporarily into `main.ts` (same pattern as prior tasks). Run both processes. Expected: the list shows every real profile id from `assets/hrtf/`, the active one highlighted; clicking another audibly switches the HRTF profile within roughly a second (the real hot-swap, per Task 6) and the highlight moves; clicking again while a switch is in flight is safe (server returns `409`, handled — no crash, hint text stays informative).

- [ ] **Step 3: Commit**

```bash
git add examples/web-demo/client/src/profileSelector.ts examples/web-demo/client/src/main.ts
git commit -m "feat(web-demo): profile selector with real hot-swap"
```

---

### Task 12: `main.ts` wiring + full layout + unreachable banner

**Files:**
- Modify: `examples/web-demo/client/src/main.ts` (replace all temporary wiring from Tasks 8–11)
- Modify: `examples/web-demo/client/index.html` (full layout markup)
- Modify: `examples/web-demo/client/src/style.css` (full layout/theme, `.metric-flash` animation, `.unreachable` banner)

**Interfaces:**
- Consumes: everything from Tasks 8–11 (`startPolling`, `initScene`, `initMetrics`, `initMp3Panel`, `initProfileSelector`).
- Produces: the finished page — nothing downstream.

- [ ] **Step 1: Rewrite `index.html`**

```html
<!doctype html>
<html lang="fr">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>NATHAN-audio — démo HRTF</title>
    <link rel="stylesheet" href="/src/style.css" />
  </head>
  <body>
    <div class="app">
      <header class="app-header">
        <div class="brand">
          <span class="brand-mark" aria-hidden="true"></span>
          <h1>NATHAN-audio <span class="brand-sub">démo HRTF</span></h1>
        </div>
      </header>

      <div class="unreachable" id="unreachable" hidden role="alert">
        <p>
          Aucun serveur audio ne répond sur <code id="unreachableHost"></code>.
          Lance <code>web_demo_server</code> (voir <code>examples/web-demo/README.md</code>) puis recharge la page.
        </p>
      </div>

      <div class="layout">
        <aside class="left-col" aria-label="Profils et métriques">
          <div id="profileSelector"></div>
          <div id="metrics"></div>
          <div id="mp3panel"></div>
        </aside>
        <main class="scene-col" id="sceneContainer" aria-label="Scène spatiale"></main>
      </div>
    </div>
    <script type="module" src="/src/main.ts"></script>
  </body>
</html>
```

- [ ] **Step 2: Rewrite `src/main.ts`**

```ts
import { startPolling } from "./api";
import { initScene } from "./scene";
import { initMetrics } from "./metrics";
import { initMp3Panel } from "./mp3panel";
import { initProfileSelector } from "./profileSelector";

const SERVER_URL = import.meta.env.VITE_SERVER_URL ?? "http://127.0.0.1:8787";

const unreachable = document.querySelector<HTMLDivElement>("#unreachable")!;
document.querySelector<HTMLElement>("#unreachableHost")!.textContent = SERVER_URL;

const scene = initScene(document.querySelector<HTMLElement>("#sceneContainer")!);
const metrics = initMetrics(document.querySelector<HTMLElement>("#metrics")!);
const mp3panel = initMp3Panel(document.querySelector<HTMLElement>("#mp3panel")!);
const profileSelector = initProfileSelector(document.querySelector<HTMLElement>("#profileSelector")!);

startPolling(
  (state) => {
    scene.render(state);
    metrics.render(state);
    mp3panel.render(state);
    profileSelector.render(state.profile.active);
  },
  (reachable) => {
    unreachable.hidden = reachable;
  },
);
```

- [ ] **Step 3: Rewrite `src/style.css` (full layout, on top of the Task 7 tokens and Task 9 scene rules — keep those, add the following)**

```css
.app { display: flex; flex-direction: column; height: 100dvh; }

.app-header {
  display: flex; align-items: baseline; gap: 0.75rem;
  padding: 1.1rem 1.25rem 0.9rem;
  border-bottom: 1px solid color-mix(in srgb, var(--paper) 12%, transparent);
}
.brand { display: flex; align-items: center; gap: 0.5rem; }
.brand-mark { width: 0.6rem; height: 0.6rem; border-radius: 50%; background: var(--accent); box-shadow: 0 0 0.5rem var(--accent); }
.app-header h1 { margin: 0; font-size: 0.95rem; font-weight: 700; letter-spacing: 0.06em; font-family: ui-monospace, monospace; }
.brand-sub { color: var(--mute); font-weight: 500; }

.unreachable {
  margin: 1rem 1.25rem 0; padding: 0.9rem 1rem;
  border: 1px solid var(--alert); border-radius: 0.6rem;
  background: color-mix(in srgb, var(--alert) 12%, var(--ink-raised));
  font-size: 0.9rem;
}
.unreachable code { background: var(--ink-raised); padding: 0.1rem 0.35rem; border-radius: 0.3rem; font-family: ui-monospace, monospace; }

.layout { flex: 1; min-height: 0; display: flex; }

.left-col {
  width: 22rem; flex: none; overflow-y: auto;
  display: flex; flex-direction: column; gap: 1.25rem; padding: 1rem;
  border-right: 1px solid color-mix(in srgb, var(--paper) 12%, transparent);
  background: var(--ink-raised);
}
.left-col h2 { margin: 0 0 0.5rem; font-size: 0.78rem; letter-spacing: 0.1em; text-transform: uppercase; color: var(--mute); }

.scene-col { flex: 1; min-width: 0; padding: 1rem; }

.metric-list { display: grid; grid-template-columns: auto 1fr; gap: 0.3rem 0.75rem; margin: 0; font-size: 0.8rem; }
.metric-list dt { color: var(--mute); }
.metric-list dd { margin: 0; font-family: ui-monospace, monospace; text-align: right; }

.metric-table { width: 100%; border-collapse: collapse; font-size: 0.75rem; font-family: ui-monospace, monospace; }
.metric-table th, .metric-table td { text-align: left; padding: 0.2rem 0.3rem; }
.metric-table th { color: var(--mute); font-weight: 500; }

.profile-list { list-style: none; margin: 0; padding: 0; max-height: 12rem; overflow-y: auto; font-size: 0.75rem; font-family: ui-monospace, monospace; }
.profile-list li { padding: 0.3rem 0.5rem; border-radius: 0.3rem; cursor: pointer; }
.profile-list li:hover { background: color-mix(in srgb, var(--mute) 12%, transparent); }
.profile-list li.profile-active { background: color-mix(in srgb, var(--accent) 22%, transparent); color: var(--accent-strong); font-weight: 600; }
.profile-hint { font-size: 0.72rem; color: var(--mute); min-height: 1.2em; }

.mp3-note { font-size: 0.7rem; color: var(--mute); }

@keyframes metric-flash-anim {
  0% { color: var(--accent-strong); }
  100% { color: var(--paper); }
}
.metric-flash { animation: metric-flash-anim 0.6s ease-out; }

@media (prefers-reduced-motion: reduce) {
  .metric-flash { animation: none; }
}

@media (max-width: 60rem) {
  .layout { flex-direction: column; }
  .left-col { width: auto; max-height: 45vh; border-right: none; border-bottom: 1px solid color-mix(in srgb, var(--paper) 12%, transparent); }
}
```

- [ ] **Step 4: Full end-to-end manual verification**

Run `.\build\Debug\web_demo_server.exe` and `npm run dev`, open the page:

1. Drag the player (X) around — vectors, distance/azimuth/gain table, and the audible sound position all update together.
2. Click a different profile in the list — it highlights, the HRTF audibly changes within about a second, `profile.active` in the metrics panel updates.
3. Stop `web_demo_server.exe` — the unreachable banner appears within ~1 polling interval; restart it — the banner disappears and the page resumes.
4. Confirm the MP3 panel shows `assets/test-audio.mp3`, a size around 175–180 Ko, 44100 Hz, 1 channel, and a plausible duration.
5. Confirm the metrics panel shows "Sources simultanées : 4 — R-AUD-03 OK".
6. Run `npm run typecheck` in `examples/web-demo/client/` — expected: no errors.

- [ ] **Step 5: Commit**

```bash
git add examples/web-demo/client/index.html examples/web-demo/client/src/main.ts examples/web-demo/client/src/style.css
git commit -m "feat(web-demo): final layout, unreachable banner, end-to-end wiring"
```

---

### Task 13: `examples/web-demo/README.md` — run instructions

**Files:**
- Create: `examples/web-demo/README.md`
- Modify: `README.md` (root) — one link, see Step 2

**Interfaces:** none (documentation only).

- [ ] **Step 1: Write `examples/web-demo/README.md`**

```markdown
# Démo web — pipeline audio HRTF

Vitrine interactive du pipeline audio (profils HRTF CIPIC, décodage MP3, spatialisation
R-AUD-03), pour démonstration et enregistrement vidéo. Design complet :
[`docs/superpowers/specs/2026-08-06-web-audio-demo-design.md`](../docs/superpowers/specs/2026-08-06-web-audio-demo-design.md).

Le moteur audio réel (`server/`, C++/OpenAL) n'est pas réécrit : la page web (`client/`,
Vite/TypeScript) n'est qu'un client qui affiche ce que le serveur mesure et pilote sa
position/son profil. **Le son sort par la sortie audio de l'OS (OpenAL natif), pas par
l'onglet du navigateur.**

## Lancer la démo

1. Compiler le projet à la racine du repo (voir le `README.md` racine — vcpkg + CMake), puis :

   ```powershell
   .\build\Debug\web_demo_server.exe
   ```

   Démarre le moteur audio (profil HRTF par défaut, 4 sources en boucle) et l'API locale sur
   `http://127.0.0.1:8787`. Laisser tourner.

2. Dans un second terminal :

   ```powershell
   cd examples/web-demo/client
   npm install
   npm run dev
   ```

   Ouvrir l'URL affichée (par défaut `http://localhost:5173`).

## Utilisation

- **Glisser le X** (joueur) à la souris — le son suit en temps réel.
- **Cliquer un profil** dans la liste de gauche — bascule HRTF réelle, audible en quelques
  centaines de millisecondes.
- Le panneau **Sources** et le panneau **Décodage MP3** affichent des valeurs mesurées par le
  serveur C++, pas des approximations calculées dans le navigateur.

## Enregistrement vidéo

Capturer le **son du bureau** (OBS : "Desktop Audio" / capture audio système), pas le son de
l'onglet du navigateur — la sortie audio vient d'OpenAL, hors du navigateur.

## Configuration

`examples/web-demo/client/.env.example` → copier en `.env` pour changer `VITE_SERVER_URL` si le
serveur tourne sur un autre port.
```

- [ ] **Step 2: Link it from the root README**

Modify root `README.md`: in the "Structure du projet" tree (see file listing around the
`tools/` entry), add a line after the `benchmarks/` block:

```
├── examples/web-demo/               ← démo web interactive (vitrine) — voir examples/web-demo/README.md
```

- [ ] **Step 3: Commit**

```bash
git add examples/web-demo/README.md README.md
git commit -m "docs(web-demo): add run instructions"
```

---

## Self-Review Notes

- **Spec coverage:** every section of `docs/superpowers/specs/2026-08-06-web-audio-demo-design.md`
  maps to a task — réutilisation (Tasks 5–6), API HTTP shape (Tasks 4, 6, 8), scène/mise en
  page (Tasks 9–12), panneau MP3 (Task 10), threading/switch-de-profil (Task 6), README/vidéo
  (Task 13). The "Hors scope" list (walls/EFX, scripted tour, in-browser DSP, writing
  `settings.json`) has no corresponding task — confirmed intentional.
- **Type consistency:** `web_demo::DemoState`/`SourceState`/`ProfileState`/`HrtfState`/`Mp3State`
  (Task 4) match field-for-field the TypeScript `DemoState`/`SourceState` (Task 8) and the
  JSON produced by `buildStateJson` (Task 4, exercised by Task 6). `createLiveSources`,
  `kSourceDefs`, `SourceDef`, `LiveSource` are defined once in Task 5 and reused unchanged in
  Task 6. `RollingFrameStats`/`FrameStatsSnapshot` (Task 3) are consumed identically by
  `state_json.h` (Task 4) and `main.cpp` (Tasks 5–6).
- **No placeholders:** every step above contains complete, concrete code — no "add error
  handling"/"similar to Task N" gaps.
