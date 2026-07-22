#include "hrtf_profile.h"

#include <mysofa.h>

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace fs = std::filesystem;

namespace hrtf_profile {

std::vector<fs::path> listProfiles(const fs::path& hrtfDir) {
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(hrtfDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".sofa") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

fs::path activeProfileDestPath() {
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (!appData) {
        throw std::runtime_error("Variable d'environnement APPDATA introuvable");
    }
    return fs::path(appData) / "OpenAL" / "hrtf" / "current.sofa";
#else
    const char* xdgDataHome = std::getenv("XDG_DATA_HOME");
    fs::path base;
    if (xdgDataHome && *xdgDataHome) {
        base = fs::path(xdgDataHome);
    } else {
        const char* home = std::getenv("HOME");
        if (!home) {
            throw std::runtime_error("Variable d'environnement HOME introuvable");
        }
        base = fs::path(home) / ".local" / "share";
    }
    return base / "openal" / "hrtf" / "current.sofa";
#endif
}

void installProfile(const fs::path& sofaPath) {
    int err = MYSOFA_OK;
    MYSOFA_HRTF* h = mysofa_load(sofaPath.string().c_str(), &err);
    if (!h || err != MYSOFA_OK) {
        if (h) mysofa_free(h);
        throw std::runtime_error("Fichier SOFA invalide : " + sofaPath.string());
    }
    mysofa_free(h);

    fs::path dest = activeProfileDestPath();
    fs::create_directories(dest.parent_path());

    // Ne garder qu'un seul fichier .sofa dans le dossier : sinon OpenAL
    // Soft peut enumerer plusieurs specifiers et ALC_HRTF_ID_SOFT=0 ne
    // designerait plus forcement le profil qu'on vient d'installer.
    for (const auto& entry : fs::directory_iterator(dest.parent_path())) {
        if (entry.is_regular_file() && entry.path().extension() == ".sofa") {
            fs::remove(entry.path());
        }
    }

    fs::copy_file(sofaPath, dest, fs::copy_options::overwrite_existing);
}

}  // namespace hrtf_profile
