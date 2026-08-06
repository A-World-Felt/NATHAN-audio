#include "profile_audition.h"

#include <AL/al.h>

#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>

#include "active_profile.h"
#include "hrtf_context.h"
#include "mp3_mono16.h"
#include "profile_settings.h"
#include "wav_mono16.h"

namespace nathan::hrtf {

namespace {

// Choisit le chargeur PCM selon l'extension du fichier audio (".mp3" ->
// dr_mp3, sinon WAV, comportement inchangee pour les .wav existants).
PcmMono16 loadTestAudio(const std::filesystem::path& path) {
    if (path.extension() == ".mp3") {
        return loadMp3Mono16(path);
    }
    return loadWavMono16(path);
}

// azimuthDeg : 0 = devant (-Z), +90 = droite (+X), -90 = gauche, 180 = derriere.
void setAzimuth(ALuint source, float azimuthDeg, float radiusMeters) {
    const float rad = azimuthDeg * 3.14159265358979323846f / 180.0f;
    alSource3f(source, AL_POSITION, std::sin(rad) * radiusMeters, 0.0f,
               -std::cos(rad) * radiusMeters);
}

// Buffer+source du son tournant. Recree a chaque appel de create() : les
// identifiants precedents deviennent invalides des que applyProfile()
// rouvre le contexte (voir active_profile.h), il ne faut donc jamais tenter
// de les liberer explicitement apres coup.
struct SpatialVoice {
    ALuint buffer = 0;
    ALuint source = 0;

    void create(const PcmMono16& wav) {
        alGenBuffers(1, &buffer);
        alBufferData(buffer, AL_FORMAT_MONO16, wav.samples.data(),
                     static_cast<ALsizei>(wav.samples.size() * sizeof(int16_t)),
                     static_cast<ALsizei>(wav.sampleRate));
        alGenSources(1, &source);
        alSourcei(source, AL_BUFFER, static_cast<ALint>(buffer));
        alSourcei(source, AL_LOOPING, AL_TRUE);
        alSourcePlay(source);
    }
};

double elapsedSeconds(const std::chrono::steady_clock::time_point& start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

}  // namespace

AuditionResult runProfileAudition(ProfileSelector& selector, AuditionInput& input,
                                   const AuditionConfig& config, std::FILE* out) {
    const PcmMono16 wav = loadTestAudio(config.testAudioPath);
    if (out) {
        const bool isMp3 = config.testAudioPath.extension() == ".mp3";
        const double durationSec =
            wav.sampleRate > 0 ? static_cast<double>(wav.samples.size()) / wav.sampleRate : 0.0;
        std::fprintf(out, "Fichier audio charge : %s (%s, mono, %d Hz, %zu echantillons, ~%.1f s)\n",
                     config.testAudioPath.filename().string().c_str(), isMp3 ? "MP3" : "WAV",
                     wav.sampleRate, wav.samples.size(), durationSec);
    }
    const std::string startingProfileId = selector.current().id;

    HrtfContext context;
    applyProfile(selector.current(), config.openalHrtfDir, context);

    SpatialVoice voice;
    voice.create(wav);
    if (out) {
        std::fprintf(out, "[Son tournant] Le son de test tourne autour de la tete. Profil actif : %s\n",
                     selector.current().id.c_str());
    }

    const auto t0 = std::chrono::steady_clock::now();

    while (true) {
        const double elapsed = elapsedSeconds(t0);
        const float azimuth =
            static_cast<float>(std::fmod(elapsed / config.rotationPeriodSec, 1.0) * 360.0);
        setAzimuth(voice.source, azimuth, config.radiusMeters);

        const AuditionCommand command = input.poll();

        if (command == AuditionCommand::Next || command == AuditionCommand::Previous) {
            const std::string previousId = selector.current().id;
            if (command == AuditionCommand::Next) {
                selector.next();
            } else {
                selector.previous();
            }

            try {
                applyProfile(selector.current(), config.openalHrtfDir, context);
                voice.create(wav);
                if (out) {
                    std::fprintf(out, "[Navigation] Profil actif : %s\n",
                                 selector.current().id.c_str());
                }
            } catch (const std::exception& e) {
                if (out) {
                    std::fprintf(out, "[Navigation] Echec de bascule vers %s (%s), repli sur %s\n",
                                 selector.current().id.c_str(), e.what(), previousId.c_str());
                }
                if (!selector.selectById(previousId)) {
                    throw std::runtime_error(
                        "Impossible de revenir au profil precedent '" + previousId +
                        "' apres l'echec de bascule : " + e.what());
                }
                // Ne pas capturer : si le repli echoue aussi, le device est
                // considere inutilisable et l'exception doit remonter.
                applyProfile(selector.current(), config.openalHrtfDir, context);
                voice.create(wav);
            }
        } else if (command == AuditionCommand::Confirm) {
            if (out) {
                std::fprintf(out, "[Confirmation] Confirmation du profil %s...\n",
                             selector.current().id.c_str());
            }
            saveSettings(AppSettings{selector.current().id}, config.settingsPath);
            if (out) {
                std::fprintf(out, "[Ecriture du choix] Profil sauvegarde dans settings.json : %s\n",
                             selector.current().id.c_str());
            }
            AuditionResult result;
            result.outcome = AuditionOutcome::Confirmed;
            result.activeProfileId = selector.current().id;
            result.persistedProfileId = selector.current().id;
            return result;
        } else if (command == AuditionCommand::Cancel) {
            if (selector.current().id != startingProfileId) {
                selector.selectById(startingProfileId);
                applyProfile(selector.current(), config.openalHrtfDir, context);
            }
            AuditionResult result;
            result.outcome = AuditionOutcome::Cancelled;
            result.activeProfileId = selector.current().id;
            result.persistedProfileId.clear();
            return result;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(config.frameIntervalMs));
    }
}

}  // namespace nathan::hrtf
