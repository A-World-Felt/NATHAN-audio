#include "mp3_mono16.h"

#include <stdexcept>

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

namespace nathan::hrtf {

PcmMono16 loadMp3Mono16(const std::filesystem::path& path) {
    drmp3_config config;
    drmp3_uint64 totalFrameCount = 0;

    drmp3_int16* pcm = drmp3_open_file_and_read_pcm_frames_s16(
        path.string().c_str(), &config, &totalFrameCount, nullptr);

    if (pcm == nullptr) {
        throw std::runtime_error("Echec du decodage MP3 : " + path.string());
    }

    if (config.channels != 1) {
        drmp3_free(pcm, nullptr);
        throw std::runtime_error("Fichier MP3 doit etre mono : " + path.string());
    }

    PcmMono16 out;
    out.sampleRate = static_cast<int>(config.sampleRate);
    out.samples.assign(pcm, pcm + totalFrameCount);

    drmp3_free(pcm, nullptr);
    return out;
}

}  // namespace nathan::hrtf
