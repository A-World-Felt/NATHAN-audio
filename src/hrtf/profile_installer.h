#pragma once

// Installation d'un profil HRTF CIPIC dans le dossier scanne par OpenAL
// Soft. Valide le fichier avant de le copier (voir sofa_validator.h) et
// supprime les anciens profils du dossier de destination pour qu'il n'y ait
// jamais d'ambiguite sur le profil actif (OpenAL Soft enumere les .sofa/.mhr
// du dossier ; un seul fichier present => index 0 le designe sans ambiguite).

#include <filesystem>

#include "profile_catalog.h"

namespace nathan::hrtf {

// Installe profile.path dans destDir :
//  1. valide le fichier via validateSofaFile (leve si invalide) ;
//  2. cree destDir si necessaire ;
//  3. supprime les .sofa/.mhr deja presents dans destDir ;
//  4. copie profile.path vers destDir sous son nom d'origine.
//
// Leve std::runtime_error avec un message explicite si le fichier source est
// invalide, ou si destDir est inaccessible (creation, suppression ou copie
// impossibles).
void installProfile(const HrtfProfile& profile, const std::filesystem::path& destDir);

}  // namespace nathan::hrtf
