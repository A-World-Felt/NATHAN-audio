# OpenAL Soft — Documentation des tests pour le projet NATHAN

> Console de jeu accessible pour personnes ayant une déficience visuelle.
> L'audio spatial binaural HRTF est l'interface principale du jeu.
> Tests réalisés sur PC Windows + MSVC, profil CIPIC `subject_003`.

---

## 1. Initialisation de la librairie

OpenAL Soft s'initialise en trois étapes : ouvrir le device audio, créer un contexte, l'activer.
L'extension `ALC_SOFT_HRTF` active le rendu binaural via HRTF.

```cpp
ALCdevice*  device  = alcOpenDevice(nullptr);
ALCint attribs[] = {
    ALC_HRTF_SOFT,    ALC_TRUE,
    ALC_HRTF_ID_SOFT, 0,       // index du profil HRTF dans le dossier scanné
    0
};
ALCcontext* context = alcCreateContext(device, attribs);
alcMakeContextCurrent(context);
```

**Statut HRTF vérifiable après ouverture :**

```cpp
ALCint status;
alcGetIntegerv(device, ALC_HRTF_STATUS_SOFT, 1, &status);
// ALC_HRTF_ENABLED_SOFT = actif
// ALC_HRTF_DENIED_SOFT  = refusé par le pilote
```

**Verdict :** Initialisation fiable, HRTF activée systématiquement sur PC.
Sur MPU Linux, même code — `libopenal` installé via `apt install libopenal-dev`.

---

## 2. Chargement des profils HRTF

### Profil par défaut — MIT KEMAR
OpenAL Soft intègre KEMAR nativement. Aucune configuration requise — actif dès que `ALC_HRTF_SOFT=ALC_TRUE`.

### Profils CIPIC — 45 sujets humains
OpenAL Soft scanne un dossier spécifique pour les fichiers `.sofa` :

| Plateforme | Dossier scanné |
|---|---|
| Windows | `%APPDATA%\OpenAL\hrtf\` |
| Linux | `~/.local/share/openal/hrtf/` |

**Pipeline de chargement d'un profil CIPIC :**

```
1. Valider le .sofa via libmysofa_load()
2. Supprimer tous les .sofa du dossier de destination
3. Copier subject_XXX.sofa → dossier/current.sofa
4. Fermer et rouvrir le contexte OpenAL (ALC_HRTF_ID_SOFT=0)
```

```cpp
// Validation libmysofa (seul rôle de libmysofa dans ce projet)
int err = MYSOFA_OK;
MYSOFA_HRTF* h = mysofa_load(path, &err);
if (!h || err != MYSOFA_OK) { /* invalide */ }
mysofa_free(h);

// Copie vers le dossier OpenAL
fs::copy_file(sofaPath, dest, fs::copy_options::overwrite_existing);

// Rechargement du contexte
alDevice.close();
alDevice.open(); // ALC_HRTF_ID_SOFT=0 → charge current.sofa
```

**Verdict :** Les 45 profils CIPIC fonctionnent. libmysofa ne fait que **valider** le fichier —
c'est OpenAL Soft qui fait toute la convolution HRTF en interne.

---

## 3. Chargement audio — WAV mono 16/24 bits

Format requis par le projet (contrainte C-AUD-01) : WAV PCM mono 44 100 Hz.
Le loader accepte le 16 bits et le 24 bits (certains assets de test, ex. `test-audio4.wav`
pour `house_benchmark`, sont fournis en 24 bits) ; les deux profondeurs sont normalisées
vers le même format float `[-1, 1]` en interne.

```cpp
// Lecture RIFF chunk par chunk, normalisation float [-1, 1]
WavAudio wav = load_wav_mono16("assets/test-audio.wav");

// Création buffer OpenAL
ALuint buffer;
alGenBuffers(1, &buffer);
std::vector<int16_t> pcm(wav.samples.size());
for (size_t i = 0; i < wav.samples.size(); ++i)
    pcm[i] = static_cast<int16_t>(std::lround(wav.samples[i] * 32767.0f));
alBufferData(buffer, AL_FORMAT_MONO16, pcm.data(),
             pcm.size() * sizeof(int16_t), wav.sampleRate);
