#pragma once

// Cycle de vie device+contexte OpenAL avec HRTF force via l'extension
// ALC_SOFT_HRTF. Aucune convolution/DSP ici : OpenAL Soft lit lui-meme le
// fichier .sofa deja copie dans son dossier hrtf-paths (voir hrtf_profile.h)
// et fait toute la convolution HRTF en interne.

#include <AL/al.h>
#include <AL/alc.h>

#include <string>

class AlHrtfDevice {
public:
    AlHrtfDevice() = default;
    ~AlHrtfDevice();

    AlHrtfDevice(const AlHrtfDevice&) = delete;
    AlHrtfDevice& operator=(const AlHrtfDevice&) = delete;

    // Ouvre (ou reouvre - "redemarre le contexte") le device + contexte
    // avec ALC_HRTF_SOFT=ALC_TRUE. Detruit l'ancien contexte/device s'il y
    // en avait un. Leve std::runtime_error en cas d'echec ou si
    // ALC_SOFT_HRTF n'est pas supporte.
    void open();
    void close();

    ALCdevice* device() const { return device_; }
    ALCcontext* context() const { return context_; }

    // Diagnostic lisible : "actif", "refuse", etc. (voir ALC_HRTF_STATUS_SOFT).
    std::string hrtfStatusString() const;

private:
    ALCdevice* device_ = nullptr;
    ALCcontext* context_ = nullptr;
};
