#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace web_demo {

struct ProfileSwitchRequest {
    std::string requestedId;
    bool completed = false;
    bool success = false;
    std::string errorMessage;
};

// Etat partage entre le thread HTTP (httplib) et la boucle audio du thread
// principal. Le thread HTTP n'appelle JAMAIS OpenAL directement : il depose
// des demandes ici, la boucle principale les applique (voir
// docs/superpowers/specs/2026-08-06-web-audio-demo-design.md, "Boucle
// principale et threading").
struct SharedState {
    std::mutex mutex;
    std::condition_variable profileSwitchDone;

    // Ecrit par le thread HTTP, lu par la boucle principale.
    float requestedPlayerX = 0.0f;
    float requestedPlayerZ = 0.0f;
    std::optional<ProfileSwitchRequest> pendingProfileSwitch;

    // Ecrit par la boucle principale, lu par le thread HTTP.
    std::string latestStateJson;
    std::vector<std::string> catalogIds;   // immuable apres le demarrage
    std::string activeProfileId;

    bool shuttingDown = false;
};

}  // namespace web_demo
