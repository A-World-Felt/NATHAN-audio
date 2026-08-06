// Interface d'ecoute interactive du profil HRTF (DEV-239) : un son de test
// tourne autour de la tete, la navigation clavier bascule le HRTF a chaud
// entre profils, la confirmation persiste le choix dans settings.json
// (DEV-238). Dernier maillon de la chaine DEV-167 -> DEV-238 -> DEV-239.
//
// A executer depuis la racine du depot (lit assets/hrtf/ et le fichier audio
// de test ci-dessous en chemin relatif) :
//   .\build\Debug\hrtf_profile_listen.exe
//
// AuditionConfig::testAudioPath bascule automatiquement entre wav_mono16.h
// et mp3_mono16.h selon l'extension (voir profile_audition.cpp). Fichier de
// demo MP3 par defaut ci-dessous ; remplacer par un ".wav" pour revenir a
// l'ancien chargeur.
//
// Commandes : fleche gauche/droite pour naviguer, Entree pour confirmer,
// Echap ou Q pour abandonner.
// Code de sortie : 0 = profil confirme, 2 = abandonne, 1 = erreur.

#include <cstdio>
#include <exception>
#include <filesystem>
#include <vector>

#include "hrtf/audition_input.h"
#include "hrtf/config_paths.h"
#include "hrtf/openal_paths.h"
#include "hrtf/profile_audition.h"
#include "hrtf/profile_catalog.h"
#include "hrtf/profile_selector.h"
#include "hrtf/profile_settings.h"

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
    if (profiles.empty()) {
        std::fprintf(stderr,
                      "Aucun profil .sofa trouve dans %s : voir README.md pour telecharger les "
                      "profils CIPIC.\n",
                      assetsHrtfDir.string().c_str());
        return 1;
    }
    std::printf("[Decouverte] %zu profil(s) trouve(s) dans %s\n", profiles.size(),
                assetsHrtfDir.string().c_str());

    AuditionConfig config;
    config.testAudioPath = "assets/test-audio.mp3";

    try {
        config.openalHrtfDir = openalHrtfDirectory();
        config.settingsPath = nathanConfigDirectory() / "settings.json";
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur de resolution des chemins : %s\n", e.what());
        return 1;
    }

    StartupResolution resolution;
    try {
        resolution = resolveStartupProfile(profiles, config.settingsPath);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Erreur de resolution du profil : %s\n", e.what());
        return 1;
    }
    if (resolution.usedFallback) {
        std::printf("[Profil par defaut] Repli sur profil par defaut : %s\n",
                    resolution.fallbackReason.c_str());
    } else {
        std::printf("[Relecture] Profil relu depuis settings.json : %s\n",
                    resolution.profile.id.c_str());
    }

    ProfileSelector selector(profiles, resolution.profile.id);
    KeyboardAuditionInput input;

    std::printf("\n=== NATHAN - Ecoute et selection du profil HRTF ===\n");
    std::printf("Fleche gauche : profil precedent | Fleche droite : profil suivant\n");
    std::printf("Entree : confirmer | Echap ou Q : abandonner\n\n");

    AuditionResult result;
    try {
        result = runProfileAudition(selector, input, config);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "\nErreur d'ecoute HRTF : %s\n", e.what());
        return 1;
    }

    if (result.outcome == AuditionOutcome::Confirmed) {
        std::printf("\nProfil confirme et sauvegarde : %s\n", result.persistedProfileId.c_str());
        return 0;
    }

    std::printf("\nSelection abandonnee, profil actif restaure : %s\n",
                result.activeProfileId.c_str());
    return 2;
}
