#pragma once

// Chargeur WAV PCM mono 16/24 bits, sortie directe en int16 pret pour un
// buffer OpenAL AL_FORMAT_MONO16 (contrairement a benchmarks/src/wav_loader.h
// qui normalise en float).

#include <cstdint>
#include <filesystem>
#include <vector>

namespace nathan::hrtf {

struct PcmMono16 {
    std::vector<int16_t> samples;
    int sampleRate = 0;
};

// Parcours RIFF generique (chunks ignores hors "fmt "/"data"). Leve
// std::runtime_error si le fichier est absent/illisible, non-PCM, non-mono,
// ou pas en 16/24 bits.
PcmMono16 loadWavMono16(const std::filesystem::path& path);

}  // namespace nathan::hrtf
