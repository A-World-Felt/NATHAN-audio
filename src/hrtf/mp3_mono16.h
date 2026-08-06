#pragma once

// Chargeur MP3, decode en PCM mono 16 bits via dr_mp3 (voir dr_mp3.h,
// vendore tel quel). Meme sortie que wav_mono16.h : les deux chargeurs sont
// interchangeables partout ou PcmMono16 est deja consomme.

#include <filesystem>

#include "wav_mono16.h"

namespace nathan::hrtf {

// Leve std::runtime_error si le fichier est absent/illisible, non-MP3, ou
// non-mono.
PcmMono16 loadMp3Mono16(const std::filesystem::path& path);

}  // namespace nathan::hrtf
