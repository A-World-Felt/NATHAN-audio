#pragma once

// Persistance du choix de profil HRTF utilisateur en JSON, et resolution du
// profil a charger au demarrage avec repli robuste. S'appuie sur
// profile_catalog (liste des profils) et sofa_validator (validite d'un
// .sofa) sans reimplementer leur logique.

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "profile_catalog.h"

namespace nathan::hrtf {

// Reglages utilisateur persistes. Un seul champ pour l'instant ; struct
// concue pour accueillir de futurs reglages audio (volume, reverberation).
struct AppSettings {
    std::string hrtfProfile;  // vide si non defini
};

// Lit settingsPath. Retourne std::nullopt si le fichier n'existe pas, n'est
// pas lisible (permissions), si le JSON est syntaxiquement invalide, ou si
// sa forme ne correspond pas a AppSettings (pas un objet, cle hrtf_profile
// absente ou pas une chaine). Ne leve jamais : ce sont tous des etats
// attendus geres par l'appelant via le repli (voir resolveStartupProfile).
std::optional<AppSettings> loadSettings(const std::filesystem::path& settingsPath);

// Ecrit settings dans settingsPath de facon atomique (fichier temporaire
// dans le meme dossier, puis rename). Cree le dossier parent si necessaire.
// Leve std::runtime_error si le dossier est inaccessible en ecriture ou si
// l'ecriture/le renommage echoue.
void saveSettings(const AppSettings& settings, const std::filesystem::path& settingsPath);

struct StartupResolution {
    HrtfProfile profile;
    bool usedFallback = false;
    std::string fallbackReason;  // vide si usedFallback == false
};

// Determine le profil a charger au demarrage :
//  1. lit settingsPath via loadSettings ;
//  2. si un hrtf_profile est present, cherche l'id correspondant dans
//     catalog et verifie le fichier via validateSofaFile ;
//  3. si tout est valide, l'utilise (usedFallback = false) ;
//  4. sinon (fichier absent, JSON invalide, id introuvable dans catalog, ou
//     fichier .sofa invalide), retombe sur un profil par defaut :
//       - le profil d'id "subject_003" s'il est present dans catalog ;
//       - sinon le premier profil de catalog (deja trie par id) ;
//     usedFallback vaut alors true et fallbackReason explique la cause
//     precise.
// Ne fait aucune I/O console : a l'appelant d'afficher fallbackReason.
// Leve std::runtime_error si catalog est vide (aucun profil disponible,
// rien a resoudre).
StartupResolution resolveStartupProfile(const std::vector<HrtfProfile>& catalog,
                                         const std::filesystem::path& settingsPath);

}  // namespace nathan::hrtf
