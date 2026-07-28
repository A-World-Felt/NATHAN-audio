#pragma once

// Boucle d'ecoute interactive pour choisir un profil HRTF a l'oreille
// (DEV-239) : un son de test tourne en continu autour de la tete de
// l'auditeur, la navigation entre profils bascule le HRTF a chaud (voir
// active_profile.h), et la confirmation persiste le choix via
// profile_settings.h (DEV-238). La console NATHAN s'adresse a des personnes
// malvoyantes : ce module ne depend d'aucun affichage graphique.

#include <cstdio>
#include <filesystem>
#include <string>

#include "audition_input.h"
#include "profile_selector.h"

namespace nathan::hrtf {

struct AuditionConfig {
    std::filesystem::path testWavPath;    // son de test, mono 16/24 bits (voir wav_mono16.h)
    std::filesystem::path openalHrtfDir;  // dossier hrtf-paths d'OpenAL Soft (openal_paths.h)
    std::filesystem::path settingsPath;   // fichier de persistance (profile_settings.h)
    double rotationPeriodSec = 6.0;       // duree d'un tour complet du cercle
    float radiusMeters = 1.5f;
    int frameIntervalMs = 20;
};

enum class AuditionOutcome {
    Confirmed,
    Cancelled,
};

struct AuditionResult {
    AuditionOutcome outcome;
    std::string activeProfileId;     // profil audible au moment de la sortie de la boucle
    std::string persistedProfileId;  // profil ecrit dans settings.json (vide si Cancelled)
};

// Boucle bloquante jusqu'a une commande Confirm ou Cancel de input.
//
// Au demarrage, installe et active selector.current() via applyProfile.
// Leve std::runtime_error si meme ce profil de depart echoue a activer le
// HRTF : le device est alors considere inutilisable, il n'y a rien a
// proposer a l'utilisateur.
//
// A chaque frame (cadence config.frameIntervalMs) : la position du son de
// test tourne suivant le temps ecoulu depuis le debut de la boucle
// (steady_clock) sur une periode de config.rotationPeriodSec secondes ; la
// phase de rotation n'est donc jamais reinitialisee par une bascule de
// profil. azimuth 0 = devant (-Z), +90 = droite (+X), auditeur a l'origine.
//
// Puis poll() de input est consulte :
//  - Next/Previous : avance/recule selector, puis applyProfile sur le
//    nouveau profil et recree le son (buffer+source, invalides apres
//    l'appel a HrtfContext::open() fait par applyProfile). Si applyProfile
//    echoue, revient au profil precedent (selector.selectById + nouvel
//    appel a applyProfile) et continue la boucle sans l'interrompre ; si ce
//    repli echoue lui-meme, leve std::runtime_error (device inutilisable).
//  - Confirm : sauvegarde selector.current().id dans config.settingsPath
//    via saveSettings, retourne AuditionOutcome::Confirmed.
//  - Cancel : n'ecrit rien dans settings.json ; si le profil actif differe
//    du profil de depart, reinstalle ce dernier cote OpenAL (coherence du
//    dossier hrtf-paths avec ce qu'annoncait le demarrage) ; retourne
//    AuditionOutcome::Cancelled.
//
// Messages de progression (bascule de profil, replis) ecrits sur out ; out
// peut etre nullptr pour les desactiver.
AuditionResult runProfileAudition(ProfileSelector& selector, AuditionInput& input,
                                   const AuditionConfig& config, std::FILE* out = stdout);

}  // namespace nathan::hrtf
