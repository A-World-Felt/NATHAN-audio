#pragma once

// Validation d'un fichier .sofa via libmysofa, avant toute utilisation par
// OpenAL Soft. Objectif : un fichier absent ou corrompu doit produire une
// erreur explicite ici plutot que de faire planter (ou pire, silencieusement
// mal se comporter) le device OpenAL au moment de l'activation du HRTF.

#include <filesystem>
#include <string>

namespace nathan::hrtf {

struct SofaValidationResult {
    bool ok = false;
    std::string error;  // vide si ok == true
};

// Verifie que sofaPath existe, est lisible par libmysofa (mysofa_load) et
// respecte les conventions SOFA attendues (mysofa_check). Ne leve jamais :
// le resultat porte l'erreur pour que l'appelant decide quoi en faire.
SofaValidationResult validateSofaFile(const std::filesystem::path& sofaPath);

}  // namespace nathan::hrtf
