// Programme de verification independant du module src/hrtf (DEV-167).
//
// Liste les profils HRTF CIPIC trouves dans assets/hrtf/, installe le
// premier dans le dossier scanne par OpenAL Soft, ouvre un contexte avec
// HRTF force et affiche le statut retourne par le pilote. Ne depend
// d'aucun autre programme du depot : sert uniquement a valider que
// decouverte + validation + installation + activation fonctionnent.
//
// A executer depuis la racine du depot (lit assets/hrtf/ en chemin relatif) :
//   .\build\Debug\hrtf_check.exe

#include <cstdio>
#include <exception>
#include <filesystem>
#include <vector>

#include "hrtf/active_profile.h"
#include "hrtf/hrtf_context.h"
#include "hrtf/openal_paths.h"
#include "hrtf/profile_catalog.h"

int main() {
    using namespace nathan::hrtf;

    const std::filesystem::path assetsHrtfDir = "assets/hrtf";

    std::vector<HrtfProfile> profiles;
    try {
        profiles = discoverProfiles(assetsHrtfDir);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur de decouverte : %s\n", e.what());
        return 1;
    }

    std::printf("%zu profil(s) trouve(s) dans %s :\n", profiles.size(),
                assetsHrtfDir.string().c_str());
    for (const auto& profile : profiles) {
        std::printf("  - %s\n", profile.id.c_str());
    }

    if (profiles.empty()) {
        std::fprintf(stderr,
                      "Aucun profil .sofa trouve : voir README.md pour telecharger les "
                      "profils CIPIC dans assets/hrtf/.\n");
        return 1;
    }

    const HrtfProfile& chosen = profiles.front();
    std::printf("\nProfil choisi : %s\n", chosen.id.c_str());

    HrtfContext ctx;
    try {
        const std::filesystem::path destDir = openalHrtfDirectory();
        applyProfile(chosen, destDir, ctx);
        std::printf("OK : HRTF actif avec le profil %s.\n", chosen.id.c_str());
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur d'application du profil : %s\n", e.what());
        return 1;
    }

    std::printf("\n=== Resume ===\n");
    std::printf("Profils trouves : %zu | Profil installe : %s | Statut HRTF : %s\n",
                profiles.size(), chosen.id.c_str(), toString(ctx.status()).c_str());

    return 0;
}
