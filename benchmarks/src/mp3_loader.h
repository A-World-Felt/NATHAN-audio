#pragma once

#include <AL/al.h>

#include <cstdint>
#include <string>
#include <vector>

// Donnees PCM decodees depuis un fichier MP3 (16 bits signes, entrelacees
// si multi-canal).
struct Mp3Audio {
    std::vector<int16_t> samples;
    uint32_t sampleRate = 0;
    uint32_t channels = 0;
};

// Decode un fichier MP3 en PCM 16 bits via dr_mp3.
// Lance std::runtime_error en cas d'echec (fichier introuvable, decodage
// impossible, etc.).
Mp3Audio load_mp3(const std::string& path);

// Cree et remplit un buffer OpenAL a partir de donnees MP3 deja decodees.
// Choisit automatiquement AL_FORMAT_MONO16 ou AL_FORMAT_STEREO16 selon
// mp3.channels. Lance std::runtime_error si le nombre de canaux n'est ni
// 1 ni 2 (non supporte par le format PCM16 standard d'OpenAL).
ALuint makeBufferFromMp3(const Mp3Audio& mp3);
