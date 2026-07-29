// Modifier ce nom de profil selon celui choisi via profile_selector
// (voir assets/config/user_profile.txt).
#define HRTF_PROFILE "subject_003"

#include <AL/al.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "src/al_hrtf_device.h"
#include "src/hrtf_profile.h"
#include "src/mp3_loader.h"

namespace {

constexpr float kRadiusMeters = 1.5f;

ALuint makeLoopingSource(ALuint buffer) {
    ALuint source = 0;
    alGenSources(1, &source);
    alSourcei(source, AL_BUFFER, static_cast<ALint>(buffer));
    alSourcei(source, AL_LOOPING, AL_TRUE);
    alSourcef(source, AL_REFERENCE_DISTANCE, 1.0f);
    alSourcef(source, AL_MAX_DISTANCE, 20.0f);
    alSourcef(source, AL_ROLLOFF_FACTOR, 1.0f);
    return source;
}

// azimuthDeg : 0=avant (-Z), +90=droite (+X), -90=gauche, 180=derriere.
void setAzimuth(ALuint source, float azimuthDeg, float radius = kRadiusMeters) {
    float rad = azimuthDeg * 3.14159265f / 180.0f;
    alSource3f(source, AL_POSITION, std::sin(rad) * radius, 0.0f, -std::cos(rad) * radius);
}

void pauseForEnter(const char* msg) {
    std::printf("\n%s\nAppuyez sur Entree pour continuer...\n", msg);
    std::fflush(stdout);
    while (std::getchar() != '\n') {
        // vide un eventuel caractere restant, jusqu'a la prochaine touche Entree
    }
}

void holdFor(double seconds, const std::function<void(double)>& updateFn = nullptr) {
    auto t0 = std::chrono::steady_clock::now();
    double elapsed = 0.0;
    while (elapsed < seconds) {
        if (updateFn) updateFn(elapsed);
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    }
}

}  // namespace

