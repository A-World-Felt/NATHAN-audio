#include "profile_installer.h"

#include <stdexcept>
#include <system_error>

#include "sofa_validator.h"

namespace fs = std::filesystem;

namespace nathan::hrtf {

void installProfile(const HrtfProfile& profile, const fs::path& destDir) {
    SofaValidationResult validation = validateSofaFile(profile.path);
    if (!validation.ok) {
        throw std::runtime_error("Profil HRTF invalide, installation annulee : " +
                                  validation.error);
    }

    std::error_code ec;
    fs::create_directories(destDir, ec);
    if (ec && !fs::is_directory(destDir)) {
        throw std::runtime_error("Dossier de destination inaccessible (creation) : " +
                                  destDir.string() + " (" + ec.message() + ")");
    }

    for (const auto& entry : fs::directory_iterator(destDir, ec)) {
        if (!entry.is_regular_file()) continue;
        const auto ext = entry.path().extension();
        if (ext != ".sofa" && ext != ".mhr") continue;
        std::error_code removeEc;
        fs::remove(entry.path(), removeEc);
        if (removeEc) {
            throw std::runtime_error("Dossier de destination inaccessible (suppression de " +
                                      entry.path().string() + ") : " + removeEc.message());
        }
    }
    if (ec) {
        throw std::runtime_error("Dossier de destination inaccessible (lecture) : " +
                                  destDir.string() + " (" + ec.message() + ")");
    }

    const fs::path dest = destDir / profile.path.filename();
    fs::copy_file(profile.path, dest, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        throw std::runtime_error("Echec de la copie vers " + dest.string() + " (" +
                                  ec.message() + ")");
    }
}

}  // namespace nathan::hrtf
