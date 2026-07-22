// Prototype du contrat SoundScape/Sound du moteur NATHAN : le module audio
// recoit une position relative (relX, relY, relZ) et la passe telle quelle
// a OpenAL Soft (alSource3f) - c'est OpenAL Soft qui fait toute la
// convolution HRTF en interne (voir src/al_hrtf_device.h). Aucune logique
// de jeu ici ne depend de windows.h ; la seule exception isolee est le
// reglage de la page de code console pour afficher le symbole "note" en
// UTF-8 sous Windows (meme principe que le #ifdef _WIN32 de input_keys.cpp).

#define HRTF_PROFILE "subject_003"

#ifdef _WIN32
#include <windows.h>
#endif

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx-presets.h>
#include <AL/efx.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "src/al_hrtf_device.h"
#include "src/hrtf_profile.h"
#include "src/input_keys.h"
#include "src/wav_loader.h"

namespace {

constexpr int kGridSize = 21;
constexpr int kGridHalf = kGridSize / 2;  // 10
constexpr auto kFrameDuration = std::chrono::milliseconds(15);
constexpr float kMetersPerCell = 1.0f;  // 1 case de grille = 1 metre en audio

struct WorldPos {
    int x = 0;
    int z = 0;
};

// --- Plan des deux pieces (phase 2) - genere proceduralement ------------
// Deux pieces carrees de kRoomInteriorSize de cote, separees par un mur
// perce d'une ouverture de kOpeningWidth cases, centree verticalement.
// (Pieces deux fois plus grandes que la version initiale 4x4, pour rendre
// l'attenuation par distance et le contraste de reverb plus perceptibles.)
constexpr int kRoomInteriorSize = 8;
constexpr int kOpeningWidth = 2;

constexpr int kRoomWidth = 1 + kRoomInteriorSize + 1 + kRoomInteriorSize + 1;  // 19
constexpr int kRoomHeight = 1 + kRoomInteriorSize + 1;                        // 10
constexpr int kSeparatorX = 1 + kRoomInteriorSize;                            // colonne du mur separateur
constexpr int kOpeningStartZ = 1 + (kRoomInteriorSize - kOpeningWidth) / 2;   // ouverture centree

bool isWall(int wx, int wz) {
    if (wx < 0 || wx >= kRoomWidth || wz < 0 || wz >= kRoomHeight) return true;
    if (wx == 0 || wx == kRoomWidth - 1 || wz == 0 || wz == kRoomHeight - 1) return true;  // perimetre
    if (wx == kSeparatorX) {
        bool isOpening = wz >= kOpeningStartZ && wz < kOpeningStartZ + kOpeningWidth;
        return !isOpening;
    }
    return false;
}

// Ray casting simple (Bresenham) : compte les murs traverses entre a et b,
// sans compter les cases de depart/arrivee elles-memes.
int countWallsBetween(WorldPos a, WorldPos b) {
    int x0 = a.x, z0 = a.z, x1 = b.x, z1 = b.z;
    int dx = std::abs(x1 - x0), dz = std::abs(z1 - z0);
    int sx = (x0 < x1) ? 1 : -1;
    int sz = (z0 < z1) ? 1 : -1;
    int err = dx - dz;
    int x = x0, z = z0;
    int count = 0;

    while (true) {
        bool isEndpoint = (x == a.x && z == a.z) || (x == b.x && z == b.z);
        if (!isEndpoint && isWall(x, z)) count++;
        if (x == x1 && z == z1) break;
        int e2 = 2 * err;
        if (e2 > -dz) { err -= dz; x += sx; }
        if (e2 < dx) { err += dx; z += sz; }
    }
    return count;
}

// --- OpenAL : buffer + source mono en boucle -----------------------------

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

// Applique directement la position relative recue du "SoundScape" au
// pipeline HRTF d'OpenAL Soft. C'est tout le contrat : (relX,relY,relZ) in,
// alSource3f out.
void applyRelativePosition(ALuint source, float relX, float relY, float relZ) {
    alSource3f(source, AL_POSITION, relX, relY, relZ);
}

// --- EFX (reverb) : charge dynamiquement, comme ALC_SOFT_HRTF -----------

struct EfxApi {
    LPALGENEFFECTS alGenEffects = nullptr;
    LPALDELETEEFFECTS alDeleteEffects = nullptr;
    LPALEFFECTI alEffecti = nullptr;
    LPALEFFECTF alEffectf = nullptr;
    LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots = nullptr;
    LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots = nullptr;
    LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti = nullptr;
    bool available = false;
};

EfxApi loadEfxApi(ALCdevice* device) {
    EfxApi api;
    if (alcIsExtensionPresent(device, "ALC_EXT_EFX") == ALC_FALSE) return api;

    api.alGenEffects = reinterpret_cast<LPALGENEFFECTS>(alGetProcAddress("alGenEffects"));
    api.alDeleteEffects = reinterpret_cast<LPALDELETEEFFECTS>(alGetProcAddress("alDeleteEffects"));
    api.alEffecti = reinterpret_cast<LPALEFFECTI>(alGetProcAddress("alEffecti"));
    api.alEffectf = reinterpret_cast<LPALEFFECTF>(alGetProcAddress("alEffectf"));
    api.alGenAuxiliaryEffectSlots = reinterpret_cast<LPALGENAUXILIARYEFFECTSLOTS>(
        alGetProcAddress("alGenAuxiliaryEffectSlots"));
    api.alDeleteAuxiliaryEffectSlots = reinterpret_cast<LPALDELETEAUXILIARYEFFECTSLOTS>(
        alGetProcAddress("alDeleteAuxiliaryEffectSlots"));
    api.alAuxiliaryEffectSloti =
        reinterpret_cast<LPALAUXILIARYEFFECTSLOTI>(alGetProcAddress("alAuxiliaryEffectSloti"));

    api.available = api.alGenEffects && api.alDeleteEffects && api.alEffecti && api.alEffectf &&
                    api.alGenAuxiliaryEffectSlots && api.alDeleteAuxiliaryEffectSlots &&
                    api.alAuxiliaryEffectSloti;
    return api;
}

// Charge un preset EFXEAXREVERBPROPERTIES dans un effet AL_EFFECT_REVERB
// (le sous-ensemble de proprietes standard, pas la variante EAX complete).
ALuint createReverbEffect(const EfxApi& efx, const EFXEAXREVERBPROPERTIES& preset) {
    ALuint effect = 0;
    efx.alGenEffects(1, &effect);
    efx.alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
    efx.alEffectf(effect, AL_REVERB_DENSITY, preset.flDensity);
    efx.alEffectf(effect, AL_REVERB_DIFFUSION, preset.flDiffusion);
    efx.alEffectf(effect, AL_REVERB_GAIN, preset.flGain);
    efx.alEffectf(effect, AL_REVERB_GAINHF, preset.flGainHF);
    efx.alEffectf(effect, AL_REVERB_DECAY_TIME, preset.flDecayTime);
    efx.alEffectf(effect, AL_REVERB_DECAY_HFRATIO, preset.flDecayHFRatio);
    efx.alEffectf(effect, AL_REVERB_REFLECTIONS_GAIN, preset.flReflectionsGain);
    efx.alEffectf(effect, AL_REVERB_REFLECTIONS_DELAY, preset.flReflectionsDelay);
    efx.alEffectf(effect, AL_REVERB_LATE_REVERB_GAIN, preset.flLateReverbGain);
    efx.alEffectf(effect, AL_REVERB_LATE_REVERB_DELAY, preset.flLateReverbDelay);
    efx.alEffectf(effect, AL_REVERB_AIR_ABSORPTION_GAINHF, preset.flAirAbsorptionGainHF);
    efx.alEffectf(effect, AL_REVERB_ROOM_ROLLOFF_FACTOR, preset.flRoomRolloffFactor);
    efx.alEffecti(effect, AL_REVERB_DECAY_HFLIMIT, preset.iDecayHFLimit);
    return effect;
}

// --- Rendu terminal --------------------------------------------------------

void printPadded(std::string line, size_t width = 70) {
    if (line.size() < width) line.append(width - line.size(), ' ');
    std::fputs(line.c_str(), stdout);
    std::fputc('\n', stdout);
}

void renderFrame(const WorldPos& player, int actorGridX, int actorGridZ, bool phase2,
                  const std::string& headerLine, const std::string& statusLine1,
                  const std::string& statusLine2, const std::string& statusLine3 = "") {
    std::fputs("\033[H", stdout);
    printPadded(headerLine);
    printPadded("");

    for (int dz = -kGridHalf; dz <= kGridHalf; ++dz) {
        std::string row;
        row.reserve(kGridSize * 3);
        for (int dx = -kGridHalf; dx <= kGridHalf; ++dx) {
            int wx = player.x + dx;
            int wz = player.z + dz;
            if (dx == 0 && dz == 0) {
                row += '@';
            } else if (wx == actorGridX && wz == actorGridZ) {
                row += "\xE2\x99\xAA";  // U+266A EIGHTH NOTE en UTF-8
            } else if (phase2 && isWall(wx, wz)) {
                row += '#';
            } else {
                row += '.';
            }
        }
        std::fputs(row.c_str(), stdout);
        std::fputc('\n', stdout);
    }

    printPadded("");
    printPadded(statusLine1);
    printPadded(statusLine2);
    printPadded(statusLine3);
    std::fflush(stdout);
}

}  // namespace

