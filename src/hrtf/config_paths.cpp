#include "config_paths.h"

#include <cstdlib>
#include <stdexcept>

namespace fs = std::filesystem;

namespace nathan::hrtf {

std::filesystem::path nathanConfigDirectory() {
#if defined(_WIN32)
    const char* appData = std::getenv("APPDATA");
    if (!appData || !*appData) {
        throw std::runtime_error("Variable d'environnement APPDATA introuvable");
    }
    return fs::path(appData) / "NATHAN";
#else
    const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfigHome && *xdgConfigHome) {
        return fs::path(xdgConfigHome) / "nathan";
    }
    const char* home = std::getenv("HOME");
    if (!home || !*home) {
        throw std::runtime_error("Variable d'environnement HOME introuvable");
    }
    return fs::path(home) / ".config" / "nathan";
#endif
}

}  // namespace nathan::hrtf
