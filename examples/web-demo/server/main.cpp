// Démo web — serveur : voir docs/superpowers/specs/2026-08-06-web-audio-demo-design.md.
#include "httplib.h"

#include <AL/al.h>
#include <AL/alc.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "hrtf/active_profile.h"
#include "hrtf/config_paths.h"
#include "hrtf/hrtf_context.h"
#include "hrtf/openal_paths.h"
#include "hrtf/profile_catalog.h"
#include "hrtf/profile_settings.h"

#include "mp3_loader.h"
#include "wav_loader.h"

#include "frame_stats.h"
#include "shared_state.h"
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

        bool switchJustCompleted = false;
        bool switchSuccess = false;
        std::string switchErrorMessage;

        if (switchId.has_value()) {
            switchSuccess = true;
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
                switchSuccess = false;
                switchErrorMessage = e.what();
            }
            switchJustCompleted = true;
            // Ne pas marquer pendingProfileSwitch->completed / notifier ici :
            // le thread HTTP en attente lirait shared.latestStateJson des son
            // reveil, et cette frame ne l'a pas encore republie avec le
            // profil a jour (voir plus bas, meme verrou que la publication).
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
        state.mp3 = {kSourceDefs[0].assetPath, static_cast<std::size_t>(mp3FileSize),
                    static_cast<int>(mp3Cache.sampleRate), static_cast<int>(mp3Cache.channels),
                    static_cast<double>(mp3Cache.samples.size() / mp3Cache.channels) / mp3Cache.sampleRate,
                    mp3Cache.samples.size() / mp3Cache.channels};

        {
            std::lock_guard<std::mutex> lock(shared.mutex);
            shared.latestStateJson = web_demo::buildStateJson(state);
            if (switchJustCompleted) {
                shared.pendingProfileSwitch->success = switchSuccess;
                shared.pendingProfileSwitch->errorMessage = switchErrorMessage;
                shared.pendingProfileSwitch->completed = true;
                if (switchSuccess) shared.activeProfileId = *switchId;
            }
        }
        // notify_all hors du verrou (evite de reveiller le thread HTTP pour
        // qu'il se rebloque aussitot sur un mutex qu'on tient encore) ; les
        // ecritures ci-dessus sont deja visibles grace au unlock qui precede.
        if (switchJustCompleted) shared.profileSwitchDone.notify_all();

        std::this_thread::sleep_until(frameStart + kFrameDuration);
    }

    svr.stop();
    httpThread.join();
    context.close();
    return 0;
}