int main() {
#ifdef _WIN32
    // Reglage local a l'affichage console (pas de la logique de jeu) pour
    // que le symbole UTF-8 du "acteur sonore" s'affiche correctement.
    SetConsoleOutputCP(CP_UTF8);
#endif

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

    WavAudio wav;
    try {
        wav = load_wav_mono16("assets/test-audio.wav");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur de chargement WAV : %s\n", e.what());
        return 1;
    }

    ALuint buffer = makeBufferFromWav(wav);
    ALuint source = makeLoopingSource(buffer);
    alSourcePlay(source);

    EfxApi efx = loadEfxApi(alDevice.device());
    ALuint effectSlot = 0, roomEffect = 0, farEffect = 0;
    bool efxReady = false;
    if (efx.available) {
        efx.alGenAuxiliaryEffectSlots(1, &effectSlot);
        EFXEAXREVERBPROPERTIES roomPreset = EFX_REVERB_PRESET_ROOM;
        EFXEAXREVERBPROPERTIES farPreset = EFX_REVERB_PRESET_STONEROOM;
        roomEffect = createReverbEffect(efx, roomPreset);
        farEffect = createReverbEffect(efx, farPreset);
        efxReady = true;
    }

    std::fputs("\033[2J\033[H", stdout);  // clear initial ; les frames suivantes n'utilisent que \033[H

    // ======================= Phase 1 : cercle libre =======================
    {
        WorldPos player{0, 0};
        auto t0 = std::chrono::steady_clock::now();
        constexpr float kCircleRadius = 6.0f;
        constexpr double kRevolutionSec = 12.0;

        bool running = true;
        while (running) {
            double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            double thetaRad = (t / kRevolutionSec) * 2.0 * 3.14159265358979323846;
            float actorXf = kCircleRadius * static_cast<float>(std::sin(thetaRad));
            float actorZf = -kCircleRadius * static_cast<float>(std::cos(thetaRad));

            float relX = (actorXf - static_cast<float>(player.x)) * kMetersPerCell;
            float relZ = (actorZf - static_cast<float>(player.z)) * kMetersPerCell;
            applyRelativePosition(source, relX, 0.0f, relZ);

            char status1[96];
            std::snprintf(status1, sizeof(status1), "Position relative acteur : x=%.2fm  z=%.2fm",
                          relX, relZ);

            renderFrame(player, static_cast<int>(std::lround(actorXf)),
                        static_cast<int>(std::lround(actorZf)), false,
                        "=== NATHAN - Phase 1 : cercle libre ===",
                        "Fleches : deplacer le joueur | Entree : passer en phase 2", status1);

            switch (poll_key_nonblocking()) {
                case KeyEvent::Up: player.z -= 1; break;
                case KeyEvent::Down: player.z += 1; break;
                case KeyEvent::Left: player.x -= 1; break;
                case KeyEvent::Right: player.x += 1; break;
                case KeyEvent::Enter: running = false; break;
                default: break;
            }

            std::this_thread::sleep_for(kFrameDuration);
        }
    }

    // ======================= Phase 2 : pieces + murs =======================
    {
        WorldPos player{5, 2};   // piece gauche, hors de l'alignement avec l'ouverture
        WorldPos actor{14, 2};   // piece droite (statique), meme rangee que le joueur

        if (efxReady) {
            alSource3i(source, AL_AUXILIARY_SEND_FILTER, static_cast<ALint>(effectSlot), 0,
                       AL_FILTER_NULL);
        }

        bool lastSameRoom = true;  // force la mise a jour au premier tour
        bool firstFrame = true;

        std::fputs("\033[2J\033[H", stdout);

        bool running = true;
        while (running) {
            int wallCount = countWallsBetween(player, actor);
            bool sameRoom = (wallCount == 0);

            if (efxReady && (sameRoom != lastSameRoom || firstFrame)) {
                ALuint activeEffect = sameRoom ? roomEffect : farEffect;
                efx.alAuxiliaryEffectSloti(effectSlot, AL_EFFECTSLOT_EFFECT,
                                           static_cast<ALint>(activeEffect));
                lastSameRoom = sameRoom;
            }
            firstFrame = false;

            alSourcef(source, AL_GAIN, sameRoom ? 1.0f : 0.6f);  // -40% si piece differente

            float relX = static_cast<float>(actor.x - player.x) * kMetersPerCell;
            float relZ = static_cast<float>(actor.z - player.z) * kMetersPerCell;
            applyRelativePosition(source, relX, 0.0f, relZ);

            char status1[96];
            std::snprintf(status1, sizeof(status1), "Position relative acteur : x=%.2fm  z=%.2fm",
                          relX, relZ);
            char status2[96];
            std::snprintf(status2, sizeof(status2),
                          "%s (%d mur(s) traverse(s)) | gain=%.1f | reverb=%s",
                          sameRoom ? "Meme piece" : "Piece differente", wallCount,
                          sameRoom ? 1.0f : 0.6f, efxReady ? (sameRoom ? "room" : "stoneroom") : "n/a");

            renderFrame(player, actor.x, actor.z, true, "=== NATHAN - Phase 2 : murs et pieces ===",
                        "Fleches : deplacer le joueur | Entree/Echap : quitter", status1, status2);

            switch (poll_key_nonblocking()) {
                case KeyEvent::Up:
                    if (!isWall(player.x, player.z - 1)) player.z -= 1;
                    break;
                case KeyEvent::Down:
                    if (!isWall(player.x, player.z + 1)) player.z += 1;
                    break;
                case KeyEvent::Left:
                    if (!isWall(player.x - 1, player.z)) player.x -= 1;
                    break;
                case KeyEvent::Right:
                    if (!isWall(player.x + 1, player.z)) player.x += 1;
                    break;
                case KeyEvent::Enter:
                case KeyEvent::Escape:
                    running = false;
                    break;
                default:
                    break;
            }

            std::this_thread::sleep_for(kFrameDuration);
        }
    }

    alSourceStop(source);
    if (efxReady) {
        alSource3i(source, AL_AUXILIARY_SEND_FILTER, 0, 0, AL_FILTER_NULL);
        efx.alDeleteAuxiliaryEffectSlots(1, &effectSlot);
        efx.alDeleteEffects(1, &roomEffect);
        efx.alDeleteEffects(1, &farEffect);
    }
    alDeleteSources(1, &source);
    alDeleteBuffers(1, &buffer);
    alDevice.close();

    std::printf("\nSimulation terminee.\n");
    return 0;
}
