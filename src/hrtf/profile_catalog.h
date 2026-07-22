#pragma once

// Decouverte des profils HRTF CIPIC disponibles sur disque (assets/hrtf/).
// Ne fait aucune validation de contenu : voir sofa_validator.h pour ca.

#include <filesystem>
#include <string>
#include <vector>

namespace nathan::hrtf {

struct HrtfProfile {
    std::string id;            // ex. "subject_003" (nom de fichier sans extension)
    std::filesystem::path path;  // chemin complet vers le .sofa
};

// Liste les profils .sofa presents dans hrtfAssetsDir, tries par identifiant.
// Leve std::runtime_error si hrtfAssetsDir n'existe pas ou n'est pas un
// dossier. Un dossier existant mais vide (aucun .sofa) n'est pas une erreur :
// retourne un vecteur vide.
std::vector<HrtfProfile> discoverProfiles(const std::filesystem::path& hrtfAssetsDir);

}  // namespace nathan::hrtf
