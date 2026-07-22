#include "profile_catalog.h"

#include <algorithm>
#include <stdexcept>
#include <system_error>

namespace fs = std::filesystem;

namespace nathan::hrtf {

std::vector<HrtfProfile> discoverProfiles(const fs::path& hrtfAssetsDir) {
    std::error_code ec;
    if (!fs::is_directory(hrtfAssetsDir, ec)) {
        throw std::runtime_error(
            "Dossier de profils HRTF introuvable : " + hrtfAssetsDir.string());
    }

    std::vector<HrtfProfile> profiles;
    for (const auto& entry : fs::directory_iterator(hrtfAssetsDir, ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".sofa") continue;
        profiles.push_back(HrtfProfile{entry.path().stem().string(), entry.path()});
    }
    if (ec) {
        throw std::runtime_error(
            "Impossible de lire le dossier de profils HRTF : " + hrtfAssetsDir.string() +
            " (" + ec.message() + ")");
    }

    std::sort(profiles.begin(), profiles.end(),
              [](const HrtfProfile& a, const HrtfProfile& b) { return a.id < b.id; });
    return profiles;
}

}  // namespace nathan::hrtf
