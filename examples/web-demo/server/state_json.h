#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "frame_stats.h"

namespace web_demo {

struct SourceState {
    std::string id;
    std::string label;
    std::string asset;
    std::string format;  // "mp3" ou "wav"
    float posX = 0.0f;
    float posZ = 0.0f;
    float distanceM = 0.0f;
    float azimuthDeg = 0.0f;
    float gain = 0.0f;
};

struct ProfileState {
    std::string active;
    std::string defaultId;
    bool usedFallback = false;
    std::string fallbackReason;
};

struct HrtfState {
    std::string status;
    std::string renderer;
    std::string version;
    std::string vendor;
    int sampleRateHz = 0;
};

struct Mp3State {
    std::string path;
    std::size_t fileSizeBytes = 0;
    int sampleRateHz = 0;
    int channels = 0;
    double durationSec = 0.0;
    std::size_t totalSamples = 0;
};

struct DemoState {
    float playerX = 0.0f;
    float playerZ = 0.0f;
    ProfileState profile;
    HrtfState hrtf;
    std::vector<SourceState> sources;
    FrameStatsSnapshot frameStats;
    double frameBudgetMs = 15.0;
    double fpsReal = 0.0;
    std::size_t framesProcessed = 0;
    Mp3State mp3;
};

// Forme exacte documentee dans docs/superpowers/specs/2026-08-06-web-audio-demo-design.md
// (section "API HTTP", GET /api/state).
std::string buildStateJson(const DemoState& state);

// { "profiles": [...], "active": "..." }
std::string buildProfilesJson(const std::vector<std::string>& profiles, const std::string& activeId);

struct PlayerRequest {
    float x = 0.0f;
    float z = 0.0f;
};

// Retourne false si le corps n'est pas un JSON valide avec x et z numeriques.
bool parsePlayerBody(const std::string& body, PlayerRequest& out);

// Retourne false si le corps n'est pas un JSON valide avec un champ "id" string.
bool parseProfileBody(const std::string& body, std::string& outId);

}  // namespace web_demo
