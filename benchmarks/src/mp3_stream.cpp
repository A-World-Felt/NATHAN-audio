#include "mp3_stream.h"

#include <vector>

namespace {

// Decode un chunk dans le buffer OpenAL donne. Gere le bouclage a l'EOF.
// Retourne false si rien n'a pu etre decode (fin de flux sans bouclage, ou
// fichier vide/corrompu).
bool fillBuffer(Mp3Stream& stream, ALuint buffer) {
    std::vector<drmp3_int16> pcm(static_cast<size_t>(kStreamFramesPerChunk) * stream.channels);

    drmp3_uint64 framesRead =
        drmp3_read_pcm_frames_s16(&stream.decoder, kStreamFramesPerChunk, pcm.data());

    if (framesRead == 0) {
        if (!stream.loop) {
            stream.finished = true;
            return false;
        }
        // EOF atteint : on revient au debut et on retente une fois.
        drmp3_seek_to_pcm_frame(&stream.decoder, 0);
        framesRead = drmp3_read_pcm_frames_s16(&stream.decoder, kStreamFramesPerChunk, pcm.data());
        if (framesRead == 0) {
            // Fichier vide ou probleme de decodage : on abandonne pour
            // eviter une boucle infinie de seeks.
            stream.finished = true;
            return false;
        }
    }

    ALenum format = (stream.channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
    ALsizei dataSize =
        static_cast<ALsizei>(framesRead * stream.channels * sizeof(drmp3_int16));
    alBufferData(buffer, format, pcm.data(), dataSize, static_cast<ALsizei>(stream.sampleRate));
    return true;
}

}  // namespace

bool mp3_stream_open(Mp3Stream& stream, const std::string& path, bool loop) {
    if (!drmp3_init_file(&stream.decoder, path.c_str(), nullptr)) {
        return false;
    }
    stream.decoderOpen = true;
    stream.channels = stream.decoder.channels;
    stream.sampleRate = stream.decoder.sampleRate;
    stream.loop = loop;
    stream.finished = false;

    alGenBuffers(kStreamBufferCount, stream.buffers);
    return true;
}

void mp3_stream_start(Mp3Stream& stream, ALuint source) {
    ALuint toQueue[kStreamBufferCount];
    int queuedCount = 0;

    for (int i = 0; i < kStreamBufferCount; ++i) {
        if (fillBuffer(stream, stream.buffers[i])) {
            toQueue[queuedCount++] = stream.buffers[i];
        }
    }

    if (queuedCount > 0) {
        alSourceQueueBuffers(source, queuedCount, toQueue);
        alSourcePlay(source);
    }
}

void mp3_stream_update(Mp3Stream& stream, ALuint source) {
    if (stream.finished) return;

    ALint processed = 0;
    alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);

    while (processed-- > 0) {
        ALuint buffer = 0;
        alSourceUnqueueBuffers(source, 1, &buffer);
        if (fillBuffer(stream, buffer)) {
            alSourceQueueBuffers(source, 1, &buffer);
        }
    }

    // Si la source s'est arretee (buffer underrun) mais qu'il reste des
    // buffers en file, on relance la lecture au lieu de rester silencieux.
    ALint state = 0;
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    ALint stillQueued = 0;
    alGetSourcei(source, AL_BUFFERS_QUEUED, &stillQueued);
    if (state != AL_PLAYING && stillQueued > 0) {
        alSourcePlay(source);
    }
}

void mp3_stream_close(Mp3Stream& stream) {
    if (stream.decoderOpen) {
        drmp3_uninit(&stream.decoder);
        stream.decoderOpen = false;
    }
    alDeleteBuffers(kStreamBufferCount, stream.buffers);
}
