#pragma once

// Abstraction d'entree pour la boucle d'ecoute de profil HRTF (DEV-239).
// Decouple runProfileAudition (profile_audition.h) du peripherique
// d'entree reel : le clavier est la premiere implementation, les boutons
// GPIO du MPU en brancheront une autre plus tard sans toucher a la logique
// d'ecoute.

#include <memory>

namespace nathan::hrtf {

enum class AuditionCommand {
    None,
    Next,
    Previous,
    Confirm,
    Cancel,
};

class AuditionInput {
public:
    virtual ~AuditionInput() = default;

    // Non bloquant : retourne AuditionCommand::None si aucune commande
    // n'est disponible pour l'instant.
    virtual AuditionCommand poll() = 0;
};

// Implementation clavier, portable Windows/Linux.
// Fleche gauche = Previous, fleche droite = Next, Entree = Confirm,
// Echap ou Q = Cancel. poll() est non bloquant sur les deux plateformes.
// POSIX : bascule le terminal en mode brut (termios) a la construction,
// restaure le mode d'origine au destructeur (RAII).
class KeyboardAuditionInput : public AuditionInput {
public:
    KeyboardAuditionInput();
    ~KeyboardAuditionInput() override;

    KeyboardAuditionInput(const KeyboardAuditionInput&) = delete;
    KeyboardAuditionInput& operator=(const KeyboardAuditionInput&) = delete;

    AuditionCommand poll() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace nathan::hrtf
