#pragma once

// Etat de selection de profil HRTF en memoire : profil actif, navigation
// suivant/precedent (avec bouclage), selection par identifiant. Consomme
// par la future interface d'ecoute (DEV-239) et par les outils de
// verification en ligne de commande.

#include <string>
#include <vector>

#include "profile_catalog.h"

namespace nathan::hrtf {

class ProfileSelector {
public:
    // Leve std::invalid_argument si profiles est vide, ou si initialId ne
    // correspond a aucun profil de profiles.
    ProfileSelector(std::vector<HrtfProfile> profiles, const std::string& initialId);

    const HrtfProfile& current() const;
    const std::vector<HrtfProfile>& profiles() const { return profiles_; }

    // Avance vers le profil suivant, boucle du dernier vers le premier.
    void next();
    // Recule vers le profil precedent, boucle du premier vers le dernier.
    void previous();

    // Selectionne le profil d'identifiant id. Retourne false sans effet si
    // id n'existe pas parmi profiles() (pas d'exception : appele en reponse
    // a une entree utilisateur qui peut etre invalide sans que le programme
    // doive s'arreter).
    bool selectById(const std::string& id);

private:
    std::vector<HrtfProfile> profiles_;
    std::size_t currentIndex_ = 0;
};

}  // namespace nathan::hrtf
