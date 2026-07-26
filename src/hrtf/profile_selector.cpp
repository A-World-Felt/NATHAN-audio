#include "profile_selector.h"

#include <stdexcept>

namespace nathan::hrtf {

ProfileSelector::ProfileSelector(std::vector<HrtfProfile> profiles, const std::string& initialId)
    : profiles_(std::move(profiles)) {
    if (profiles_.empty()) {
        throw std::invalid_argument("ProfileSelector: la liste de profils ne peut pas etre vide");
    }

    bool found = false;
    for (std::size_t i = 0; i < profiles_.size(); ++i) {
        if (profiles_[i].id == initialId) {
            currentIndex_ = i;
            found = true;
            break;
        }
    }
    if (!found) {
        throw std::invalid_argument("ProfileSelector: profil initial '" + initialId +
                                     "' introuvable dans la liste de profils");
    }
}

const HrtfProfile& ProfileSelector::current() const { return profiles_[currentIndex_]; }

void ProfileSelector::next() { currentIndex_ = (currentIndex_ + 1) % profiles_.size(); }

void ProfileSelector::previous() {
    currentIndex_ = (currentIndex_ == 0) ? profiles_.size() - 1 : currentIndex_ - 1;
}

bool ProfileSelector::selectById(const std::string& id) {
    for (std::size_t i = 0; i < profiles_.size(); ++i) {
        if (profiles_[i].id == id) {
            currentIndex_ = i;
            return true;
        }
    }
    return false;
}

}  // namespace nathan::hrtf
