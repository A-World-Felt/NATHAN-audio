# Démo web du pipeline audio HRTF — design

- **Date** : 2026-08-06
- **Statut** : Approuvé
- **Contexte** : vitrine de l'avancement de la partie audio (profils HRTF CIPIC, décodage MP3, spatialisation), pour une démo interactive et une vidéo destinées aux parties prenantes du projet NATHAN.

## But

Montrer, à des gens qui ne liront pas le code, que le pipeline audio fonctionne réellement :
rendu HRTF binaural par profil CIPIC, 4 sources simultanées (R-AUD-03), décodage MP3, et les
métriques réelles associées — sans réécrire le moteur audio, qui existe déjà (`src/hrtf/` en
production, `benchmarks/` pour les prototypes validés comme `house_benchmark.cpp`).

## Principe d'architecture : le C++ reste le moteur, le web n'est qu'un client

Deux process locaux :

1. **`web-demo/server/`** — un nouvel exécutable C++ (`web_demo_server`) qui réutilise tel
   quel le code existant (voir « Réutilisation » plus bas) et expose son état + ses commandes
   via une petite API HTTP locale.
2. **`web-demo/client/`** — une page web (Vite + TypeScript, DOM natif, pas de framework —
   même esprit que `examples/web-chat` du repo `nathan-agent-core`) qui interroge cette API en
   boucle, affiche la scène (joueur/sprites/vecteurs), le sélecteur de profils et les
   métriques, et envoie la position de la souris et le profil choisi au serveur.

Le son sort par la sortie audio réelle de l'OS (OpenAL natif), **pas** par l'onglet du
navigateur. Aucune donnée SOFA n'est reconvertie, aucune convolution n'est refaite en
JavaScript : c'est le même OpenAL Soft + les mêmes profils `.sofa` qu'en production qui
produisent le son.

## Réutilisation du code existant (rien n'est réécrit)

| Besoin | Module réutilisé | Comportement |
|---|---|---|
| Catalogue + bascule de profil HRTF à chaud | `src/hrtf/` (`profile_catalog`, `active_profile`, `hrtf_context`, `resolveStartupProfile`) | Vraie bascule à chaud (contrairement à `house_benchmark.cpp` qui fige le profil par `#define`) |
| Chargement audio (MP3 + WAV) | `benchmarks/src/mp3_loader.h`, `benchmarks/src/wav_loader.h` | Même code que `spatial_tests.cpp` — `dr_mp3` pour le MP3 |
| Modèle de distance / atténuation | `AL_INVERSE_DISTANCE_CLAMPED` natif d'OpenAL, `AL_REFERENCE_DISTANCE=1.0`, `AL_MAX_DISTANCE=20.0`, `AL_ROLLOFF_FACTOR=1.0` (mêmes constantes que `spatial_tests.cpp`) | OpenAL calcule l'atténuation ; le serveur ne fait qu'appeler `alSource3f`/`alSourcef` |
| Mesure du temps de traitement par frame | Même méthode que `audio_benchmark.cpp`/`house_benchmark.cpp` (`measureStart`/`measureEnd`, stats glissantes) | Chiffres réels, pas une approximation JS |
| Rendu HRTF binaural | OpenAL Soft (`ALC_SOFT_HRTF`) | Aucun changement |

Nouveau code ajouté : la boucle de mise à jour continue (position flottante au lieu de la
grille de `house_benchmark`), le serveur HTTP, et la sérialisation JSON de l'état.

## `web-demo/server/` — détails

### Dépendance vendorée

`web-demo/server/httplib.h` — [cpp-httplib](https://github.com/yhirose/cpp-httplib) (MIT,
header unique, aucune dépendance hors sockets), vendoré directement dans le repo, même
convention que `benchmarks/src/dr_mp3.h`. Aucun changement à vcpkg.

### Scène et convention de coordonnées

Reprend exactement la convention documentée dans `OPENAL_SOFT_NATHAN.md` §4 : joueur fixe à
l'origine du référentiel audio, +X = droite, −Z = avant, +Z = derrière, 1 unité = 1 mètre
(`kMetersPerCell`). 4 sources fixes en champ libre (pas de murs/pièces — hors scope, voir plus
bas) :

| id | label | position monde (x, z) | fichier | format |
|---|---|---|---|---|
| `avant` | Avant | (0, −5) | `assets/test-audio.mp3` | MP3 (`dr_mp3`) |
| `droite` | Droite | (5, 0) | `assets/test-audio2.wav` | WAV |
| `arriere` | Arrière | (0, 5) | `assets/test-audio3.wav` | WAV |
| `gauche` | Gauche | (−5, 0) | `assets/test-audio4.wav` | WAV |

