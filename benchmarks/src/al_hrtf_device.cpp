#include "al_hrtf_device.h"

#include <AL/alext.h>

#include <stdexcept>

void AlHrtfDevice::open() {
    close();

    device_ = alcOpenDevice(nullptr);
    if (!device_) {
        throw std::runtime_error("alcOpenDevice a echoue");
    }

    if (alcIsExtensionPresent(device_, "ALC_SOFT_HRTF") == ALC_FALSE) {
        alcCloseDevice(device_);
        device_ = nullptr;
        throw std::runtime_error("L'extension ALC_SOFT_HRTF n'est pas supportee par ce device");
    }

    ALCint attribs[] = {
        ALC_HRTF_SOFT, ALC_TRUE,
        ALC_HRTF_ID_SOFT, 0,  // un seul fichier .sofa dans le dossier -> ID 0 = notre profil
        0,
    };
    context_ = alcCreateContext(device_, attribs);
    if (!context_) {
        alcCloseDevice(device_);
        device_ = nullptr;
        throw std::runtime_error("alcCreateContext a echoue");
    }
    alcMakeContextCurrent(context_);
}

void AlHrtfDevice::close() {
    if (context_) {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context_);
        context_ = nullptr;
    }
    if (device_) {
        alcCloseDevice(device_);
        device_ = nullptr;
    }
}

AlHrtfDevice::~AlHrtfDevice() { close(); }

std::string AlHrtfDevice::hrtfStatusString() const {
    if (!device_) return "device ferme";
    ALCint status = 0;
    alcGetIntegerv(device_, ALC_HRTF_STATUS_SOFT, 1, &status);
    switch (status) {
        case ALC_HRTF_ENABLED_SOFT: return "actif";
        case ALC_HRTF_DENIED_SOFT: return "refuse par le pilote";
        case ALC_HRTF_REQUIRED_SOFT: return "requis mais indisponible";
        case ALC_HRTF_UNSUPPORTED_FORMAT_SOFT: return "format non supporte";
        case ALC_HRTF_DISABLED_SOFT: return "desactive";
        default: return "inconnu (" + std::to_string(status) + ")";
    }
}
