#pragma once

#include <AL/al.h>

#include <cstdint>
#include <string>

#include "dr_mp3.h"  // types drmp3 (l'implementation reste dans mp3_loader.cpp)

// Nombre de buffers OpenAL utilises en rotation. 3 est un bon compromis :
// pendant qu'un buffer joue, on peut en decoder/remplir un autre a l'avance,
// avec une marge si une frame de mise a jour est en retard.
constexpr int kStreamBufferCount = 3;

// Taille d'un chunk decode, en frames PCM (pas en octets). A ajuster selon
// la latence vs. la frequence d'appel de mp3_stream_update : plus petit =
// moins de RAM par chunk mais mise a jour plus frequente necessaire.
constexpr drmp3_uint64 kStreamFramesPerChunk = 4096;

// Etat d'un flux MP3 en cours de lecture par streaming. Une instance par
// source audio a streamer.
struct Mp3Stream {
    drmp3 decoder{};
    bool decoderOpen = false;
    uint32_t channels = 0;
    uint32_t sampleRate = 0;
    ALuint buffers[kStreamBufferCount] = {};
    bool loop = true;
    bool finished = false;  // vrai si (loop == false) et le flux est arrive au bout
};

// Ouvre le fichier MP3 et prepare le decodeur + les buffers OpenAL.
// Ne lance pas la lecture (voir mp3_stream_start). Retourne false en cas
// d'echec d'ouverture du fichier.
bool mp3_stream_open(Mp3Stream& stream, const std::string& path, bool loop = true);

// Remplit et met en file d'attente les buffers initiaux sur la source
// donnee, puis demarre la lecture. A appeler une fois, juste apres
// mp3_stream_open. Ne pas activer AL_LOOPING sur la source : le bouclage
// est gere manuellement par ce module.
void mp3_stream_start(Mp3Stream& stream, ALuint source);

// A appeler a chaque frame/tick de la boucle de jeu : libere les buffers
// deja joues par OpenAL, decode les chunks suivants et les remet en file.
// Gere le bouclage (retour au debut du fichier) si stream.loop == true.
// Relance aussi la lecture si la source s'est arretee faute de buffers
// (sous-alimentation, ex. apres un ralentissement ponctuel).
void mp3_stream_update(Mp3Stream& stream, ALuint source);

// Libere le decodeur dr_mp3 et les buffers OpenAL associes. A appeler une
// fois la lecture terminee / avant de detruire la source.
void mp3_stream_close(Mp3Stream& stream);
