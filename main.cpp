#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include <cmath>
#include <windows.h>

// Pointeur fonction
LPALCRESETDEVICESOFT alcResetDeviceSOFT = nullptr;

// ─────────────────────────────────────────────
// Lecture WAV mono 16 bits
// ─────────────────────────────────────────────

struct WAVData {
    std::vector<short> samples;
    int sampleRate;
    int channels;
};

WAVData load_wav(const char* path) {
    WAVData result = {};
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        printf("Erreur : impossible d'ouvrir %s\n", path);
        return result;
    }

    char riff[4]; file.read(riff, 4);
    int fileSize; file.read((char*)&fileSize, 4);
    char wave[4]; file.read(wave, 4);
    char fmt[4];  file.read(fmt, 4);
    int  fmtSize; file.read((char*)&fmtSize, 4);

    short audioFormat, numChannels;
    int   sampleRate, byteRate;
    short blockAlign, bitsPerSample;

    file.read((char*)&audioFormat,   2);
    file.read((char*)&numChannels,   2);
    file.read((char*)&sampleRate,    4);
    file.read((char*)&byteRate,      4);
    file.read((char*)&blockAlign,    2);
    file.read((char*)&bitsPerSample, 2);

    char chunkId[4];
    int  chunkSize;
    while (file.read(chunkId, 4) && file.read((char*)&chunkSize, 4)) {
        if (strncmp(chunkId, "data", 4) == 0) break;
        file.seekg(chunkSize, std::ios::cur);
    }

    int numSamples = chunkSize / (bitsPerSample / 8);
    result.samples.resize(numSamples);
    file.read((char*)result.samples.data(), chunkSize);
    result.sampleRate = sampleRate;
    result.channels   = numChannels;
    return result;
}

// ─────────────────────────────────────────────
// Utilitaires
// ─────────────────────────────────────────────

void wait(int seconds, const char* msg) {
    printf("\n>>> %s\n", msg);
    for (int i = seconds; i > 0; i--) {
        printf("    %d seconde(s)...\r", i);
        fflush(stdout);
        Sleep(1000);
    }
    printf("\n");
}

void pause(const char* msg) {
    printf("\n--- %s ---\n", msg);
    printf("    Appuie sur Entree pour continuer...\n");
    getchar();
}

// ─────────────────────────────────────────────
// Point d'entree
// ─────────────────────────────────────────────

