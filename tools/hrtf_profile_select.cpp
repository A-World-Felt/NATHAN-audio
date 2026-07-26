// Programme de verification independant du module de selection/persistance
// du profil HRTF (DEV-238).
//
// Sans argument : resout le profil actif (fichier de configuration ou
// repli par defaut) et l'affiche, avec la liste du catalogue.
// Avec un argument <id> : selectionne ce profil et sauvegarde le choix.
// Ne depend d'aucun autre programme du depot ; sert uniquement a valider
// le cycle complet selection -> persistance -> relecture.
//
// A executer depuis la racine du depot (lit assets/hrtf/ en chemin relatif) :
//   .\build\Debug\hrtf_profile_select.exe
//   .\build\Debug\hrtf_profile_select.exe subject_008

#include <cstdio>
#include <exception>
#include <filesystem>
#include <vector>

#include "hrtf/config_paths.h"
#include "hrtf/profile_catalog.h"
#include "hrtf/profile_selector.h"
#include "hrtf/profile_settings.h"

int main(int argc, char** argv) {
    using namespace nathan::hrtf;

    const std::filesystem::path assetsHrtfDir = "assets/hrtf";

    std::vector<HrtfProfile> profiles;
    try {
        profiles = discoverProfiles(assetsHrtfDir);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur de decouverte : %s\n", e.what());
        return 1;
    }

    if (profiles.empty()) {
        std::fprintf(stderr,
                      "Aucun profil .sofa trouve dans %s : voir README.md pour telecharger les "
                      "profils CIPIC.\n",
                      assetsHrtfDir.string().c_str());
        return 1;
    }

    std::filesystem::path settingsPath;
    try {
        settingsPath = nathanConfigDirectory() / "settings.json";
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur de resolution du dossier de configuration : %s\n", e.what());
        return 1;
    }

    StartupResolution resolution;
    try {
        resolution = resolveStartupProfile(profiles, settingsPath);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur de resolution du profil : %s\n", e.what());
        return 1;
    }

    if (resolution.usedFallback) {
        std::printf("Repli sur profil par defaut : %s\n", resolution.fallbackReason.c_str());
    }

    ProfileSelector selector(profiles, resolution.profile.id);

    std::printf("Profil actif : %s\n", selector.current().id.c_str());
    std::printf("Fichier de configuration : %s\n", settingsPath.string().c_str());
    std::printf("Profils disponibles (%zu) :\n", profiles.size());
    for (const auto& profile : profiles) {
        const char* marker = (profile.id == selector.current().id) ? "*" : " ";
        std::printf("  %s %s\n", marker, profile.id.c_str());
    }

    if (argc < 2) {
        return 0;
    }

    const std::string requestedId = argv[1];
    if (!selector.selectById(requestedId)) {
        std::fprintf(stderr, "\nProfil '%s' introuvable dans le catalogue, aucune modification.\n",
                      requestedId.c_str());
        return 1;
    }

    try {
        saveSettings(AppSettings{selector.current().id}, settingsPath);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "\nErreur de sauvegarde : %s\n", e.what());
        return 1;
    }

    std::printf("\nProfil selectionne et sauvegarde : %s\n", selector.current().id.c_str());
    return 0;
}
