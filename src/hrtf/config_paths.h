#pragma once

// Resolution du dossier de configuration propre a NATHAN (distinct du
// dossier hrtf-paths d'OpenAL Soft, voir openal_paths.h). Seul point de ce
// fichier qui differe par plateforme ; isole ici pour que le reste de la
// logique de persistance reste portable et n'inclue jamais d'en-tete
// specifique a une plateforme.
//
// Windows : %APPDATA%\NATHAN\
// Linux   : $XDG_CONFIG_HOME/nathan/ (ou ~/.config/nathan/)

#include <filesystem>

namespace nathan::hrtf {

// Leve std::runtime_error si les variables d'environnement necessaires
// (APPDATA sous Windows, HOME sous Linux a defaut de XDG_CONFIG_HOME) sont
// introuvables.
std::filesystem::path nathanConfigDirectory();

}  // namespace nathan::hrtf
