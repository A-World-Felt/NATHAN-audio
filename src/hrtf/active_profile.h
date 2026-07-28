#pragma once

// Point unique qui rend un profil HRTF audible : installe le .sofa dans le
// dossier scanne par OpenAL Soft (profile_installer.h) puis rouvre le
// contexte OpenAL pour qu'il soit pris en compte (hrtf_context.h). Evite de
// dupliquer cette sequence entre les outils de verification et l'interface
// d'ecoute (profile_audition.h, DEV-239).

#include <filesystem>

#include "hrtf_context.h"
#include "profile_catalog.h"

namespace nathan::hrtf {

// Installe profile dans openalHrtfDir (voir installProfile) puis appelle
// context.open() pour que le pilote recharge son unique .sofa restant.
//
// ATTENTION : HrtfContext::open() detruit le device/contexte OpenAL
// precedent. Toute source ou buffer OpenAL cree avant cet appel devient
// invalide (identifiants perimes) et doit etre recree par l'appelant apres
// le retour de applyProfile.
//
// Leve std::runtime_error si l'installation ou l'ouverture du contexte
// echoue (voir installProfile et HrtfContext::open) ; dans ce cas le
// contexte est ferme (voir HrtfContext::open) et l'appelant ne dispose plus
// d'un device utilisable tant qu'un nouvel appel n'a pas reussi.
void applyProfile(const HrtfProfile& profile, const std::filesystem::path& openalHrtfDir,
                   HrtfContext& context);

}  // namespace nathan::hrtf
