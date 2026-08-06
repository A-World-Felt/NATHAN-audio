#include "state_json.h"

#include <cassert>
#include <cstdio>

#include <nlohmann/json.hpp>

int main() {
    using namespace web_demo;
    using json = nlohmann::json;

    DemoState state;
    state.playerX = 1.5f;
    state.playerZ = -0.5f;
    state.profile = {"subject_003", "subject_003", false, ""};
    state.hrtf = {"Enabled", "OpenAL Soft", "1.1 ALSOFT 1.24.3", "OpenAL Community", 44100};
    state.sources.push_back({"avant", "Avant", "assets/test-audio.mp3", "mp3",
                              0.0f, -5.0f, 4.6f, -12.3f, 0.83f});
    state.frameStats = {120, 12.4, 14.1, 8.2, 61.5, 22.0};
    state.frameBudgetMs = 15.0;
    state.fpsReal = 66.2;
    state.framesProcessed = 48213;
    state.mp3 = {"assets/test-audio.mp3", 179837, 44100, 1, 4.08, 179928};

    json j = json::parse(buildStateJson(state));
    assert(j["player"]["x"].get<float>() == 1.5f);
    assert(j["player"]["z"].get<float>() == -0.5f);
    assert(j["profile"]["active"] == "subject_003");
    assert(j["profile"]["usedFallback"] == false);
    assert(j["hrtf"]["sampleRateHz"] == 44100);
    assert(j["sources"].size() == 1);
    assert(j["sources"][0]["id"] == "avant");
    assert(j["sources"][0]["format"] == "mp3");
    assert(j["metrics"]["framesProcessed"] == 48213);
    assert(j["metrics"]["sourcesSimultaneous"] == 1);
    assert(j["metrics"]["meetsRAud03"] == false);  // 1 source < 4
    assert(j["mp3"]["fileSizeBytes"] == 179837);
    assert(j["mp3"]["channels"] == 1);

    // 4 sources -> meetsRAud03 doit passer a true.
    state.sources.push_back({"droite", "Droite", "assets/test-audio2.wav", "wav", 5, 0, 5, 90, 0.2f});
    state.sources.push_back({"arriere", "Arriere", "assets/test-audio3.wav", "wav", 0, 5, 5, 180, 0.2f});
    state.sources.push_back({"gauche", "Gauche", "assets/test-audio4.wav", "wav", -5, 0, 5, -90, 0.2f});
    json j4 = json::parse(buildStateJson(state));
    assert(j4["metrics"]["sourcesSimultaneous"] == 4);
    assert(j4["metrics"]["meetsRAud03"] == true);

    json profilesJson = json::parse(buildProfilesJson({"subject_003", "subject_008"}, "subject_003"));
    assert(profilesJson["profiles"].size() == 2);
    assert(profilesJson["active"] == "subject_003");

    PlayerRequest pr;
    assert(parsePlayerBody(R"({"x":1.2,"z":-0.4})", pr));
    assert(pr.x == 1.2f && pr.z == -0.4f);
    assert(!parsePlayerBody(R"({"x":1.2})", pr));       // z manquant
    assert(!parsePlayerBody("not json", pr));            // JSON invalide
    assert(!parsePlayerBody(R"({"x":"a","z":1})", pr));  // x pas numerique

    std::string id;
    assert(parseProfileBody(R"({"id":"subject_040"})", id));
    assert(id == "subject_040");
    assert(!parseProfileBody(R"({})", id));               // id manquant
    assert(!parseProfileBody(R"({"id":42})", id));         // id pas une chaine

    std::printf("state_json_test : OK\n");
    return 0;
}