Le joueur a une position flottante `(px, pz)` en mètres, mise à jour par `POST /api/player`.
Chaque frame, pour chaque source : `relX = sourceX - px`, `relZ = sourceZ - pz`,
`alSource3f(source, AL_POSITION, relX, 0, relZ)` — même contrat que
`game_simulation.cpp`/`house_benchmark.cpp`.

Azimut affiché (calcul géométrique pour l'UI, indépendant du rendu HRTF lui-même) :
`atan2(relX, -relZ)` en degrés → 0° avant, +90° droite, ±180° derrière, −90° gauche. Gain
affiché : la formule publique d'`AL_INVERSE_DISTANCE_CLAMPED` appliquée aux mêmes constantes
que celles passées à OpenAL — donc pas une approximation, la même formule qu'OpenAL applique
avec ces paramètres.

### Boucle principale et threading

- Thread principal : boucle à ~60 Hz (budget 15 ms, même langage que les benchmarks
  existants : « Budget par frame », « FPS réel »). Chaque itération : lit la position joueur
  courante et une éventuelle demande de changement de profil (sous mutex), applique les
  positions/gains via OpenAL, mesure le temps de traitement (`measureStart`/`measureEnd`,
  stats glissantes sur une fenêtre récente), met à jour l'état partagé exposé par l'API.
- Thread HTTP (`httplib::Server::listen`, thread dédié) : sert `GET`/`POST`, lit/écrit l'état
  partagé sous mutex. **Ne touche jamais OpenAL directement** — un changement de profil est
  seulement *demandé* par ce thread ; c'est la boucle principale qui exécute
  `active_profile::applyProfile` (parce que ça détruit/recrée le device OpenAL, donc les
  buffers/sources existants — ça doit rester sur un seul thread, celui qui possède déjà le
  contexte).
- CORS : `Access-Control-Allow-Origin: *` sur toutes les réponses, gestion explicite des
  `OPTIONS` (préflight CORS déclenché par le `Content-Type: application/json` des `POST`).

### API HTTP (`http://127.0.0.1:8787` par défaut)

- `GET /api/profiles` → `{ "profiles": ["subject_003", ...], "active": "subject_003" }`
- `POST /api/profile` `{ "id": "subject_040" }` → bascule réelle ; `200` avec le nouveau bloc
  `profile`/`hrtf` (voir `GET /api/state`), ou `404` si l'`id` n'est pas dans le catalogue, ou
  `500` `{ "error": "..." }` si la bascule échoue à l'installation/l'ouverture du device (texte
  repris de `SofaValidationResult`/`StartupResolution` — vocabulaire « repli »).
- `POST /api/player` `{ "x": 1.2, "z": -0.4 }` → `204`, position clampée à `[-10, 10]` sur
  chaque axe côté serveur.
- `GET /api/state` → snapshot complet :

```jsonc
{
  "player": { "x": 1.2, "z": -0.4 },
  "profile": { "active": "subject_003", "default": "subject_003",
               "usedFallback": false, "fallbackReason": "" },
  "hrtf": { "status": "Enabled", "renderer": "OpenAL Soft",
            "version": "1.1 ALSOFT 1.24.3", "vendor": "OpenAL Community",
            "sampleRateHz": 44100 },
  "sources": [
    { "id": "avant", "label": "Avant", "asset": "assets/test-audio.mp3", "format": "mp3",
      "pos": { "x": 0.0, "z": -5.0 }, "distanceM": 4.6, "azimuthDeg": -12.3, "gain": 0.83 }
    /* ... droite, arriere, gauche */
  ],
  "metrics": {
    "framesProcessed": 48213,
    "frameTimeUs": { "last": 12.4, "mean": 14.1, "min": 8.2, "max": 61.5, "p95": 22.0 },
    "frameBudgetMs": 15.0, "fpsReal": 66.2, "cpuBudgetPercent": 0.09,
    "sourcesSimultaneous": 4, "meetsRAud03": true
  },
  "mp3": { "path": "assets/test-audio.mp3", "fileSizeBytes": 179837,
           "sampleRateHz": 44100, "channels": 1, "durationSec": 4.08,
           "totalSamples": 179928 }
}
```

`mp3` vient directement de `mp3_loader::load_mp3` — ce sont les vraies valeurs décodées par
`dr_mp3`, pas une estimation.

### Build

`web-demo/server/CMakeLists.txt` ajoute la cible `web_demo_server`, liée à `nathan_hrtf` et
`nathan_support` (+ `ws2_32` sous Windows pour `httplib.h`). Ajouté depuis la racine via
`add_subdirectory(web-demo/server)`, avec un commentaire le distinguant clairement de
`src/`/`tools/` (production) et de `benchmarks/` (archive gelée) : c'est du code de démo, pas
l'un ni l'autre.

## `web-demo/client/` — détails

Vite + TypeScript, DOM natif, français, palette sombre technique (même esprit que
`examples/web-chat` de `nathan-agent-core`, sans en reprendre les couleurs — cf. son propre
ADR : la palette n'est pas un standard à copier). Vocabulaire repris tel quel du reste du
produit : **profil**, **profil actif**, **bascule/basculer**, **catalogue**, **repli**,
**son de test**.

### Mise en page

- **Droite** — scène SVG, 1 unité = 1 m, `viewBox` −12..12 sur les deux axes, grille légère
  tous les mètres + marquée tous les 5 m, légende de convention d'axes dans un coin (reprend
  le diagramme d'`OPENAL_SOFT_NATHAN.md` §4 : avant en haut, droite à droite). Le joueur (X)
  est déplaçable à la souris (pointer events, throttle ~50 ms vers `POST /api/player`,
  fire-and-forget). Les 4 sprites (O) sont fixes, avec une icône de format (MP3/WAV). Un
  vecteur joueur→sprite par source, étiqueté distance + azimut.
- **Gauche, haut** — sélecteur de profil : liste des profils retournés par
  `GET /api/profiles`, profil actif en surbrillance, clic → `POST /api/profile`.
- **Gauche, milieu** — métriques : par source (distance, azimut, gain linéaire + dB,
  fichier/format) et globales (statut HRTF, renderer, fréquence d'échantillonnage, temps de
  traitement par frame — moyenne/p95/budget %, badge « 4 sources simultanées — R-AUD-03 »).
  Toutes les valeurs viennent de `GET /api/state`, aucune n'est calculée dans le navigateur.
- **Gauche, bas** — panneau MP3 : nom de fichier, taille, et les propriétés réelles retournées
  par `mp3_loader` (fréquence, canaux, durée, nb d'échantillons) — légende explicite que ce
  sont les valeurs décodées par `dr_mp3` côté C++.

### Rafraîchissement et robustesse

- `GET /api/state` interrogé en boucle (~60 ms). Micro-flash visuel sur toute valeur qui
  change (même principe que le flash `.active` de la démo agent-core), pour rester lisible à
  l'enregistrement sans voix off.
- Si le serveur ne répond pas : bannière « Aucun serveur audio ne répond sur
  `http://127.0.0.1:8787`. Lance `web_demo_server` (voir `web-demo/README.md`) puis recharge
  la page. » — même gabarit que la bannière « Ollama injoignable » de la démo agent-core.
- URL du serveur configurable via `VITE_SERVER_URL` (défaut `http://127.0.0.1:8787`), même
  convention que `VITE_OLLAMA_HOST` dans `examples/web-chat`.

## Hors scope

- Murs, pièces, réverbération EFX (`house_benchmark`'s room model) — pas demandé pour cette
  démo, qui reste en champ libre à 4 sources.
- Mode « pilote automatique » scripté pour l'enregistrement — décision prise : contrôle
  manuel à la souris uniquement.
- Toute exécution du moteur audio dans le navigateur (Web Audio/SOFA/convolution) —
  explicitement écarté : le moteur C++ existant est réutilisé tel quel.
- Persistance du profil choisi dans la démo vers `settings.json` — la démo ne doit pas
  modifier la configuration utilisateur réelle de la console ; elle bascule le profil en
  mémoire pour la session du serveur de démo uniquement, sans appeler `saveSettings`.

## Vérification

Pas de suite de tests automatisés dans ce repo pour le pipeline audio — la validation
existante est entièrement interactive/à l'oreille (`OPENAL_SOFT_NATHAN.md`, « Verdict » par
test). On reste cohérent avec ça :

- Compilation propre : `cmake --build build` inclut `web_demo_server` sans casser les cibles
  existantes.
- Vérification manuelle de l'API : `curl` sur chaque endpoint (`/api/profiles`, `/api/state`,
  `POST /api/player`, `POST /api/profile`) pour confirmer les formes JSON et les codes HTTP.
- Vérification à l'oreille : déplacer le joueur doit déplacer le son perçu ; changer de profil
  doit changer le rendu HRTF ; les 4 sources doivent jouer simultanément sans coupure
  perceptible (R-AUD-03).
- Vérification visuelle : les métriques affichées correspondent à ce que retourne
  `GET /api/state` (pas de calcul dupliqué côté client qui pourrait diverger).

## Lancement (documenté dans `web-demo/README.md`)

1. `./build/Debug/web_demo_server.exe` (ou `./build/web_demo_server` sur Linux) — démarre le
   moteur audio + l'API locale.
2. `cd web-demo/client && npm install && npm run dev` — démarre la page, l'ouvrir dans le
   navigateur.

Pour la vidéo : capturer le son du **bureau** (OBS, capture audio desktop), pas celui de
l'onglet — le son sort par la sortie OpenAL native, pas par le navigateur.
