// Benchmark non-interactif du pipeline audio spatial HRTF (OpenAL Soft).
// Simule une scene NATHAN realiste (4 sources en mouvement continu, cf.
// R-AUD-03) pendant une duree fixe, mesure precisement le temps de
// traitement audio par frame de game loop, et produit un rapport de
// statistiques portable destine a etre reproduit sur les MPU candidats
// (ex: Raspberry Pi Zero 2W) pour guider le choix du processeur.
//
// Modifier ce nom de profil selon celui a comparer (cf. assets/hrtf/*.sofa).
#define HRTF_PROFILE "subject_003"

#include <AL/al.h>
#include <AL/alc.h>

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
#include "src/wav_loader.h"

namespace {

constexpr int kNumSources = 4;                                       // R-AUD-03
constexpr auto kBenchmarkDuration = std::chrono::seconds(30);
constexpr auto kFrameDuration = std::chrono::milliseconds(15);       // ~66 FPS
constexpr const char* kResultsPath = "benchmark_results.txt";

struct SourceMotion {
    float radiusMeters;
    double periodSec;      // temps pour un tour complet
    double phaseRad;
    float elevationAmplitude;
    double elevationPeriodSec;
};

// 4 trajectoires distinctes (rayon/vitesse/hauteur differents) pour eviter
// toute synchronisation artificielle entre sources, comme des acteurs de
// jeu independants se deplacant autour du joueur.
const SourceMotion kMotions[kNumSources] = {
    {3.0f, 9.0, 0.0, 0.0f, 0.0},
    {5.0f, 14.0, 1.5, 0.0f, 0.0},
    {2.0f, 6.0, 3.1, 1.5f, 5.0},
    {6.0f, 20.0, 4.7, 0.8f, 3.0},
};

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

// Position relative au joueur a l'instant gameTimeSec, sur une trajectoire
// circulaire avec une composante d'elevation optionnelle - meme contrat
// (relX,relY,relZ) -> alSource3f que le reste du projet (cf. game_simulation.cpp).
void computePosition(const SourceMotion& m, double gameTimeSec, float& x, float& y, float& z) {
    double theta = (gameTimeSec / m.periodSec) * 2.0 * 3.14159265358979323846 + m.phaseRad;
    x = m.radiusMeters * static_cast<float>(std::sin(theta));
    z = -m.radiusMeters * static_cast<float>(std::cos(theta));
    y = m.elevationAmplitude > 0.0f
            ? m.elevationAmplitude *
                  static_cast<float>(std::sin(gameTimeSec / m.elevationPeriodSec * 2.0 * 3.14159265358979323846))
            : 0.0f;
}

// Le "traitement audio" mesure : mise a jour des positions des kNumSources
// sources + les appels OpenAL (alSource3f) correspondants. C'est l'unique
// travail effectue a chaque frame de la game loop simulee.
void updateSourcePositions(const ALuint sources[kNumSources], double gameTimeSec) {
    for (int i = 0; i < kNumSources; ++i) {
        float x, y, z;
        computePosition(kMotions[i], gameTimeSec, x, y, z);
        alSource3f(sources[i], AL_POSITION, x, y, z);
    }
}

// --- Statistiques ----------------------------------------------------------

// Percentile par interpolation lineaire (methode standard, ex: numpy) ;
// p=0.5 donne la mediane, ce qui evite une fonction dediee.
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

std::string buildReport(const FrameStats& stats, const AlHrtfDevice& alDevice) {
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

    out << "=== NATHAN - Benchmark pipeline audio HRTF ===\n";
    out << "Date                    : " << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S") << "\n";
    out << "Plateforme               : " << platformName() << "\n";
    out << "Profil HRTF              : " << HRTF_PROFILE << "\n";
    out << "Statut HRTF               : " << alDevice.hrtfStatusString() << "\n";
    out << "Renderer OpenAL           : " << alGetString(AL_RENDERER) << " " << alGetString(AL_VERSION)
        << " (" << alGetString(AL_VENDOR) << ")\n";
    out << "Frequence d'echantillonnage : " << freq << " Hz\n";
    out << "Sources simultanees        : " << kNumSources << " (R-AUD-03)\n";
    out << "Duree simulee             : " << std::chrono::duration_cast<std::chrono::seconds>(kBenchmarkDuration).count()
        << " s\n";
    out << "Budget par frame           : " << frameBudgetUs / 1000.0 << " ms (~"
        << std::fixed << std::setprecision(1) << (1000.0 / (frameBudgetUs / 1000.0)) << " FPS)\n";
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

    out << "\n--- Reperes pour extrapolation MPU ---\n";
    out << "Echantillons rendus/source  : ~"
        << (static_cast<long long>(freq) *
            std::chrono::duration_cast<std::chrono::seconds>(kBenchmarkDuration).count())
        << " (" << freq << " Hz x " << std::chrono::duration_cast<std::chrono::seconds>(kBenchmarkDuration).count()
        << " s)\n";
    out << "Appels alSource3f/frame     : " << kNumSources << "\n";
    out << "Note                       : le nombre d'operations de convolution HRTF par\n";
    out << "                            echantillon depend de la longueur des filtres SOFA\n";
    out << "                            internes a OpenAL Soft (non mesurable depuis cette API).\n";
    out << "                            Comparer les MPU via le temps moyen/percentiles ci-dessus,\n";
    out << "                            mesures dans les memes conditions (meme profil, meme duree).\n";

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

    WavAudio wav;
    try {
        wav = load_wav_mono16("assets/test-audio.wav");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur de chargement WAV : %s\n", e.what());
        return 1;
    }

    ALuint buffer = makeBufferFromWav(wav);
    ALuint sources[kNumSources] = {0, 0, 0, 0};
    for (int i = 0; i < kNumSources; ++i) {
        sources[i] = makeLoopingSource(buffer);
        alSourcePlay(sources[i]);
    }

    std::printf("=== NATHAN - Benchmark pipeline audio HRTF (profil %s, HRTF: %s) ===\n", HRTF_PROFILE,
                alDevice.hrtfStatusString().c_str());
    std::printf("Simulation de %lld s avec %d sources, frame ~%lld ms...\n",
                static_cast<long long>(std::chrono::duration_cast<std::chrono::seconds>(kBenchmarkDuration).count()),
                kNumSources, static_cast<long long>(kFrameDuration.count()));
    std::fflush(stdout);

    std::vector<double> frameTimesUs;
    frameTimesUs.reserve(static_cast<size_t>(kBenchmarkDuration / kFrameDuration) + 1);

    auto benchStart = std::chrono::steady_clock::now();
    long long frameIndex = 0;
    while (true) {
        auto frameSchedule = benchStart + frameIndex * kFrameDuration;
        if (frameSchedule - benchStart >= kBenchmarkDuration) break;

        double gameTimeSec = std::chrono::duration<double>(frameSchedule - benchStart).count();

        auto measureStart = std::chrono::high_resolution_clock::now();
        updateSourcePositions(sources, gameTimeSec);
        auto measureEnd = std::chrono::high_resolution_clock::now();

        frameTimesUs.push_back(std::chrono::duration<double, std::micro>(measureEnd - measureStart).count());

        ++frameIndex;
        std::this_thread::sleep_until(benchStart + frameIndex * kFrameDuration);
    }

    for (int i = 0; i < kNumSources; ++i) {
        alSourceStop(sources[i]);
        alDeleteSources(1, &sources[i]);
    }
    alDeleteBuffers(1, &buffer);

    FrameStats stats = computeStats(frameTimesUs);
    std::string report = buildReport(stats, alDevice);

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
