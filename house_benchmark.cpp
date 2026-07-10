// Benchmark interactif du pipeline audio spatial HRTF (OpenAL Soft) dans une
// "grande maison" a 5 pieces, pour se rapprocher des conditions reelles du
// jeu La Maison de Nathan : le joueur est fixe a (0,0,0) dans le referentiel
// audio, c'est la maison (ses 5 sources sonores) qui se deplace autour de
// lui - meme contrat (relX,relY,relZ) -> alSource3f que game_simulation.cpp
// et audio_benchmark.cpp. 5 sources simultanees (au-dela du minimum R-AUD-03
// de 4) pour tester une charge superieure au strict necessaire.
//
// Modifier ce nom de profil selon celui a comparer (cf. assets/hrtf/*.sofa).
#define HRTF_PROFILE "subject_003"

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx-presets.h>
#include <AL/efx.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "src/al_hrtf_device.h"
#include "src/hrtf_profile.h"
#include "src/input_keys.h"
#include "src/wav_loader.h"

namespace {

constexpr auto kFrameDuration = std::chrono::milliseconds(15);  // ~66 FPS, budget temps reel
constexpr float kMetersPerCell = 1.0f;  // 1 case de grille = 1 metre en audio
constexpr const char* kResultsPath = "house_benchmark_results.txt";

struct WorldPos {
    int x = 0;
    int z = 0;
};

// --- Plan de la maison : grille fixe 31x21, 5 pieces --------------------
//
//   Cuisine    | Salon
//   -----------+-----------
//   Chambre    | Salle de bain
//   -----------+
//       Salle de musique
//
// Perimetre + 2 murs separateurs horizontaux (kSepZ1, kSepZ2) + 1 mur
// separateur vertical (kSepX, uniquement au-dessus de la salle de musique).
// Chaque mur separateur est perce d'ouvertures (portes) de 2 cases.
constexpr int kHouseWidth = 31;
constexpr int kHouseHeight = 21;

constexpr int kSepX = 15;   // colonne separant les pieces gauche/droite
constexpr int kSepZ1 = 7;   // rangee separant les pieces du haut (Cuisine/Salon) du milieu
constexpr int kSepZ2 = 14;  // rangee separant les pieces du milieu de la salle de musique

constexpr int kDoorTopZ0 = 3, kDoorTopZ1 = 4;    // porte Cuisine <-> Salon
constexpr int kDoorMidZ0 = 10, kDoorMidZ1 = 11;  // porte Chambre <-> Salle de bain
constexpr int kDoorLeftX0 = 6, kDoorLeftX1 = 7;   // porte Cuisine <-> Chambre, et Chambre <-> Musique
constexpr int kDoorRightX0 = 21, kDoorRightX1 = 22;  // porte Salon <-> Salle de bain, et SdB <-> Musique

bool isWall(int x, int z) {
    if (x < 0 || x >= kHouseWidth || z < 0 || z >= kHouseHeight) return true;
    if (x == 0 || x == kHouseWidth - 1 || z == 0 || z == kHouseHeight - 1) return true;  // perimetre

    // Poteaux d'angle a l'intersection separateur vertical / horizontaux :
    // murs pleins, les portes sont volontairement decalees de ces points.
    if (x == kSepX && (z == kSepZ1 || z == kSepZ2)) return true;

    // Separateur vertical : Cuisine|Salon en haut, Chambre|Salle de bain au
    // milieu. Pas de separateur au-dessus de la salle de musique (z > kSepZ2).
    if (x == kSepX) {
        if (z >= 1 && z <= kSepZ1 - 1) return !(z == kDoorTopZ0 || z == kDoorTopZ1);
        if (z >= kSepZ1 + 1 && z <= kSepZ2 - 1) return !(z == kDoorMidZ0 || z == kDoorMidZ1);
        return false;  // bande de la salle de musique : sol ouvert
    }

    // Separateur horizontal haut/milieu, avec une porte de chaque cote.
    if (z == kSepZ1) {
        if (x >= 1 && x <= kSepX - 1) return !(x == kDoorLeftX0 || x == kDoorLeftX1);
        if (x >= kSepX + 1 && x <= kHouseWidth - 2) return !(x == kDoorRightX0 || x == kDoorRightX1);
    }

    // Separateur horizontal milieu/salle de musique, une porte sous chaque piece.
    if (z == kSepZ2) {
        if (x >= 1 && x <= kHouseWidth - 2) {
            bool door = (x == kDoorLeftX0 || x == kDoorLeftX1 || x == kDoorRightX0 || x == kDoorRightX1);
            return !door;
        }
    }

    return false;
}

// Ray casting simple (Bresenham) : compte les murs traverses entre a et b,
// sans compter les cases de depart/arrivee elles-memes (cf. game_simulation.cpp).
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

enum Room { kRoomCuisine = 0, kRoomSalon, kRoomChambre, kRoomSalleDeBain, kRoomSalleMusique, kNumRooms };

const char* const kRoomNames[kNumRooms] = {
    "Cuisine", "Salon", "Chambre", "Salle de bain", "Salle de musique",
};
const char* const kRoomWavFiles[kNumRooms] = {
    "assets/test-audio.wav", "assets/test-audio2.wav", "assets/test-audio3.wav",
    "assets/test-audio4.wav", "assets/test-audio5.wav",
};
// Position fixe (dans le referentiel monde) de la source sonore de chaque piece.
const WorldPos kSourcePos[kNumRooms] = {
    {7, 3}, {22, 3}, {7, 10}, {22, 10}, {15, 17},
};

// Piece contenant la case (x,z), ou -1 si c'est un mur. Les cases de porte
// sont rattachees arbitrairement a l'une des deux pieces adjacentes (utilise
// uniquement pour choisir la reverb EFX du joueur - sans consequence sur le
// rendu spatial qui reste base sur les positions relatives + ray casting).
int roomAt(int x, int z) {
    if (isWall(x, z)) return -1;
    if (z <= kSepZ1) return (x <= kSepX - 1) ? kRoomCuisine : kRoomSalon;
    if (z <= kSepZ2) return (x <= kSepX - 1) ? kRoomChambre : kRoomSalleDeBain;
    return kRoomSalleMusique;
}

// Preset EFX associe a chaque piece (surfaces/volume approximatifs).
EFXEAXREVERBPROPERTIES presetForRoom(int room) {
    switch (room) {
        case kRoomCuisine: return EFXEAXREVERBPROPERTIES(EFX_REVERB_PRESET_STONEROOM);
        case kRoomSalon: return EFXEAXREVERBPROPERTIES(EFX_REVERB_PRESET_LIVINGROOM);
        case kRoomChambre: return EFXEAXREVERBPROPERTIES(EFX_REVERB_PRESET_WOODEN_SMALLROOM);
        case kRoomSalleDeBain: return EFXEAXREVERBPROPERTIES(EFX_REVERB_PRESET_BATHROOM);
        case kRoomSalleMusique: return EFXEAXREVERBPROPERTIES(EFX_REVERB_PRESET_PREFAB_PRACTISEROOM);
        default: return EFXEAXREVERBPROPERTIES(EFX_REVERB_PRESET_GENERIC);
    }
}

const char* presetNameForRoom(int room) {
    switch (room) {
        case kRoomCuisine: return "stoneroom";
        case kRoomSalon: return "livingroom";
        case kRoomChambre: return "wooden_smallroom";
        case kRoomSalleDeBain: return "bathroom";
        case kRoomSalleMusique: return "prefab_practiseroom";
        default: return "generic";
    }
}

std::string platformName() {
#if defined(__APPLE__)
    return "macOS";
#elif defined(_WIN32)
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#else
    return "Inconnue";
#endif
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
    alSourcef(source, AL_GAIN, 0.7f);
    return source;
}

// --- EFX (reverb) : charge dynamiquement, degradation gracieuse ----------

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

void printPadded(std::string line, size_t width = 78) {
    if (line.size() < width) line.append(width - line.size(), ' ');
    std::fputs(line.c_str(), stdout);
    std::fputc('\n', stdout);
}

char sourceDigitAt(int x, int z) {
    for (int i = 0; i < kNumRooms; ++i) {
        if (kSourcePos[i].x == x && kSourcePos[i].z == z) return static_cast<char>('1' + i);
    }
    return 0;
}

void renderFrame(const WorldPos& player, const std::string& statusLine1, const std::string& statusLine2,
                  const std::string& statusLine3) {
    std::fputs("\033[H", stdout);
    printPadded("=== NATHAN - Benchmark maison (5 pieces) ===");
    printPadded("Legende : @ joueur | 1 Cuisine 2 Salon 3 Chambre 4 SalleDeBain 5 SalleMusique | # mur | . sol");
    printPadded("");

    for (int z = 0; z < kHouseHeight; ++z) {
        std::string row;
        row.reserve(kHouseWidth);
        for (int x = 0; x < kHouseWidth; ++x) {
            if (x == player.x && z == player.z) {
                row += '@';
                continue;
            }
            char digit = sourceDigitAt(x, z);
            if (digit != 0) {
                row += digit;
            } else if (isWall(x, z)) {
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

// --- Statistiques (memes methodes que audio_benchmark.cpp) -----------------

double percentile(const std::vector<double>& sortedUs, double p) {
    if (sortedUs.empty()) return 0.0;
    size_t n = sortedUs.size();
    if (n == 1) return sortedUs[0];
    double rank = p * static_cast<double>(n - 1);
    size_t lower = static_cast<size_t>(std::floor(rank));
    size_t upper = static_cast<size_t>(std::ceil(rank));
    if (lower == upper) return sortedUs[lower];
    double frac = rank - static_cast<double>(lower);
    return sortedUs[lower] * (1.0 - frac) + sortedUs[upper] * frac;
}

struct FrameStats {
    size_t count = 0;
    double meanUs = 0.0;
    double medianUs = 0.0;
    double minUs = 0.0;
    double maxUs = 0.0;
    double stddevUs = 0.0;
    double p95Us = 0.0;
    double p99Us = 0.0;
    double totalMs = 0.0;
};

FrameStats computeStats(std::vector<double> frameTimesUs) {
    FrameStats s;
    s.count = frameTimesUs.size();
    if (s.count == 0) return s;

    double sum = 0.0;
    for (double v : frameTimesUs) sum += v;
    s.meanUs = sum / static_cast<double>(s.count);

    double variance = 0.0;
    for (double v : frameTimesUs) variance += (v - s.meanUs) * (v - s.meanUs);
    s.stddevUs = std::sqrt(variance / static_cast<double>(s.count));

    std::sort(frameTimesUs.begin(), frameTimesUs.end());
    s.minUs = frameTimesUs.front();
    s.maxUs = frameTimesUs.back();
    s.medianUs = percentile(frameTimesUs, 0.5);
    s.p95Us = percentile(frameTimesUs, 0.95);
    s.p99Us = percentile(frameTimesUs, 0.99);
    s.totalMs = sum / 1000.0;
    return s;
}

std::string buildReport(const FrameStats& stats, const AlHrtfDevice& alDevice, double sessionDurationSec,
                         bool efxReady) {
    std::ostringstream out;

    std::time_t now = std::time(nullptr);
    std::tm tmBuf{};
#if defined(_WIN32)
    localtime_s(&tmBuf, &now);
#else
    localtime_r(&now, &tmBuf);
#endif

    ALCint freq = 0;
    alcGetIntegerv(alDevice.device(), ALC_FREQUENCY, 1, &freq);

    double frameBudgetUs = std::chrono::duration<double, std::micro>(kFrameDuration).count();
    double cpuBudgetPercent = stats.count > 0 ? (stats.meanUs / frameBudgetUs) * 100.0 : 0.0;

    out << "=== NATHAN - Benchmark maison (5 pieces, interactif) ===\n";
    out << "Date                      : " << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S") << "\n";
    out << "Plateforme                : " << platformName() << "\n";
    out << "Profil HRTF               : " << HRTF_PROFILE << "\n";
    out << "Statut HRTF               : " << alDevice.hrtfStatusString() << "\n";
    out << "Renderer OpenAL           : " << alGetString(AL_RENDERER) << " " << alGetString(AL_VERSION)
        << " (" << alGetString(AL_VENDOR) << ")\n";
    out << "Frequence d'echantillonnage : " << freq << " Hz\n";
    out << "EFX (reverb)              : " << (efxReady ? "actif" : "indisponible (degradation gracieuse)") << "\n";
    out << "Sources simultanees       : " << kNumRooms
        << " (au-dela du minimum R-AUD-03 de 4 - charge superieure)\n";
    out << "Duree de la session       : " << std::fixed << std::setprecision(1) << sessionDurationSec << " s\n";
    out << "Budget par frame          : " << (frameBudgetUs / 1000.0) << " ms (~"
        << std::setprecision(1) << (1000.0 / (frameBudgetUs / 1000.0)) << " FPS)\n";
    out << std::defaultfloat;
    out << "\n--- Resultats (temps de traitement audio par frame) ---\n";
    out << "Frames traitees           : " << stats.count << "\n";
    out << std::fixed << std::setprecision(2);
    out << "Temps moyen                : " << stats.meanUs << " us\n";
    out << "Temps median                : " << stats.medianUs << " us\n";
    out << "Temps min                  : " << stats.minUs << " us\n";
    out << "Temps max                  : " << stats.maxUs << " us\n";
    out << "Ecart-type                 : " << stats.stddevUs << " us\n";
    out << "Percentile 95e              : " << stats.p95Us << " us\n";
    out << "Percentile 99e              : " << stats.p99Us << " us\n";
    out << "Temps CPU total (traitement audio) : " << stats.totalMs << " ms\n";
    out << "Budget temps reel utilise    : " << cpuBudgetPercent << " % (moyenne / " << (frameBudgetUs / 1000.0)
        << " ms)\n";

    out << "\n--- Contenu mesure a chaque frame ---\n";
    out << "Pour chacune des " << kNumRooms << " sources : position relative au joueur, ray casting\n";
    out << "Bresenham (comptage de murs traverses) pour l'attenuation inter-pieces, appel alSource3f/alSourcef,\n";
    out << "et selection de la reverb EFX de la piece du joueur (si changement de piece).\n";

    return out.str();
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

    ALuint buffers[kNumRooms] = {0, 0, 0, 0, 0};
    ALuint sources[kNumRooms] = {0, 0, 0, 0, 0};
    for (int i = 0; i < kNumRooms; ++i) {
        WavAudio wav;
        try {
            wav = load_wav_mono16(kRoomWavFiles[i]);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "Erreur de chargement WAV (%s) : %s\n", kRoomWavFiles[i], e.what());
            return 1;
        }
        buffers[i] = makeBufferFromWav(wav);
        sources[i] = makeLoopingSource(buffers[i]);
    }

    EfxApi efx = loadEfxApi(alDevice.device());
    ALuint effectSlot = 0;
    ALuint roomEffects[kNumRooms] = {0, 0, 0, 0, 0};
    bool efxReady = false;
    if (efx.available) {
        efx.alGenAuxiliaryEffectSlots(1, &effectSlot);
        for (int i = 0; i < kNumRooms; ++i) {
            roomEffects[i] = createReverbEffect(efx, presetForRoom(i));
        }
        for (int i = 0; i < kNumRooms; ++i) {
            alSource3i(sources[i], AL_AUXILIARY_SEND_FILTER, static_cast<ALint>(effectSlot), 0, AL_FILTER_NULL);
        }
        efxReady = true;
    }

    for (int i = 0; i < kNumRooms; ++i) alSourcePlay(sources[i]);

    std::printf("=== NATHAN - Benchmark maison (profil %s, HRTF: %s, EFX: %s) ===\n", HRTF_PROFILE,
                alDevice.hrtfStatusString().c_str(), efxReady ? "actif" : "indisponible");
    std::printf("Fleches : deplacer le joueur | Q ou Echap : quitter et sauvegarder le rapport\n");
    std::fflush(stdout);

    WorldPos player{4, 2};  // dans la Cuisine, a l'ecart de la source
    int lastRoom = -2;      // force la mise a jour de la reverb au premier tour

    std::vector<double> frameTimesUs;
    frameTimesUs.reserve(4096);
    double runningSum = 0.0;
    size_t runningCount = 0;
    double emaFps = 0.0;
    bool firstFrame = true;

    auto sessionStart = std::chrono::steady_clock::now();
    auto frameStartPrev = sessionStart;

    std::fputs("\033[2J\033[H", stdout);

    bool running = true;
    while (running) {
        auto frameStart = std::chrono::steady_clock::now();
        double periodSec = std::chrono::duration<double>(frameStart - frameStartPrev).count();
        frameStartPrev = frameStart;
        double instFps = periodSec > 0.0 ? 1.0 / periodSec : 0.0;
        if (firstFrame) {
            // Pas de frame precedente valide : on part de la cadence nominale
            // plutot que du delai (quasi nul) entre l'amorce et cette 1ere
            // mesure, ce qui eviterait un pic aberrant que l'EWMA mettrait
            // ensuite des dizaines de frames a resorber.
            emaFps = 1000.0 / static_cast<double>(kFrameDuration.count());
        } else {
            emaFps = emaFps * 0.9 + instFps * 0.1;
        }

        // --- Traitement audio mesure : positions + ray casting + EFX -----
        auto measureStart = std::chrono::high_resolution_clock::now();

        int currentRoom = roomAt(player.x, player.z);
        if (efxReady && currentRoom != lastRoom) {
            int roomForReverb = currentRoom >= 0 ? currentRoom : kRoomCuisine;
            efx.alAuxiliaryEffectSloti(effectSlot, AL_EFFECTSLOT_EFFECT,
                                       static_cast<ALint>(roomEffects[roomForReverb]));
            lastRoom = currentRoom;
        }

        for (int i = 0; i < kNumRooms; ++i) {
            int wallCount = countWallsBetween(player, kSourcePos[i]);
            float gain = std::max(0.1f, std::pow(0.5f, static_cast<float>(wallCount)));  // -6dB/mur, plancher 0.1

            float relX = static_cast<float>(kSourcePos[i].x - player.x) * kMetersPerCell;
            float relZ = static_cast<float>(kSourcePos[i].z - player.z) * kMetersPerCell;
            alSource3f(sources[i], AL_POSITION, relX, 0.0f, relZ);
            alSourcef(sources[i], AL_GAIN, gain);
        }

        auto measureEnd = std::chrono::high_resolution_clock::now();
        double frameUs = std::chrono::duration<double, std::micro>(measureEnd - measureStart).count();
        frameTimesUs.push_back(frameUs);
        runningSum += frameUs;
        ++runningCount;
        firstFrame = false;

        // --- Affichage ------------------------------------------------------
        char status1[96];
        std::snprintf(status1, sizeof(status1), "Piece du joueur : %-16s | Reverb : %s",
                      currentRoom >= 0 ? kRoomNames[currentRoom] : "?",
                      efxReady ? presetNameForRoom(currentRoom >= 0 ? currentRoom : kRoomCuisine) : "n/a");
        char status2[96];
        std::snprintf(status2, sizeof(status2), "Frame %6zu | Temps audio moyen (direct) : %7.1f us | FPS reel : %5.1f",
                      runningCount, runningSum / static_cast<double>(runningCount), emaFps);
        const std::string status3 = "Fleches : deplacer | Q / Echap : quitter et sauvegarder le rapport";

        renderFrame(player, status1, status2, status3);

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
            case KeyEvent::Escape:
            case KeyEvent::Quit:
                running = false;
                break;
            default:
                break;
        }

        std::this_thread::sleep_for(kFrameDuration);
    }

    double sessionDurationSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - sessionStart).count();

    for (int i = 0; i < kNumRooms; ++i) {
        alSourceStop(sources[i]);
        if (efxReady) {
            alSource3i(sources[i], AL_AUXILIARY_SEND_FILTER, 0, 0, AL_FILTER_NULL);
        }
        alDeleteSources(1, &sources[i]);
        alDeleteBuffers(1, &buffers[i]);
    }
    if (efxReady) {
        efx.alDeleteAuxiliaryEffectSlots(1, &effectSlot);
        for (int i = 0; i < kNumRooms; ++i) efx.alDeleteEffects(1, &roomEffects[i]);
    }

    FrameStats stats = computeStats(frameTimesUs);
    std::string report = buildReport(stats, alDevice, sessionDurationSec, efxReady);

    alDevice.close();

    std::printf("\n%s\n", report.c_str());

    std::ofstream resultsFile(kResultsPath, std::ios::out | std::ios::trunc);
    if (resultsFile) {
        resultsFile << report;
        std::printf("Rapport sauvegarde dans %s\n", kResultsPath);
    } else {
        std::fprintf(stderr, "Erreur : impossible d'ecrire %s\n", kResultsPath);
        return 1;
    }

    return 0;
}