```

**Verdict :** Chargement robuste. Le loader valide le format et lève une exception si le fichier
n'est pas mono 16 ou 24 bits — compatible avec la contrainte C-AUD-01.

---

## 4. Positionnement spatial — `alSource3f`

C'est la fonction centrale du contrat SoundScape/Sound entre le game engine et le module audio.

```cpp
// Positionner un acteur dans l'espace 3D relatif au joueur
alSource3f(source, AL_POSITION, relX, relY, relZ);
```

**Conventions de coordonnées :**

```
        0° avant (z = -1)
             │
-90° gauche ─┼─ +90° droite (x = +1)
             │
        180° derrière (z = +1)

y = +1 : au-dessus
y = -1 : en dessous
```

**Puisque le joueur est toujours fixe à (0,0,0) dans notre architecture :**

```cpp
// Le game engine calcule la position relative et l'envoie directement
float relX = (actorWorldX - playerWorldX) * metersPerCell;
float relZ = (actorWorldZ - playerWorldZ) * metersPerCell;
alSource3f(source, AL_POSITION, relX, 0.0f, relZ);
// OpenAL Soft applique la HRTF correspondante automatiquement
```

**Verdict :** Le contrat est minimal et efficace. Un seul appel par acteur par frame suffit.

---

## 5. Résultats des tests de spatialisation

### Test 1 — Localisation gauche / droite ✅
Son fixe à gauche (-90°) puis à droite (+90°). Localisation nette et immédiate.
**Pertinent pour R-ACC-04** : les utilisateurs identifient correctement la direction.

### Test 2 — 4 sources simultanées ✅
4 sources aux positions avant/droite/derrière/gauche, ajoutées progressivement.
Aucun artefact ni coupure perçu. **Valide R-AUD-03** : 4 sources sans coupure audio perceptible.

### Test 3 — Mouvement continu ✅
Sweep droite → gauche en 4 secondes puis 2 cercles complets.
Mouvement fluide et continu, la position suit exactement la trajectoire programmée.

```cpp
// Mise à jour à chaque frame (15ms) via holdFor + lambda
holdFor(7.2, [&](double t) {
    float az = static_cast<float>(t) / 7.2f * 720.0f; // 2 tours
    setAzimuth(src, az);
});
```

### Test 4 — Atténuation par distance ✅
Acteur de 10m → 0.5m → 10m devant le joueur. Atténuation naturelle et progressive.
Modèle utilisé : `AL_INVERSE_DISTANCE_CLAMPED`.

```cpp
alSourcef(src, AL_REFERENCE_DISTANCE, 1.0f);  // distance de référence (gain=1)
alSourcef(src, AL_MAX_DISTANCE,       20.0f); // distance maximale d'audibilité
alSourcef(src, AL_ROLLOFF_FACTOR,     1.0f);  // courbe d'atténuation physique
```

### Test 5 — Confusion avant / arrière ⚠️
Alternance lente (2s) et rapide (0.4s) entre 0° (avant) et 180° (derrière).
**Résultat :** Différence perceptible mais subtile avec KEMAR et certains profils CIPIC.
C'est le problème classique des HRTFs génériques — les indices spectraux du pavillon varient
selon la morphologie individuelle. La sélection du bon profil CIPIC améliore ce test.
**À valider avec les utilisateurs aveugles de l'APHVE.**

### Test 6 — Élévation (Y ≠ 0) ⚠️
Son fixe en bas (y=-2m), en haut (y=+2m), sweep bas→haut, diagonale devant-bas→derrière-haut.
**Résultat :** Différence perceptible entre haut et bas mais moins nette qu'en azimuth.
Les HRTFs génériques sont moins précises en élévation qu'en azimuth — connu dans la littérature.
Pour un jeu 2D comme *La Maison de Nathan*, l'élévation est secondaire mais utilisable pour
distinguer des sons de sources situées à des hauteurs très différentes (sol vs plafond).

---

## 6. Simulation du contrat SoundScape/Sound

Le prototype `game_simulation.cpp` valide le contrat entre le game engine et le module audio.

### Contrat implémenté

```
Chaque frame (15ms) :
  SoundScape {
    acteurs actifs : position absolue monde (x, z)
  }
  → Module audio calcule (relX, relZ) = acteur - joueur
  → alSource3f(source, AL_POSITION, relX, 0, relZ)