int main() {
    // ── Init OpenAL Soft ──
    ALCdevice*  device = alcOpenDevice(NULL);
    ALCcontext* ctx    = alcCreateContext(device, NULL);
    alcMakeContextCurrent(ctx);

    alcResetDeviceSOFT = (LPALCRESETDEVICESOFT)
        alcGetProcAddress(device, "alcResetDeviceSOFT");

    printf("OpenAL Soft : %s %s\n",
           alGetString(AL_VENDOR),
           alGetString(AL_VERSION));

    // ── Activer HRTF ──
    if (alcIsExtensionPresent(device, "ALC_SOFT_HRTF") && alcResetDeviceSOFT) {
        ALCint attrs[] = { ALC_HRTF_SOFT, ALC_TRUE, 0 };
        alcResetDeviceSOFT(device, attrs);
        ALCint hrtfStatus;
        alcGetIntegerv(device, ALC_HRTF_STATUS_SOFT, 1, &hrtfStatus);
        printf("HRTF : %s\n\n",
               hrtfStatus == ALC_HRTF_ENABLED_SOFT ? "ACTIVE" : "NON ACTIVE");
    }

    // ── Charger le WAV ──
    WAVData wav = load_wav("assets/test-audio.wav");
    if (wav.samples.empty()) return -1;

    ALuint buffer;
    alGenBuffers(1, &buffer);
    alBufferData(buffer, AL_FORMAT_MONO16,
                 wav.samples.data(),
                 (ALsizei)(wav.samples.size() * sizeof(short)),
                 wav.sampleRate);

    // ── Auditeur fixe a l'origine ──
    alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
    float orientation[] = { 0.0f, 0.0f, -1.0f,
                             0.0f, 1.0f,  0.0f };
    alListenerfv(AL_ORIENTATION, orientation);

    printf("=== METS TES ECOUTEURS ===\n");
    pause("Pret a commencer ?");

    // ════════════════════════════════════════
    // TEST 1 — Localisation gauche / droite
    // ════════════════════════════════════════
    printf("\n========================================\n");
    printf("  TEST 1 — Localisation gauche / droite\n");
    printf("  But : verifier que le son est bien\n");
    printf("  localise des deux cotes (R-ACC-04)\n");
    printf("========================================\n");

    ALuint src1;
    alGenSources(1, &src1);
    alSourcei(src1, AL_BUFFER,  buffer);
    alSourcei(src1, AL_LOOPING, AL_TRUE);
    alSourcef(src1, AL_GAIN,    1.0f);
    alSourcePlay(src1);

    alSource3f(src1, AL_POSITION, -10.0f, 0.0f, 0.0f);
    wait(4, "Totalement a GAUCHE");

    alSource3f(src1, AL_POSITION, 10.0f, 0.0f, 0.0f);
    wait(4, "Totalement a DROITE");

    alSourceStop(src1);
    alDeleteSources(1, &src1);
    pause("TEST 1 termine. Continuer ?");

    // ════════════════════════════════════════
    // TEST 2 — 4 sources simultanees (R-AUD-03)
    // ════════════════════════════════════════
    printf("\n========================================\n");
    printf("  TEST 2 — 4 sources simultanees\n");
    printf("  But : valider R-AUD-03 — pas de coupure\n");
    printf("  ni d'artefact avec 4 acteurs en meme temps\n");
    printf("========================================\n");
    printf("\n  Positions des 4 sources :\n");
    printf("  Source 1 : devant droite  ( 3,  0,  5)\n");
    printf("  Source 2 : devant gauche  (-2,  0,  4)\n");
    printf("  Source 3 : derriere haut  ( 1,  1, -3)\n");
    printf("  Source 4 : gauche proche  (-4,  0,  1)\n\n");

    ALuint sources4[4];
    alGenSources(4, sources4);

    float positions4[4][3] = {
        {  3.0f, 0.0f,  5.0f },
        { -2.0f, 0.0f,  4.0f },
        {  1.0f, 1.5f, -3.0f },
        { -4.0f, 0.0f,  1.0f },
    };

    for (int i = 0; i < 4; i++) {
        alSourcei (sources4[i], AL_BUFFER,  buffer);
        alSourcei (sources4[i], AL_LOOPING, AL_TRUE);
        alSourcef (sources4[i], AL_GAIN,    0.5f);
        alSource3f(sources4[i], AL_POSITION,
                   positions4[i][0],
                   positions4[i][1],
                   positions4[i][2]);
    }

    // Demarrer les 4 sources une par une pour bien les entendre
    printf(">>> Ajout progressif des sources\n");
    for (int i = 0; i < 4; i++) {
        printf("    Source %d ajoutee\n", i + 1);
        alSourcePlay(sources4[i]);
        Sleep(2000);
    }

    wait(4, "4 sources actives simultanement — ecoute les artefacts");

    for (int i = 0; i < 4; i++) alSourceStop(sources4[i]);
    alDeleteSources(4, sources4);
    pause("TEST 2 termine. Continuer ?");

    // ════════════════════════════════════════
    // TEST 3 — Mouvement : sweep + cercle
    // ════════════════════════════════════════
    printf("\n========================================\n");
    printf("  TEST 3 — Suivi d'un acteur en mouvement\n");
    printf("  But : simuler un ennemi qui se deplace\n");
    printf("  autour du joueur (cas d'usage NATHAN)\n");
    printf("========================================\n");

    ALuint src3;
    alGenSources(1, &src3);
    alSourcei(src3, AL_BUFFER,  buffer);
    alSourcei(src3, AL_LOOPING, AL_TRUE);
    alSourcef(src3, AL_GAIN,    1.0f);
    alSourcePlay(src3);

    // Sweep droite -> gauche
    printf("\n>>> Sweep DROITE -> GAUCHE\n");
    for (int i = 0; i <= 100; i++) {
        float x = 10.0f - (20.0f * i / 100.0f);
        alSource3f(src3, AL_POSITION, x, 0.0f, 0.0f);
        Sleep(40);
    }

    // Cercle complet — 2 tours
    printf(">>> Cercle complet — 2 tours\n");
    for (int i = 0; i <= 720; i++) {
        float angle = i * 3.14159265f / 180.0f;
        float x =  sinf(angle) * 5.0f;
        float z = -cosf(angle) * 5.0f;
        alSource3f(src3, AL_POSITION, x, 0.0f, z);
        Sleep(10);
    }

    alSourceStop(src3);
    alDeleteSources(1, &src3);
    pause("TEST 3 termine. Continuer ?");

    // ════════════════════════════════════════
    // TEST 4 — Attenuation par distance
    // ════════════════════════════════════════
    printf("\n========================================\n");
    printf("  TEST 4 — Attenuation par distance\n");
    printf("  But : un acteur s'approche de 20 a 0\n");
    printf("  unites — le son doit augmenter\n");
    printf("========================================\n");

    ALuint src4;
    alGenSources(1, &src4);
    alSourcei(src4, AL_BUFFER,         buffer);
    alSourcei(src4, AL_LOOPING,        AL_TRUE);
    alSourcef(src4, AL_GAIN,           1.0f);
    alSourcef(src4, AL_ROLLOFF_FACTOR, 1.0f);
    alSourcef(src4, AL_REFERENCE_DISTANCE, 2.0f);
    alSourcef(src4, AL_MAX_DISTANCE,   25.0f);
    alSourcePlay(src4);

    // Approche depuis loin (20 unites) jusqu'au joueur (1 unite)
    printf("\n>>> Acteur qui s'approche (20 -> 1 unite devant)\n");
    for (int i = 0; i <= 100; i++) {
        float z = -(20.0f - (19.0f * i / 100.0f)); // -20 -> -1
        alSource3f(src4, AL_POSITION, 0.0f, 0.0f, z);
        if (i % 20 == 0)
            printf("    Distance : %.1f unites\n", fabsf(z));
        Sleep(60);
    }

    // Eloignement jusqu'a 20 unites
    printf(">>> Acteur qui s'eloigne (1 -> 20 unites derriere)\n");
    for (int i = 0; i <= 100; i++) {
        float z = 1.0f + (19.0f * i / 100.0f); // 1 -> 20
        alSource3f(src4, AL_POSITION, 0.0f, 0.0f, z);
        if (i % 20 == 0)
            printf("    Distance : %.1f unites\n", fabsf(z));
        Sleep(60);
    }

    alSourceStop(src4);
    alDeleteSources(1, &src4);
    pause("TEST 4 termine. Continuer ?");

    // ════════════════════════════════════════
    // TEST 5 — Avant vs Arriere (front/back)
    // ════════════════════════════════════════
    printf("\n========================================\n");
    printf("  TEST 5 — Confusion avant / arriere\n");
    printf("  But : valider que HRTF distingue bien\n");
    printf("  devant et derriere (critique pour aveugles)\n");
    printf("========================================\n");
    printf("\n  Note : les HRTFs generiques ont parfois\n");
    printf("  de la difficulte a distinguer avant/arriere.\n");
    printf("  Ecoute attentivement la difference.\n\n");

    ALuint src5;
    alGenSources(1, &src5);
    alSourcei(src5, AL_BUFFER,  buffer);
    alSourcei(src5, AL_LOOPING, AL_TRUE);
    alSourcef(src5, AL_GAIN,    1.0f);
    alSourcePlay(src5);

    // Devant
    alSource3f(src5, AL_POSITION, 0.0f, 0.0f, -5.0f);
    wait(4, "Son DEVANT (z = -5)");

    // Derriere — meme azimuth, meme elevation, direction opposee
    alSource3f(src5, AL_POSITION, 0.0f, 0.0f, 5.0f);
    wait(4, "Son DERRIERE (z = +5) — entends-tu la difference ?");

    // Alternance rapide avant/arriere
    printf("\n>>> Alternance rapide DEVANT / DERRIERE\n");
    for (int i = 0; i < 6; i++) {
        if (i % 2 == 0) {
            alSource3f(src5, AL_POSITION, 0.0f, 0.0f, -5.0f);
            printf("    DEVANT\n");
        } else {
            alSource3f(src5, AL_POSITION, 0.0f, 0.0f, 5.0f);
            printf("    DERRIERE\n");
        }
        Sleep(1500);
    }

    alSourceStop(src5);
    alDeleteSources(1, &src5);

    // ── Nettoyage ──
    alDeleteBuffers(1, &buffer);
    alcMakeContextCurrent(NULL);
    alcDestroyContext(ctx);
    alcCloseDevice(device);

    printf("\n========================================\n");
    printf("  TOUS LES TESTS TERMINES\n");
    printf("  Notes a prendre :\n");
    printf("  - Test 2 : artefacts audibles ? O/N\n");
    printf("  - Test 4 : attenuation naturelle ? O/N\n");
    printf("  - Test 5 : avant/arriere distinguable ? O/N\n");
    printf("========================================\n\n");
    printf("Appuie sur Entree pour quitter...\n");
    getchar();
    return 0;
}