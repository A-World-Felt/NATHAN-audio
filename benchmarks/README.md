# benchmarks/ — prototypes de la phase d'exploration

Ce dossier contient les programmes écrits pendant la phase d'exploration du
pipeline audio spatial HRTF (OpenAL Soft + profils CIPIC) : validation des
briques de base, tests de spatialisation, simulation de scène de jeu et
benchmarks de performance.

Ils sont **conservés comme référence** (résultats, approche, code d'exemple)
mais ne sont plus le code actif du projet. **Le code de production vit dans
`src/` et `tools/` à la racine du dépôt.** Ne pas ajouter de nouveau
développement ici.

## Contenu

| Fichier | Rôle |
|---|---|
| `main.cpp` | Tout premier prototype du pipeline HRTF, antérieur aux programmes ci-dessous. Non compilé (absent du CMakeLists) — gardé comme trace du point de départ. |
| `profile_selector.cpp` | Sélecteur interactif de profil HRTF CIPIC |
| `spatial_tests.cpp` | 6 tests de spatialisation séquentiels |
| `game_simulation.cpp` | Simulateur du contrat SoundScape/Sound du moteur NATHAN |
| `audio_benchmark.cpp` | Benchmark non-interactif (4 sources, 30-60 s) |
| `house_benchmark.cpp` | Benchmark interactif maison à 5 pièces (5 sources) |
| `src/` | Modules de support utilisés par ces prototypes (WAV, device HRTF, profils, clavier) |
| `scripts/` | Scripts Python d'inspection SOFA ponctuels (voir plus bas) |

Voir [OPENAL_SOFT_NATHAN.md](../OPENAL_SOFT_NATHAN.md) à la racine pour la
documentation complète des tests, et le [README](../README.md) racine pour
la compilation et l'exécution.

## `scripts/`

Scripts Python d'exploration ponctuels, utilisés pour inspecter le contenu
des fichiers CIPIC (`sofa_inspect.py`), en déduire la grille d'élévations
irrégulière qui a motivé le choix de ne pas passer par `makemhr`
(`grid.py`), et copier un profil vers le dossier HRTF d'OpenAL Soft avant que
`src/hrtf/` n'existe (`copy_sofa.py`, obsolète — voir `tools/hrtf_check`).
Chemins codés en dur pour la machine d'origine : gardés tels quels comme
référence, **pas destinés à être exécutés ici**.

## Compilation

Ces programmes compilent toujours depuis leur nouvel emplacement : ils sont
inclus par le `CMakeLists.txt` racine via `add_subdirectory(benchmarks)`.
Les exécutables sont produits normalement dans `build/` et doivent être
lancés depuis la racine du dépôt (ils lisent `assets/...` en chemin relatif).