```

### Phase 1 — Cercle libre ✅
L'acteur fait un cercle automatique (rayon 6m, période 12s). Le joueur se déplace avec les flèches.
Le son suit exactement la position visuelle de l'acteur sur la grille.
**Valide que le référentiel centré sur le joueur fonctionne correctement.**

### Phase 2 — Murs et pièces ✅
Deux pièces séparées par un mur avec une ouverture de 2 cases.

**Détection de pièce — Ray casting Bresenham :**
```cpp
int wallCount = countWallsBetween(player, actor); // ligne droite entre les deux
bool sameRoom = (wallCount == 0);
```

**Effets audio selon la situation :**

| Situation | Gain | Réverbération |
|---|---|---|
| Même pièce | 1.0 (100%) | `EFX_REVERB_PRESET_ROOM` — légère |
| Pièce différente | 0.6 (60%) | `EFX_REVERB_PRESET_STONEROOM` — longue et diffuse |

**Verdict :** Un utilisateur aveugle peut distinguer s'il est dans la même pièce que la source
sonore, grâce à la combinaison atténuation + changement de réverbération.

---

## 7. Réverbération EFX

OpenAL EFX (Extension for Effects) ajoute des effets audio via un système de slots.

```
Source audio
    ↓ AL_AUXILIARY_SEND_FILTER
Effect Slot ← Effet actif (reverb room ou stoneroom)
    ↓
Sortie avec réverbération appliquée
```

Les fonctions EFX sont chargées dynamiquement — si EFX n'est pas disponible,
le programme fonctionne sans réverbération (dégradation gracieuse).

```cpp
// Chargement dynamique
LPALGENEFFECTS alGenEffects =
    (LPALGENEFFECTS)alGetProcAddress("alGenEffects");

// Création d'un effet reverb depuis un preset
ALuint effect;
alGenEffects(1, &effect);
alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
alEffectf(effect, AL_REVERB_DECAY_TIME, preset.flDecayTime);
// ... autres paramètres du preset

// Activation sur le slot
alAuxiliaryEffectSloti(slot, AL_EFFECTSLOT_EFFECT, effect);

// Attachement de la source au slot
alSource3i(source, AL_AUXILIARY_SEND_FILTER, slot, 0, AL_FILTER_NULL);
```

---

## 8. Portabilité Windows / Linux

Tout le code de logique audio est portable. Seuls deux éléments sont conditionnels :

| Élément | Windows | Linux / Raspberry Pi |
|---|---|---|
| Clavier non-bloquant | `_kbhit()` + `_getch()` (conio.h) | `termios` + `select()` |
| Page de code console | `SetConsoleOutputCP(CP_UTF8)` | Non nécessaire |
| Dossier HRTF OpenAL | `%APPDATA%\OpenAL\hrtf\` | `~/.local/share/openal/hrtf/` |
| Installation librairie | vcpkg | `apt install libopenal-dev libmysofa-dev` |

---

## 9. Ce qui reste à valider — Prochaines étapes

| Étape | Objectif | Outil |
|---|---|---|
| **Étape 1 — MIPS** | Mesurer la charge CPU réelle du pipeline OpenAL Soft | `audio_benchmark` / `house_benchmark` (rapport portable intégré) + Intel VTune Profiler |
| **Étape 3 — RPi Zero** | Valider latence ≤ 25ms (R-AUD-02) et qualité sur vrai MPU | `house_benchmark` (scénario réaliste 5 pièces, reproductible) sur Raspberry Pi Zero 2W |
| **Tests APHVE** | Valider R-ACC-04 et R-AUD-01 avec utilisateurs aveugles | Protocole CER |

---

## 10. Résumé — Fonctionnalités OpenAL Soft validées pour NATHAN

| Fonctionnalité OpenAL Soft | Requis NATHAN | Statut |
|---|---|---|
| Rendu HRTF binaural | R-AUD-01 | ✅ Validé |
| 4 sources simultanées | R-AUD-03 | ✅ Validé |
| Positionnement 3D (x, y, z) | R-ACC-04 | ✅ Validé |
| Atténuation par distance | R-ACC-04 | ✅ Validé |
| Profils CIPIC sélectionnables | Paramètres jeu | ✅ Validé |
| Réverbération EFX par pièce | Immersion | ✅ Validé |
| Portabilité Linux (MPU) | Architecture | ✅ Validé |
| Latence ≤ 25ms | R-AUD-02 | ⏳ À mesurer sur RPi |
| Confusion avant/arrière | R-ACC-04 | ⚠️ Dépend du profil CIPIC |
| Élévation perceptible | Jeu 2D+Z | ⚠️ Secondaire, utilisable |
