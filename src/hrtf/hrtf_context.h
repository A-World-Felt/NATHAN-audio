#pragma once

// Ouverture d'un device+contexte OpenAL avec le rendu HRTF force
// (ALC_SOFT_HRTF), et verification que le pilote l'a effectivement active
// (ALC_HRTF_STATUS_SOFT). Le profil a utiliser doit deja avoir ete installe
// dans le dossier hrtf-paths d'OpenAL Soft (voir profile_installer.h) avant
// l'appel a open().

#include <AL/al.h>
#include <AL/alc.h>

#include <string>

namespace nathan::hrtf {

enum class HrtfStatus {
    Disabled,
    Enabled,
    Denied,
    Required,
    HeadphonesDetected,
    UnsupportedFormat,
    Unknown,
};

std::string toString(HrtfStatus status);

class HrtfContext {
public:
    HrtfContext() = default;
    ~HrtfContext();

    HrtfContext(const HrtfContext&) = delete;
    HrtfContext& operator=(const HrtfContext&) = delete;

    // Ouvre le device par defaut avec ALC_HRTF_SOFT = ALC_TRUE, puis lit
    // ALC_HRTF_STATUS_SOFT. Leve std::runtime_error si :
    //  - l'ouverture du device ou du contexte echoue ;
    //  - l'extension ALC_SOFT_HRTF n'est pas supportee par ce device ;
    //  - le statut lu apres ouverture n'est pas ALC_HRTF_ENABLED_SOFT
    //    (message explicite incluant le statut retourne par le pilote).
    // Detruit un device/contexte deja ouvert avant d'en recreer un.
    void open();
    void close();

    ALCdevice* device() const { return device_; }
    ALCcontext* context() const { return context_; }

    // Relit ALC_HRTF_STATUS_SOFT sur le device courant.
    HrtfStatus status() const;

private:
    ALCdevice* device_ = nullptr;
    ALCcontext* context_ = nullptr;
};

}  // namespace nathan::hrtf
