#include "openal_paths.h"

#include <cstdlib>
#include <stdexcept>

namespace fs = std::filesystem;

namespace nathan::hrtf {

std::filesystem::path openalHrtfDirectory() {
#if defined(_WIN32)
    const char* appData = std::getenv("APPDATA");
    if (!appData || !*appData) {
        throw std::runtime_error("Variable d'environnement APPDATA introuvable");
    }
    return fs::path(appData) / "OpenAL" / "hrtf";
#else
    const char* xdgDataHome = std::getenv("XDG_DATA_HOME");
    if (xdgDataHome && *xdgDataHome) {
        return fs::path(xdgDataHome) / "openal" / "hrtf";
    }
    const char* home = std::getenv("HOME");
    if (!home || !*home) {
        throw std::runtime_error("Variable d'environnement HOME introuvable");
    }
    return fs::path(home) / ".local" / "share" / "openal" / "hrtf";
#endif
}

}  // namespace nathan::hrtf
