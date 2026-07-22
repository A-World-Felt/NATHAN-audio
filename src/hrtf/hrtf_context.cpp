#include "hrtf_context.h"

#include <AL/alext.h>

#include <stdexcept>

namespace nathan::hrtf {

std::string toString(HrtfStatus status) {
    switch (status) {
        case HrtfStatus::Disabled: return "desactive";
        case HrtfStatus::Enabled: return "actif";
        case HrtfStatus::Denied: return "refuse par le pilote";
        case HrtfStatus::Required: return "requis mais indisponible";
        case HrtfStatus::HeadphonesDetected: return "casque detecte (HRTF pas encore actif)";
        case HrtfStatus::UnsupportedFormat: return "format audio non supporte pour le HRTF";
        default: return "statut inconnu";
    }
}

namespace {

HrtfStatus toHrtfStatus(ALCint raw) {
    switch (raw) {
        case ALC_HRTF_DISABLED_SOFT: return HrtfStatus::Disabled;
        case ALC_HRTF_ENABLED_SOFT: return HrtfStatus::Enabled;
        case ALC_HRTF_DENIED_SOFT: return HrtfStatus::Denied;
        case ALC_HRTF_REQUIRED_SOFT: return HrtfStatus::Required;
        case ALC_HRTF_HEADPHONES_DETECTED_SOFT: return HrtfStatus::HeadphonesDetected;
        case ALC_HRTF_UNSUPPORTED_FORMAT_SOFT: return HrtfStatus::UnsupportedFormat;
        default: return HrtfStatus::Unknown;
    }
}

}  // namespace

void HrtfContext::open() {
    close();

    device_ = alcOpenDevice(nullptr);
    if (!device_) {
        throw std::runtime_error("alcOpenDevice a echoue : aucun device audio disponible");
    }

    if (alcIsExtensionPresent(device_, "ALC_SOFT_HRTF") == ALC_FALSE) {
        alcCloseDevice(device_);
        device_ = nullptr;
        throw std::runtime_error("L'extension ALC_SOFT_HRTF n'est pas supportee par ce device");
    }

    const ALCint attribs[] = {
        ALC_HRTF_SOFT, ALC_TRUE,
        0,
    };
    context_ = alcCreateContext(device_, attribs);
    if (!context_) {
        alcCloseDevice(device_);
        device_ = nullptr;
        throw std::runtime_error("alcCreateContext a echoue");
    }
    alcMakeContextCurrent(context_);

    const HrtfStatus s = status();
    if (s != HrtfStatus::Enabled) {
        const std::string message = "HRTF non actif apres ouverture du contexte : " + toString(s);
        close();
        throw std::runtime_error(message);
    }
}

void HrtfContext::close() {
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

HrtfContext::~HrtfContext() { close(); }

HrtfStatus HrtfContext::status() const {
    if (!device_) return HrtfStatus::Unknown;
    ALCint raw = 0;
    alcGetIntegerv(device_, ALC_HRTF_STATUS_SOFT, 1, &raw);
    return toHrtfStatus(raw);
}

}  // namespace nathan::hrtf
