#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct WavAudio {
    std::vector<float> samples;  // normalise [-1,1], mono
    uint32_t sampleRate = 0;
};

// Parcours RIFF generique (chunks ignores hors "fmt "/"data"). Leve
// std::runtime_error si le fichier n'est pas PCM mono 16 bits.
WavAudio load_wav_mono16(const std::string& path);
