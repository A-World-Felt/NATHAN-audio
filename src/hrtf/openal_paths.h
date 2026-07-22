#pragma once

// Resolution du dossier que OpenAL Soft scanne pour son HRTF interne
// (hrtf-paths). Seul point du module qui differe par plateforme ; isole ici
// pour que le reste de la logique (validation, installation, activation)
// reste portable et n'inclue jamais d'en-tete specifique a une plateforme.
//
// Windows : %APPDATA%\OpenAL\hrtf\
// Linux   : $XDG_DATA_HOME/openal/hrtf/ (ou ~/.local/share/openal/hrtf/)

#include <filesystem>

namespace nathan::hrtf {

// Leve std::runtime_error si les variables d'environnement necessaires
// (APPDATA sous Windows, HOME sous Linux a defaut de XDG_DATA_HOME) sont
// introuvables.
std::filesystem::path openalHrtfDirectory();

}  // namespace nathan::hrtf
