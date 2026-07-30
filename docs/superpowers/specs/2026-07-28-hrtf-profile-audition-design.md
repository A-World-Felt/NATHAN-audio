# DEV-239 — Interface de sélection sonore (son tournant + navigation)

## Contexte

`src/hrtf/` fournit déjà le chargement des profils HRTF CIPIC (DEV-167) et
la sélection/persistance du choix utilisateur (DEV-238) :
`discoverProfiles`/`HrtfProfile` (`profile_catalog`), `ProfileSelector`
(`profile_selector`), `installProfile`/`validateSofaFile`
(`profile_installer`/`sofa_validator`), `HrtfContext` (`hrtf_context`),
`openalHrtfDirectory`/`nathanConfigDirectory` (`openal_paths`/
`config_paths`), `saveSettings`/`resolveStartupProfile`
(`profile_settings`).

Cette tâche est le dernier maillon de la chaîne DEV-167 → DEV-238 → DEV-239 :
elle relie ces briques à une interface d'écoute interactive, sur le modèle
déjà validé par le prototype `benchmarks/profile_selector.cpp`, mais portée
sur les modules de production plutôt que sur `benchmarks/src/` (qui reste un
prototype figé, non touché) et sur `settings.json` plutôt que sur
`user_profile.txt`.

La console NATHAN s'adresse à des personnes malvoyantes : le choix du profil
HRTF se fait à l'oreille (un son de test tourne autour de la tête), pas via
un écran.

## Architecture

Quatre nouveaux fichiers dans `src/hrtf/`, ajoutés à `nathan_hrtf`
(`src/CMakeLists.txt`), plus un outil de vérification :

- `wav_mono16.h/.cpp` — chargeur WAV PCM mono 16/24 bits, sortie directe en
  `int16_t` (prêt pour `AL_FORMAT_MONO16`), contrairement à
  `benchmarks/src/wav_loader.h` qui normalise en `float`.
- `audition_input.h/.cpp` — abstraction d'entrée (`AuditionInput`,
  `AuditionCommand`) pour découpler la boucle d'écoute du périphérique
  physique. `KeyboardAuditionInput` est la première implémentation ; les
  boutons GPIO du MPU en brancheront une autre plus tard sans toucher à
  `profile_audition`.
- `active_profile.h/.cpp` — point unique qui rend un profil audible :
  `applyProfile()` = `installProfile()` puis `HrtfContext::open()`. Évite de
  dupliquer cette séquence entre les outils et l'interface d'écoute.
- `profile_audition.h/.cpp` — la boucle d'écoute elle-même
  (`runProfileAudition`).

Plus `tools/hrtf_profile_listen.cpp`, sur le modèle de
`tools/hrtf_profile_select.cpp` et `tools/hrtf_check.cpp`.

## Le point clé : bascule de profil à chaud