int main() {
    const std::string sofaPath = std::string("assets/hrtf/") + HRTF_PROFILE + ".sofa";

    try {
        hrtf_profile::installProfile(sofaPath);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur d'installation du profil HRTF : %s\n", e.what());
        return 1;
    }

    AlHrtfDevice alDevice;
    try {
        alDevice.open();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur d'initialisation OpenAL/HRTF : %s\n", e.what());
        return 1;
    }
    std::printf("=== NATHAN - Tests de spatialisation (profil %s, HRTF: %s) ===\n", HRTF_PROFILE,
                alDevice.hrtfStatusString().c_str());

    Mp3Audio mp3;
        try {
            mp3 = load_mp3("assets/test-audio.mp3");
        } catch (const std::exception& e) {
            std::fprintf(stderr, "Erreur de chargement MP3 : %s\n", e.what());
            return 1;
        }
    ALuint buffer = makeBufferFromMp3(mp3);
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

    // --- Test 1 : gauche puis droite ---
    pauseForEnter("Test 1/6 : son totalement a GAUCHE (4s) puis totalement a DROITE (4s).");
    {
        ALuint src = makeLoopingSource(buffer);
        setAzimuth(src, -90.0f);
        alSourcePlay(src);
        holdFor(4.0);
        setAzimuth(src, 90.0f);
        holdFor(4.0);
        alSourceStop(src);
        alDeleteSources(1, &src);
    }

    // --- Test 2 : 4 sources simultanees ---
    pauseForEnter(
        "Test 2/6 : 4 sources simultanees (avant, droite, derriere, gauche), demarrage echelonne.");
    {
        const float azimuths[4] = {0.0f, 90.0f, 180.0f, -90.0f};
        const char* labels[4] = {"avant", "droite", "derriere", "gauche"};
        ALuint srcs[4] = {0, 0, 0, 0};
        for (int i = 0; i < 4; ++i) {
            srcs[i] = makeLoopingSource(buffer);
            setAzimuth(srcs[i], azimuths[i]);
            alSourcef(srcs[i], AL_GAIN, 0.7f);
            alSourcePlay(srcs[i]);
            std::printf("  Source %d ajoutee (%s, %.0f deg)\n", i + 1, labels[i], azimuths[i]);
            holdFor(2.0);
        }
        holdFor(4.0);
        for (int i = 0; i < 4; ++i) {
            alSourceStop(srcs[i]);
            alDeleteSources(1, &srcs[i]);
        }
    }

    // --- Test 3 : sweep droite->gauche puis 2 tours complets ---
    pauseForEnter("Test 3/6 : balayage droite -> gauche, puis 2 tours complets autour de la tete.");
    {
        ALuint src = makeLoopingSource(buffer);
        setAzimuth(src, 90.0f);
        alSourcePlay(src);
        holdFor(4.0, [&](double t) {
            float az = 90.0f - 180.0f * static_cast<float>(t / 4.0);
            setAzimuth(src, az);
        });
        holdFor(7.2, [&](double t) {
            float az = static_cast<float>(t) / 7.2f * 720.0f;
            setAzimuth(src, az);
        });
        alSourceStop(src);
        alDeleteSources(1, &src);
    }

    // --- Test 4 : attenuation par distance ---
    pauseForEnter("Test 4/6 : la source s'approche puis s'eloigne (avant).");
    {
        ALuint src = makeLoopingSource(buffer);
        alSource3f(src, AL_POSITION, 0.0f, 0.0f, -10.0f);
        alSourcePlay(src);

        double lastPrint = -1.0;
        holdFor(6.0, [&](double t) {
            float dist = 10.0f - 9.5f * static_cast<float>(t / 6.0);  // 10m -> 0.5m
            alSource3f(src, AL_POSITION, 0.0f, 0.0f, -dist);
            if (t - lastPrint >= 1.0) {
                std::printf("  Distance ~= %.1f m\n", dist);
                lastPrint = t;
            }
        });
        lastPrint = -1.0;
        holdFor(6.0, [&](double t) {
            float dist = 0.5f + 9.5f * static_cast<float>(t / 6.0);  // 0.5m -> 10m
            alSource3f(src, AL_POSITION, 0.0f, 0.0f, -dist);
            if (t - lastPrint >= 1.0) {
                std::printf("  Distance ~= %.1f m\n", dist);
                lastPrint = t;
            }
        });
        alSourceStop(src);
        alDeleteSources(1, &src);
    }

    // --- Test 5 : confusion avant/arriere ---
    pauseForEnter("Test 5/6 : alternance avant/derriere, d'abord lente puis rapide.");
    {
        ALuint src = makeLoopingSource(buffer);
        setAzimuth(src, 0.0f);
        alSourcePlay(src);

        for (int cycle = 0; cycle < 3; ++cycle) {
            setAzimuth(src, 0.0f);
            std::printf("  AVANT\n");
            holdFor(2.0);
            setAzimuth(src, 180.0f);
            std::printf("  DERRIERE\n");
            holdFor(2.0);
        }
        for (int cycle = 0; cycle < 6; ++cycle) {
            setAzimuth(src, 0.0f);
            holdFor(0.4);
            setAzimuth(src, 180.0f);
            holdFor(0.4);
        }
        alSourceStop(src);
        alDeleteSources(1, &src);
    }

    // --- Test 6 : élévation (Y != 0) ---
    pauseForEnter("Test 6/6 : elevation - son venant du bas puis du haut puis en diagonale.");
    {
        ALuint src = makeLoopingSource(buffer);
        alSource3f(src, AL_POSITION, 0.0f, 0.0f, -1.5f);
        alSourcePlay(src);

        // Bas (-2m)
        alSource3f(src, AL_POSITION, 0.0f, -2.0f, -1.5f);
        std::printf("  BAS (y = -2m)\n");
        holdFor(3.0);

        // Haut (+2m)
        alSource3f(src, AL_POSITION, 0.0f, 2.0f, -1.5f);
        std::printf("  HAUT (y = +2m)\n");
        holdFor(3.0);

        // Sweep bas -> haut
        std::printf("  Sweep BAS -> HAUT\n");
        holdFor(4.0, [&](double t) {
            float y = -2.0f + 4.0f * static_cast<float>(t / 4.0);
            alSource3f(src, AL_POSITION, 0.0f, y, -1.5f);
        });

        // Diagonale : devant-bas -> derriere-haut
        std::printf("  Diagonale : devant-bas -> derriere-haut\n");
        holdFor(4.0, [&](double t) {
            float ratio = static_cast<float>(t / 4.0);
            float z = -1.5f + 3.0f * ratio;   // devant -> derriere
            float y = -2.0f + 4.0f * ratio;   // bas -> haut
            alSource3f(src, AL_POSITION, 0.0f, y, z);
        });

        alSourceStop(src);
        alDeleteSources(1, &src);
    }

    alDeleteBuffers(1, &buffer);
    alDevice.close();

    std::printf("\n=== Tests termines ===\n");
    std::printf("Verifiez a l'oreille : gauche/droite net (T1), 4 sources distinctes sans\n");
    std::printf("artefact (T2), balayage/cercle fluides (T3), attenuation naturelle (T4),\n");
    std::printf("avant/arriere distinguable meme en alternance rapide (T5), haut/bas et\n");
    std::printf("diagonale perceptibles (T6).\n");
    return 0;
}
