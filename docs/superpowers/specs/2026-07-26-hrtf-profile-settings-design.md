# DEV-238 — Sélection et persistance du profil HRTF utilisateur

## Contexte

`src/hrtf/` contient déjà le chargement de profils HRTF CIPIC (DEV-206) :
découverte (`profile_catalog`), validation (`sofa_validator`), installation
dans le dossier scanné par OpenAL Soft (`profile_installer`), activation
(`hrtf_context`), et résolution de chemins spécifiques à la plateforme
(`openal_paths`).

Cette tâche ajoute la logique qui décide **quel** profil charger et fait
survivre ce choix aux redémarrages. Elle ne touche pas au chargement OpenAL
existant. Elle ne fournit pas non plus l'interface d'écoute interactive (son
tournant, navigation) — c'est DEV-239. Ici on fournit seulement les briques
que cette future interface utilisera : profil suivant/précédent, sélection
par identifiant, sauvegarde.

## Architecture

Trois nouveaux fichiers dans `src/hrtf/`, ajoutés à la bibliothèque
`nathan_hrtf` existante (`src/CMakeLists.txt`) :

- `config_paths.h/.cpp` — résout le dossier de configuration NATHAN
  (`%APPDATA%\NATHAN\` sous Windows, `$XDG_CONFIG_HOME/nathan/` ou
  `~/.config/nathan/` sous Linux). Isole le code spécifique à la plateforme,
  à l'image de `openal_paths.h`. Fichier séparé de `openal_paths.h` : ce
  dernier résout spécifiquement le dossier que *OpenAL Soft* scanne pour son
  HRTF ; le dossier de config NATHAN est un concept différent (propre à
  l'application, pas à OpenAL) et ne doit pas être mélangé dans le même
  module.
- `profile_settings.h/.cpp` — persistance JSON et logique de résolution au
  démarrage.
- `profile_selector.h/.cpp` — état de sélection en mémoire.

Plus un outil de vérification : `tools/hrtf_profile_select.cpp`, sur le
modèle de `tools/hrtf_check.cpp` (DEV-167/DEV-206).

## Format de persistance

Fichier JSON unique, une clé pour l'instant, format extensible pour de
futurs réglages (volume, réverbération, etc.) :

```json
{
  "hrtf_profile": "subject_003"
}
```

Emplacement : `<dossier config NATHAN>/settings.json`.

Librairie : nlohmann/json (header-only), installée via vcpkg en mode
classic — cohérent avec l'installation existante d'OpenAL Soft et
libmysofa (pas de passage en mode manifeste vcpkg.json) :

- `vcpkg install nlohmann-json:x64-windows` (à documenter dans le README,
  section « Installer les librairies »)
- `find_package(nlohmann_json CONFIG REQUIRED)` dans `CMakeLists.txt` racine
- `target_link_libraries(nathan_hrtf PUBLIC ... nlohmann_json::nlohmann_json)`
  dans `src/CMakeLists.txt`

`nlohmann::json` reste un détail d'implémentation de `profile_settings.cpp` :
le header `profile_settings.h` n'expose qu'un struct `AppSettings` simple,
pas de type nlohmann dans l'API publique.

## Composants

### `config_paths.h`

```cpp
// Leve std::runtime_error si les variables d'environnement necessaires
// (APPDATA sous Windows, HOME sous Linux a defaut de XDG_CONFIG_HOME) sont
// introuvables.
std::filesystem::path nathanConfigDirectory();
```

### `profile_settings.h`

```cpp
struct AppSettings {
    std::string hrtfProfile;  // vide si non defini
};

// Lit settingsPath. Retourne std::nullopt si le fichier n'existe pas, n'est
// pas lisible (permissions), si le JSON est syntaxiquement invalide, ou si
// sa forme ne correspond pas a AppSettings (pas un objet, cle hrtf_profile
// absente ou pas une chaine). Ne leve jamais : ce sont tous des etats
// attendus geres par l'appelant via le repli (voir resolveStartupProfile et
// la section Erreurs).
std::optional<AppSettings> loadSettings(const std::filesystem::path& settingsPath);

// Ecrit settings dans settingsPath de facon atomique (fichier temporaire
// dans le meme dossier, puis rename). Cree le dossier parent si necessaire.
// Leve std::runtime_error si le dossier est inaccessible en ecriture ou si
// le rename echoue.
void saveSettings(const AppSettings& settings, const std::filesystem::path& settingsPath);

struct StartupResolution {
    HrtfProfile profile;
    bool usedFallback = false;
    std::string fallbackReason;  // vide si usedFallback == false
};

// Determine le profil a charger au demarrage :
//  1. lit settingsPath via loadSettings ;
//  2. si un hrtf_profile est present, cherche l'id correspondant dans
//     catalog et verifie le fichier via validateSofaFile ;
//  3. si tout est valide, l'utilise (usedFallback = false) ;
//  4. sinon (fichier absent, JSON invalide, id introuvable dans catalog,
//     ou fichier .sofa invalide), retombe sur un profil par defaut :
//     - le profil d'id "subject_003" s'il est present dans catalog ;
//     - sinon le premier profil de catalog (deja trie par id) ;
//     usedFallback = true, fallbackReason explique la cause precise.
// Ne fait aucune I/O console : c'est a l'appelant d'afficher fallbackReason.
// Leve std::runtime_error si catalog est vide (aucun profil disponible,
// rien a resoudre).
StartupResolution resolveStartupProfile(const std::vector<HrtfProfile>& catalog,
                                         const std::filesystem::path& settingsPath);
```

### `profile_selector.h`

```cpp
class ProfileSelector {
public:
    // Leve std::invalid_argument si profiles est vide, ou si initialId ne
    // correspond a aucun profil de profiles.
    ProfileSelector(std::vector<HrtfProfile> profiles, const std::string& initialId);

    const HrtfProfile& current() const;
    const std::vector<HrtfProfile>& profiles() const;

    void next();      // avance, boucle a la fin vers le debut
    void previous();  // recule, boucle au debut vers la fin

    // Selectionne le profil d'id donne. Retourne false sans effet si id
    // n'existe pas dans profiles (pas d'exception : appel attendu depuis une
    // future UI en reponse a une entree utilisateur, qui doit pouvoir gerer
    // un id invalide sans interrompre le programme).
    bool selectById(const std::string& id);
};
```

## Flux de données

1. `discoverProfiles(assetsHrtfDir)` (existant) — liste le catalogue.
2. `resolveStartupProfile(catalog, settingsPath)` — décide le profil actif
   au démarrage, avec repli robuste.
3. `ProfileSelector selector(catalog, resolution.profile.id)` — état en
   mémoire pour navigation/sélection future.
4. Sur nouvelle sélection (`selector.selectById(id)` ou `next()`/`previous()`
   suivi d'une confirmation) : `saveSettings({selector.current().id},
   settingsPath)`.

Ce flux ne déclenche pas `profile_installer`/`hrtf_context` — l'intégration
avec le chargement OpenAL réel reste hors scope (probablement DEV-239 ou une
tâche d'intégration ultérieure, qui appellera `installProfile` +
`HrtfContext::open()` avec le profil résolu ici).

## Gestion d'erreurs

| Situation | Comportement |
|---|---|
| Fichier settings.json absent | `loadSettings` retourne `nullopt` ; `resolveStartupProfile` retombe sur le défaut, `fallbackReason` l'indique. |
| JSON syntaxiquement invalide | Idem : `nullopt`, repli, raison explicite. |
| JSON valide mais pas un objet / clé `hrtf_profile` absente ou pas une chaîne | Traité comme un cas de repli (raison explicite), pas d'exception. |
| `hrtf_profile` référence un id absent du catalogue | Repli, raison explicite (id introuvable). |
| `hrtf_profile` référence un id présent mais `.sofa` invalide (`validateSofaFile` échoue) | Repli, raison explicite (fichier invalide). |
| Fichier settings.json présent mais illisible (permissions) | `loadSettings` retourne `nullopt` (traité comme absent) ; repli, raison explicite. |
| Catalogue entièrement vide | `resolveStartupProfile` lève `std::runtime_error` — rien à résoudre, l'appelant affiche l'erreur et s'arrête (même logique que `hrtf_check.cpp` face à un catalogue vide). |
| Dossier de config non accessible en écriture au moment du save | `saveSettings` lève `std::runtime_error` explicite ; l'appelant capture et reporte, ne crashe pas. |

Aucun de ces cas ne doit produire un crash silencieux ou un comportement
indéfini : soit un repli avec message clair, soit une exception explicite
capturée par l'appelant.

## Outil de vérification : `tools/hrtf_profile_select.cpp`

Sur le modèle de `hrtf_check.cpp` : lit `assets/hrtf` en relatif depuis la
racine du dépôt, exécutable indépendant, ne dépend d'aucun autre outil.

- **Sans argument** : appelle `discoverProfiles` + `resolveStartupProfile`,
  affiche le profil actif (et le message de repli si `usedFallback`), puis
  liste tous les profils du catalogue.
- **Avec un argument `<id>`** : comme ci-dessus, puis si `<id>` existe dans
  le catalogue, `selector.selectById(id)` + `saveSettings(...)` ; affiche la
  confirmation. Si `<id>` n'existe pas, message d'erreur explicite, code de
  sortie non nul, sans modifier le fichier settings.

Validation de la boucle complète : lancer sans argument (profil par défaut
au premier lancement), lancer avec un id différent (sélection + save),
relancer sans argument (confirme que le nouveau choix est bien restauré).

## Tests

Pas de nouveau framework de test introduit (le dépôt n'en a pas encore).
L'outil `hrtf_profile_select` couvre le cycle complet
sélection → persistance → relecture manuellement, dans la continuité de la
convention établie par DEV-206/`hrtf_check.cpp`.

## Hors scope

- Interface d'écoute interactive (son tournant, navigation au clavier/à la
  manette) — DEV-239.
- Intégration du profil résolu avec `profile_installer`/`hrtf_context` au
  démarrage réel de l'application — tâche d'intégration ultérieure.
- Autres réglages audio (volume, réverbération) — le format JSON est conçu
  pour les accueillir plus tard, mais ils ne sont pas implémentés ici.
