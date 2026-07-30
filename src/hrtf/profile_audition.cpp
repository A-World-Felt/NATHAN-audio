#include "profile_audition.h"

#include <AL/al.h>

#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>

#include "active_profile.h"
#include "hrtf_context.h"
#include "profile_settings.h"
#include "wav_mono16.h"

namespace nathan::hrtf {

namespace {

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
    const PcmMono16 wav = loadWavMono16(config.testWavPath);
    const std::string startingProfileId = selector.current().id;

    HrtfContext context;
    applyProfile(selector.current(), config.openalHrtfDir, context);

    SpatialVoice voice;
    voice.create(wav);
    if (out) {
        std::fprintf(out, "Profil actif : %s\n", selector.current().id.c_str());
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
                    std::fprintf(out, "Profil actif : %s\n", selector.current().id.c_str());
                }
            } catch (const std::exception& e) {
                if (out) {
                    std::fprintf(out, "Echec de bascule vers %s (%s), repli sur %s\n",
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
            saveSettings(AppSettings{selector.current().id}, config.settingsPath);
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
