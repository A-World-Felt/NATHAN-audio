# nathan-audio — Benchmark audio spatial HRTF

Prototype de validation du pipeline audio binaural pour la console NATHAN,
une console de jeu portable accessible aux personnes ayant une déficience visuelle.

Ce repo contient trois programmes de test, un simulateur de scène de jeu et deux
benchmarks de performance, tous basés sur OpenAL Soft avec rendu HRTF binaural via profils CIPIC.

---

## Prérequis

- Windows 10/11 x64
- [Build Tools pour Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) — workload **Développement Desktop en C++**
- Git

---

## Installation des dépendances

### 1. Installer vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg integrate install
```

### 2. Installer les librairies

```powershell
C:\vcpkg\vcpkg install openal-soft:x64-windows
C:\vcpkg\vcpkg install libmysofa:x64-windows
```

---

## Compilation

```powershell
cd "chemin\vers\le\repo"

cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

cmake --build build
```

---

## Profils HRTF CIPIC

Les 45 profils CIPIC (`.sofa`) doivent être placés dans `assets/hrtf/`.
Ils sont téléchargeables sur [sofaconventions.org](https://sofaconventions.org/mediawiki/index.php/Files) — section CIPIC.

Un fichier WAV mono 16 ou 24 bits 44 100 Hz doit être présent dans `assets/test-audio.wav`
(et `test-audio2.wav` à `test-audio5.wav` pour `house_benchmark`, un son par pièce).

---

## Programmes disponibles

### `profile_selector`
Sélecteur interactif de profil HRTF CIPIC.
Un son tourne en cercle autour de la tête — flèches gauche/droite pour changer de profil.
Entrée pour confirmer. Le profil choisi est sauvegardé dans `assets/config/user_profile.txt`.

```powershell
.\build\Debug\profile_selector.exe
```

### `spatial_tests`
6 tests de spatialisation séquentiels avec le profil défini dans `#define HRTF_PROFILE`.

```powershell
.\build\Debug\spatial_tests.exe
```

Modifier le profil en haut de `spatial_tests.cpp` :
```cpp
#define HRTF_PROFILE "subject_003"  // remplacer par le profil choisi
```

| Test | Description |
|---|---|
| 1 | Son fixe à gauche puis à droite |
| 2 | 4 sources simultanées |
| 3 | Sweep droite → gauche + 2 cercles |
| 4 | Atténuation par distance |
| 5 | Confusion avant / arrière |
| 6 | Élévation (bas / haut / diagonale) |

### `game_simulation`
Prototype du contrat SoundScape/Sound du moteur NATHAN.
Affichage terminal 2D avec joueur (`@`) et acteur sonore (`♪`).

```powershell
.\build\Debug\game_simulation.exe
```

**Phase 1 — Cercle libre :** l'acteur tourne automatiquement, le joueur se déplace avec les flèches.
Le son suit la position visuelle de l'acteur en temps réel.

**Phase 2 — Murs et pièces :** deux pièces séparées par un mur.
Le son s'atténue et la réverbération change selon que le joueur est dans la même pièce que l'acteur.

### `audio_benchmark`
Benchmark non-interactif : 4 sources en mouvement continu (cf. R-AUD-03) pendant 30 secondes.
Mesure le temps de traitement audio par frame (positions + appels OpenAL) et produit un rapport
de statistiques portable (moyenne/médiane/min/max/percentiles 95e-99e) destiné à être reproduit
sur les MPU candidats pour guider le choix du processeur.

```powershell
.\build\Debug\audio_benchmark.exe
```

Rapport sauvegardé dans `benchmark_results.txt`.

### `house_benchmark`
Benchmark interactif et réaliste : une grande maison (31×21 cases) à 5 pièces (Cuisine, Salon,
Chambre, Salle de bain, Salle de musique), chacune avec sa propre source sonore fixe —
5 sources simultanées, au-delà du minimum R-AUD-03 de 4. Le joueur se déplace avec les flèches ;
à chaque frame (15ms) la position relative de chaque source, l'atténuation inter-pièces
(ray casting Bresenham) et la réverbération EFX de la pièce du joueur sont recalculées, avec
affichage en direct du temps de traitement audio et du FPS réel.

```powershell
.\build\Debug\house_benchmark.exe
```

Touches : flèches pour se déplacer, `Q` ou `Échap` pour quitter et sauvegarder le rapport
complet (frames totales, statistiques par frame, % du budget temps réel 15ms) dans
`house_benchmark_results.txt`.

---

## Structure du projet

```
nathan-audio/
├── assets/
│   ├── hrtf/               ← profils CIPIC .sofa (à télécharger)
│   ├── config/             ← user_profile.txt (généré par profile_selector)
│   ├── test-audio.wav      ← son de test (mono 16 bits 44100 Hz)
│   └── test-audio2.wav … test-audio5.wav  ← un son par pièce (house_benchmark, mono 16/24 bits)
├── src/
│   ├── al_hrtf_device.h/.cpp   ← cycle de vie device OpenAL + HRTF
│   ├── hrtf_profile.h/.cpp     ← installation profil CIPIC via libmysofa
│   ├── input_keys.h/.cpp       ← clavier non-bloquant (Windows + Linux)
│   └── wav_loader.h/.cpp       ← chargement WAV mono 16 ou 24 bits
├── profile_selector.cpp
├── spatial_tests.cpp
├── game_simulation.cpp
├── audio_benchmark.cpp
├── house_benchmark.cpp
├── CMakeLists.txt
├── README.md
└── OPENAL_SOFT_NATHAN.md   ← documentation complète des tests
```

---

## Documentation

Voir [OPENAL_SOFT_NATHAN.md](OPENAL_SOFT_NATHAN.md) pour la documentation complète :
fonctionnalités validées, résultats des tests, code d'exemple, et mapping avec les requis du projet.

---

## Dépendances

| Librairie | Version | Rôle |
|---|---|---|
| [OpenAL Soft](https://openal-soft.org/) | 1.25.1 | Rendu audio binaural HRTF |
| [libmysofa](https://github.com/hoene/libmysofa) | via vcpkg | Validation et lecture des fichiers SOFA CIPIC |

---

## Contexte

Ce repo fait partie du projet NATHAN — PMC660/760/860, Université de Sherbrooke.
Équipe AUD — validation du pipeline audio binaural sur PC avant portage sur MPU embarqué.