#include "mp3_loader.h"

#include <stdexcept>

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

Mp3Audio load_mp3(const std::string& path) {
    drmp3_config config;
    drmp3_uint64 totalFrameCount = 0;

    drmp3_int16* pcm = drmp3_open_file_and_read_pcm_frames_s16(
        path.c_str(), &config, &totalFrameCount, nullptr);

    if (pcm == nullptr) {
        throw std::runtime_error("Echec du decodage MP3 : " + path);
    }

    Mp3Audio audio;
    audio.sampleRate = config.sampleRate;
    audio.channels = config.channels;
    audio.samples.assign(pcm, pcm + totalFrameCount * config.channels);

    drmp3_free(pcm, nullptr);
    return audio;
}

ALuint makeBufferFromMp3(const Mp3Audio& mp3) {
    ALenum format;
    if (mp3.channels == 1) {
        format = AL_FORMAT_MONO16;
    } else if (mp3.channels == 2) {
        format = AL_FORMAT_STEREO16;
    } else {
        throw std::runtime_error(
            "Nombre de canaux MP3 non supporte par PCM16 OpenAL : " +
            std::to_string(mp3.channels));
    }

    ALsizei dataSize = static_cast<ALsizei>(mp3.samples.size() * sizeof(int16_t));

    ALuint buffer = 0;
    alGenBuffers(1, &buffer);
    alBufferData(buffer, format, mp3.samples.data(), dataSize,
                 static_cast<ALsizei>(mp3.sampleRate));
    return buffer;
}