`installProfile` copie un seul `.sofa` dans le dossier scanné par OpenAL
Soft (il supprime les autres au passage — ce fichier *est* le HRTF actif,
sans ambiguïté d'index). `HrtfContext::open()` referme puis rouvre le
device pour que le pilote recharge ce fichier et vérifie
`ALC_HRTF_STATUS_SOFT`.

Conséquence directe : rouvrir le device **détruit** le contexte OpenAL en
cours, donc toute source et tout buffer créés avant cet appel deviennent
invalides. `profile_audition` recrée systématiquement le son tournant
(buffer + source, `alSourcePlay`) juste après chaque appel à
`applyProfile()`. Les anciens identifiants ne sont jamais explicitement
libérés après une bascule : ils ont disparu avec le device qui les
possédait, les libérer serait un appel sur un device déjà fermé.

La phase de rotation n'est pas réinitialisée à chaque bascule : l'azimuth
est recalculé à chaque frame à partir du temps écoulé depuis le début de la
boucle (`steady_clock`), pas depuis la dernière bascule — le son ne
« repart pas de zéro ».

Ce mécanisme est délibérément différent de celui du prototype
`benchmarks/src/al_hrtf_device.cpp`, qui utilisait
`ALC_HRTF_ID_SOFT` = 0 en plus de `ALC_HRTF_SOFT` = `ALC_TRUE`. Avec un seul
fichier dans le dossier, l'index est de toute façon toujours 0 : le
paramètre est redondant et supprimé côté production (`HrtfContext::open()`,
DEV-167, ne le pose déjà pas).

## Composants

### `wav_mono16.h`

```cpp
struct PcmMono16 {
    std::vector<int16_t> samples;
    int sampleRate = 0;
};

PcmMono16 loadWavMono16(const std::filesystem::path& path);
```

Parcours RIFF générique (chunks ignorés hors `fmt `/`data`), identique dans
sa structure à `benchmarks/src/wav_loader.cpp`. Différence : le 24 bits est
tronqué directement vers 16 bits (les deux octets de poids fort de
l'échantillon signé) au lieu de passer par une normalisation flottante
intermédiaire — la destination finale est toujours un buffer
`AL_FORMAT_MONO16`, la conversion en `float` du prototype n'apportait rien
ici. Lève `std::runtime_error` si le fichier est absent/illisible,
non-PCM, non-mono, ou pas en 16/24 bits.

### `audition_input.h`

```cpp
enum class AuditionCommand { None, Next, Previous, Confirm, Cancel };

class AuditionInput {
public:
    virtual ~AuditionInput() = default;
    virtual AuditionCommand poll() = 0;  // non bloquant
};

class KeyboardAuditionInput : public AuditionInput {
public:
    KeyboardAuditionInput();
    ~KeyboardAuditionInput() override;
    AuditionCommand poll() override;
    // ...
};
```

Clavier : `←` = Previous, `→` = Next, `Entrée` = Confirm, `Échap`/`Q` =
Cancel. Reprend la logique de `benchmarks/src/input_keys.cpp` (conio sous
Windows avec préfixe `0`/`0xE0` pour les flèches ; termios en mode brut +
`select` sous POSIX avec parsing `ESC[C`/`ESC[D`), avec deux différences
délibérées :

- API en classe (`AuditionInput`/`KeyboardAuditionInput`) plutôt qu'une
  fonction libre : `profile_audition` dépend de l'abstraction, pas d'une
  implémentation clavier précise — un futur `GpioAuditionInput` s'y
  substituera sans changer `runProfileAudition`.
- Sous POSIX, le mode brut du terminal est une ressource RAII **membre de
  l'instance** (constructeur/destructeur de `KeyboardAuditionInput::Impl`),
  pas un singleton statique de fonction comme dans le prototype : le cycle
  de vie du terminal suit celui de l'objet, pas du process entier.

### `active_profile.h`

```cpp
void applyProfile(const HrtfProfile& profile,
                   const std::filesystem::path& openalHrtfDir,
                   HrtfContext& context);
```

`installProfile(profile, openalHrtfDir)` puis `context.open()`. Documente
explicitement dans le header que `open()` détruit le contexte précédent :
tout objet OpenAL créé avant l'appel doit être recréé après.

### `profile_audition.h`

```cpp
struct AuditionConfig {
    std::filesystem::path testWavPath;
    std::filesystem::path openalHrtfDir;
    std::filesystem::path settingsPath;
    double rotationPeriodSec = 6.0;
    float radiusMeters = 1.5f;
    int frameIntervalMs = 20;
};

enum class AuditionOutcome { Confirmed, Cancelled };

struct AuditionResult {
    AuditionOutcome outcome;
    std::string activeProfileId;
    std::string persistedProfileId;  // vide si Cancelled
};

AuditionResult runProfileAudition(ProfileSelector& selector, AuditionInput& input,
                                   const AuditionConfig& config,
                                   std::FILE* out = stdout);
```

Boucle bloquante jusqu'à Confirm ou Cancel :

1. Au démarrage : `applyProfile(selector.current(), ...)`. Si même ce
   profil de départ échoue à activer le HRTF, l'exception de
   `applyProfile` remonte telle quelle — device inutilisable, rien à
   proposer à l'utilisateur.
2. À chaque frame : azimuth recalculé depuis `steady_clock` (pas
   réinitialisé par les bascules), `alSource3f` sur la source courante.
3. `poll()` de `input` :
   - **Next/Previous** : `selector.next()`/`previous()`, puis
     `applyProfile` + recréation du son. Si `applyProfile` échoue, retour
     au profil précédent (`selectById` + nouvel `applyProfile`) sans
     arrêter la boucle ; si ce repli échoue aussi, l'exception remonte
     (device inutilisable, on ne peut plus rien garantir).
   - **Confirm** : `saveSettings(AppSettings{selector.current().id},
     settingsPath)`, retourne `Confirmed`.
   - **Cancel** : n'écrit rien dans `settings.json` ; si le profil actif a
     changé depuis le départ, réinstalle le `.sofa` du profil de départ
     côté OpenAL (cohérence du dossier avec ce qu'annonçait le démarrage) ;
     retourne `Cancelled`.

## Outil de vérification : `tools/hrtf_profile_listen.cpp`

Sur le modèle de `hrtf_profile_select.cpp` : `discoverProfiles("assets/hrtf")`,
`resolveStartupProfile` pour le point de départ, `ProfileSelector`,
`AuditionConfig` (`testWavPath = "assets/test-audio.wav"`,
`openalHrtfDir = openalHrtfDirectory()`,
`settingsPath = nathanConfigDirectory()/"settings.json"`),
`KeyboardAuditionInput`. Code de sortie : `0` = confirmé, `2` = abandonné,
`1` = erreur.

## Vérification effectuée

- Compilation `/W4 /WX` propre des quatre nouveaux fichiers `src/hrtf/` et
  de `hrtf_profile_listen.cpp` (aucun avertissement).
- Build complet via CMake/MSBuild (Debug) : `nathan_hrtf` + tous les outils.
- `hrtf_profile_listen` lancé depuis la racine du dépôt : découverte de 45
  profils, repli correct sur `subject_003` en l'absence de
  `settings.json`, activation HRTF réussie (`Profil actif : subject_003`).
- Cycle complet rejoué via un driver scripté (`AuditionInput` de test
  injectant Next/Next/Previous/Confirm puis Next/Next/Previous/Cancel,
  hors dépôt, contre les mêmes bibliothèques de production) :
  - Confirm : bascules successives `subject_003 → subject_008 →
    subject_009 → subject_008`, `settings.json` mis à jour à
    `subject_008`.
  - Relance : `resolveStartupProfile` repart bien de `subject_008`
    (persistance confirmée).
  - Cancel : bascules `subject_008 → subject_009 → subject_010 →
    subject_009`, sortie sans modifier `settings.json` (resté à
    `subject_008`) et `.sofa` réinstallé côté OpenAL = `subject_008.sofa`
    (cohérence du dossier restaurée).

## Hors scope

- Implémentation d'entrée GPIO pour le MPU — seule l'abstraction
  `AuditionInput` est posée ici.
- Autres réglages audio (volume, réverbération) dans `settings.json` — le
  format reste celui défini par DEV-238.
- Intégration de `runProfileAudition` dans un flux de démarrage applicatif
  plus large (menu, autres écrans) — hors périmètre de la chaîne
  DEV-167 → DEV-238 → DEV-239.
