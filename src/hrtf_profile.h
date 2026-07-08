#pragma once

// Gestion des profils CIPIC en tant que fichiers : lister assets/hrtf/*.sofa,
// valider un fichier via libmysofa (seul role de libmysofa dans ce projet -
// aucune extraction de coefficients, aucune convolution), et l'installer
// dans le dossier que OpenAL Soft scanne lui-meme pour son HRTF interne
// (ALC_SOFT_HRTF). C'est OpenAL Soft qui fait toute la convolution HRTF.

#include <filesystem>
#include <vector>

namespace hrtf_profile {

// Liste les profils CIPIC disponibles (*.sofa), tries par nom de fichier.
std::vector<std::filesystem::path> listProfiles(const std::filesystem::path& hrtfDir);

// Chemin ou OpenAL Soft doit trouver le fichier HRTF actif.
// Windows : %APPDATA%\OpenAL\hrtf\current.sofa
// Linux   : $XDG_DATA_HOME/openal/hrtf/current.sofa (ou ~/.local/share/...)
std::filesystem::path activeProfileDestPath();

// Valide sofaPath via libmysofa (mysofa_load/mysofa_free), nettoie le
// dossier de destination (pour qu'il ne contienne jamais qu'un seul fichier
// HRTF actif - evite toute ambiguite sur ALC_HRTF_ID_SOFT) puis copie le
// fichier vers activeProfileDestPath(). Leve std::runtime_error en cas
// d'echec.
void installProfile(const std::filesystem::path& sofaPath);

}  // namespace hrtf_profile
