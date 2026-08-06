#pragma once

namespace web_demo {

// Gain lineaire pour AL_INVERSE_DISTANCE_CLAMPED (formule publique OpenAL),
// avec les memes constantes que AL_REFERENCE_DISTANCE/AL_MAX_DISTANCE/
// AL_ROLLOFF_FACTOR passees a OpenAL — donc la meme valeur que celle
// qu'OpenAL applique reellement, pas une approximation.
float distanceGain(float distanceMeters, float referenceDistance, float maxDistance,
                    float rolloffFactor);

// Distance euclidienne 2D entre l'origine et le point (relX, relZ).
float distanceMeters(float relX, float relZ);

// Azimut en degres. Convention identique a OPENAL_SOFT_NATHAN.md #4 :
// 0 = avant (-Z), +90 = droite (+X), +-180 = derriere, -90 = gauche.
float azimuthDegrees(float relX, float relZ);

}  // namespace web_demo
