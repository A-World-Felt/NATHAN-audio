#include "active_profile.h"

#include "profile_installer.h"

namespace nathan::hrtf {

void applyProfile(const HrtfProfile& profile, const std::filesystem::path& openalHrtfDir,
                   HrtfContext& context) {
    installProfile(profile, openalHrtfDir);
    context.open();
}

}  // namespace nathan::hrtf
