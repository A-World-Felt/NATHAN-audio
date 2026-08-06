#include "active_profile.h"

#include <cstdio>

#include "profile_installer.h"

namespace nathan::hrtf {

void applyProfile(const HrtfProfile& profile, const std::filesystem::path& openalHrtfDir,
                   HrtfContext& context) {
    std::printf("[Validation] Validation SOFA du profil %s via libmysofa...\n",
                profile.id.c_str());
    // installProfile valide le fichier (validateSofaFile) avant de le copier :
    // si l'appel suivant ne leve pas, la validation a reussi.
    installProfile(profile, openalHrtfDir);
    std::printf(
        "[Validation] Fichier .sofa valide (libmysofa) : format SOFA reconnu, donnees HRTF "
        "conformes.\n");
    std::printf("[Installation] Installe dans : %s\n", openalHrtfDir.string().c_str());

    std::printf("[Activation HRTF] Ouverture du device audio avec HRTF force...\n");
    context.open();
    const HrtfStatus status = context.status();
    if (status == HrtfStatus::Enabled) {
        std::printf("[Activation HRTF] Statut HRTF : %s (ALC_HRTF_ENABLED_SOFT)\n",
                    toString(status).c_str());
    } else {
        std::printf("[Activation HRTF] Statut HRTF : %s\n", toString(status).c_str());
    }
}

}  // namespace nathan::hrtf
