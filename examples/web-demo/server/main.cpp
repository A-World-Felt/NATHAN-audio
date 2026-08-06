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
