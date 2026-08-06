#include "state_json.h"

#include <nlohmann/json.hpp>

namespace web_demo {

using json = nlohmann::json;

std::string buildStateJson(const DemoState& state) {
    json sourcesJson = json::array();
    for (const auto& s : state.sources) {
        sourcesJson.push_back({
            {"id", s.id},
            {"label", s.label},
            {"asset", s.asset},
            {"format", s.format},
            {"pos", {{"x", s.posX}, {"z", s.posZ}}},
            {"distanceM", s.distanceM},
            {"azimuthDeg", s.azimuthDeg},
            {"gain", s.gain},
        });
    }

    double cpuBudgetPercent = state.frameStats.meanUs > 0.0
        ? (state.frameStats.meanUs / (state.frameBudgetMs * 1000.0)) * 100.0
        : 0.0;

    json j = {
        {"player", {{"x", state.playerX}, {"z", state.playerZ}}},
        {"profile", {
            {"active", state.profile.active},
            {"default", state.profile.defaultId},
            {"usedFallback", state.profile.usedFallback},
            {"fallbackReason", state.profile.fallbackReason},
        }},
        {"hrtf", {
            {"status", state.hrtf.status},
            {"renderer", state.hrtf.renderer},
            {"version", state.hrtf.version},
            {"vendor", state.hrtf.vendor},
            {"sampleRateHz", state.hrtf.sampleRateHz},
        }},
        {"sources", sourcesJson},
        {"metrics", {
            {"framesProcessed", state.framesProcessed},
            {"frameTimeUs", {
                {"last", state.frameStats.lastUs},
                {"mean", state.frameStats.meanUs},
                {"min", state.frameStats.minUs},
                {"max", state.frameStats.maxUs},
                {"p95", state.frameStats.p95Us},
            }},
            {"frameBudgetMs", state.frameBudgetMs},
            {"fpsReal", state.fpsReal},
            {"cpuBudgetPercent", cpuBudgetPercent},
            {"sourcesSimultaneous", state.sources.size()},
            {"meetsRAud03", state.sources.size() >= 4},
        }},
        {"mp3", {
            {"path", state.mp3.path},
            {"fileSizeBytes", state.mp3.fileSizeBytes},
            {"sampleRateHz", state.mp3.sampleRateHz},
            {"channels", state.mp3.channels},
            {"durationSec", state.mp3.durationSec},
            {"totalSamples", state.mp3.totalSamples},
        }},
    };
    return j.dump();
}

std::string buildProfilesJson(const std::vector<std::string>& profiles, const std::string& activeId) {
    json j = {
        {"profiles", profiles},
        {"active", activeId},
    };
    return j.dump();
}

bool parsePlayerBody(const std::string& body, PlayerRequest& out) {
    json j;
    try {
        j = json::parse(body);
    } catch (const json::parse_error&) {
        return false;
    }
    if (!j.is_object() || !j.contains("x") || !j.contains("z")) return false;
    if (!j["x"].is_number() || !j["z"].is_number()) return false;
    out.x = j["x"].get<float>();
    out.z = j["z"].get<float>();
    return true;
}

bool parseProfileBody(const std::string& body, std::string& outId) {
    json j;
    try {
        j = json::parse(body);
    } catch (const json::parse_error&) {
        return false;
    }
    if (!j.is_object() || !j.contains("id") || !j["id"].is_string()) return false;
    outId = j["id"].get<std::string>();
    return true;
}

}  // namespace web_demo
