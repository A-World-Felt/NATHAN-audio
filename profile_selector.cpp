#include <AL/al.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "src/al_hrtf_device.h"
#include "src/hrtf_profile.h"
#include "src/input_keys.h"
#include "src/wav_loader.h"

namespace fs = std::filesystem;

namespace {

constexpr double kSweepPeriodSec = 15.0;  // duree d'un tour complet du cercle
constexpr float kRadiusMeters = 1.5f;

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

ALuint makeLoopingSource(ALuint buffer) {
    ALuint source = 0;
    alGenSources(1, &source);
    alSourcei(source, AL_BUFFER, static_cast<ALint>(buffer));
    alSourcei(source, AL_LOOPING, AL_TRUE);
    return source;
}

// azimuthDeg : 0=avant (-Z), +90=droite (+X), -90=gauche, 180=derriere.
void setAzimuth(ALuint source, float azimuthDeg, float radius = kRadiusMeters) {
    float rad = azimuthDeg * 3.14159265f / 180.0f;
    alSource3f(source, AL_POSITION, std::sin(rad) * radius, 0.0f, -std::cos(rad) * radius);
}

}  // namespace

int main() {
    const fs::path hrtfDir = "assets/hrtf";
    const fs::path wavPath = "assets/test-audio.wav";
    const fs::path configDir = "assets/config";
    const fs::path configPath = configDir / "user_profile.txt";

    std::vector<fs::path> files;
    try {
        files = hrtf_profile::listProfiles(hrtfDir);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur en listant %s : %s\n", hrtfDir.string().c_str(), e.what());
        return 1;
    }
    if (files.empty()) {
        std::fprintf(stderr, "Aucun fichier .sofa trouve dans %s\n", hrtfDir.string().c_str());
        return 1;
    }

    WavAudio wav;
    try {
        wav = load_wav_mono16(wavPath.string());
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur de chargement WAV : %s\n", e.what());
        return 1;
    }

    size_t idx = 0;
    AlHrtfDevice alDevice;
    ALuint buffer = 0;
    ALuint source = 0;

    // Installe le profil courant (copie .sofa -> dossier OpenAL), redemarre
    // le contexte OpenAL avec ALC_HRTF_SOFT=ALC_TRUE, recree buffer+source
    // (invalides depuis la fermeture du device precedent).
    auto loadCurrentProfile = [&]() {
        hrtf_profile::installProfile(files[idx]);
        alDevice.open();
        buffer = makeBufferFromWav(wav);
        source = makeLoopingSource(buffer);
        alSourcePlay(source);
        std::printf("\rProfil %3zu / %3zu : %-20s [HRTF: %s]", idx + 1, files.size(),
                    files[idx].stem().string().c_str(), alDevice.hrtfStatusString().c_str());
        std::fflush(stdout);
    };

    try {
        loadCurrentProfile();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "\nErreur d'initialisation HRTF : %s\n", e.what());
        return 1;
    }

    std::printf("\n=== NATHAN - Selecteur de profil HRTF CIPIC ===\n");
    std::printf("Fleche gauche/droite : changer de profil | Entree : confirmer\n\n");

    auto t0 = std::chrono::steady_clock::now();
    bool running = true;

    while (running) {
        double elapsedSec =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        float azimuth = static_cast<float>(std::fmod(elapsedSec / kSweepPeriodSec, 1.0) * 360.0);
        setAzimuth(source, azimuth);

        bool changed = false;
        switch (poll_key_nonblocking()) {
            case KeyEvent::Left:
                idx = (idx + files.size() - 1) % files.size();
                changed = true;
                break;
            case KeyEvent::Right:
                idx = (idx + 1) % files.size();
                changed = true;
                break;
            case KeyEvent::Enter:
                running = false;
                break;
            default:
                break;
        }

        if (changed) {
            try {
                loadCurrentProfile();
            } catch (const std::exception& e) {
                std::fprintf(stderr, "\nErreur de rechargement HRTF : %s\n", e.what());
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    alDevice.close();

    std::error_code ec;
    fs::create_directories(configDir, ec);
    std::string chosen = files[idx].stem().string();
    std::ofstream out(configPath);
    out << chosen << "\n";
    out.close();

    std::printf("\n\nProfil enregistre : %s -> %s\n", chosen.c_str(), configPath.string().c_str());
    return 0;
}
