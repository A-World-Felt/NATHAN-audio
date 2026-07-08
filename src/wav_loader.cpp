#include "wav_loader.h"

#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace {

uint32_t readU32(std::ifstream& f) {
    unsigned char b[4];
    f.read(reinterpret_cast<char*>(b), 4);
    return static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
           (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
}

uint16_t readU16(std::ifstream& f) {
    unsigned char b[2];
    f.read(reinterpret_cast<char*>(b), 2);
    return static_cast<uint16_t>(b[0]) | (static_cast<uint16_t>(b[1]) << 8);
}

}  // namespace

WavAudio load_wav_mono16(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw std::runtime_error("Impossible d'ouvrir le fichier WAV : " + path);
    }

    std::array<char, 4> riffId{};
    f.read(riffId.data(), 4);
    uint32_t riffSize = readU32(f);
    (void)riffSize;
    std::array<char, 4> waveId{};
    f.read(waveId.data(), 4);
    if (!f || std::strncmp(riffId.data(), "RIFF", 4) != 0 ||
        std::strncmp(waveId.data(), "WAVE", 4) != 0) {
        throw std::runtime_error("Fichier WAV invalide (en-tete RIFF/WAVE manquant) : " + path);
    }

    bool haveFmt = false;
    uint16_t audioFormat = 0, numChannels = 0, bitsPerSample = 0;
    uint32_t sampleRate = 0;
    std::vector<int16_t> rawSamples;
    bool haveData = false;

    while (f && !haveData) {
        std::array<char, 4> chunkId{};
        f.read(chunkId.data(), 4);
        if (!f) break;
        uint32_t chunkSize = readU32(f);
        if (!f) break;

        if (std::strncmp(chunkId.data(), "fmt ", 4) == 0) {
            std::streampos chunkStart = f.tellg();
            audioFormat = readU16(f);
            numChannels = readU16(f);
            sampleRate = readU32(f);
            readU32(f);  // byteRate
            readU16(f);  // blockAlign
            bitsPerSample = readU16(f);
            haveFmt = true;
            f.seekg(chunkStart + static_cast<std::streamoff>(chunkSize));
        } else if (std::strncmp(chunkId.data(), "data", 4) == 0) {
            rawSamples.resize(chunkSize / sizeof(int16_t));
            f.read(reinterpret_cast<char*>(rawSamples.data()), chunkSize);
            haveData = true;
        } else {
            f.seekg(chunkSize, std::ios::cur);
        }

        if (chunkSize % 2 != 0) {
            f.seekg(1, std::ios::cur);  // padding d'alignement RIFF
        }
    }

    if (!haveFmt || !haveData) {
        throw std::runtime_error("Fichier WAV incomplet (chunk fmt/data manquant) : " + path);
    }
    if (audioFormat != 1) {
        throw std::runtime_error("Fichier WAV non-PCM non supporte : " + path);
    }
    if (numChannels != 1) {
        throw std::runtime_error("Fichier WAV doit etre mono : " + path);
    }
    if (bitsPerSample != 16) {
        throw std::runtime_error("Fichier WAV doit etre en 16 bits : " + path);
    }

    WavAudio out;
    out.sampleRate = sampleRate;
    out.samples.resize(rawSamples.size());
    for (size_t i = 0; i < rawSamples.size(); ++i) {
        out.samples[i] = static_cast<float>(rawSamples[i]) / 32768.0f;
    }
    return out;
}
