#include "profile_settings.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

#include "sofa_validator.h"

namespace fs = std::filesystem;

namespace nathan::hrtf {

namespace {

const HrtfProfile* findById(const std::vector<HrtfProfile>& catalog, const std::string& id) {
    for (const auto& profile : catalog) {
        if (profile.id == id) {
            return &profile;
        }
    }
    return nullptr;
}

// Premier profil "subject_003" si present, sinon premier du catalogue
// (deja trie par id). catalog ne doit pas etre vide.
const HrtfProfile& defaultProfile(const std::vector<HrtfProfile>& catalog) {
    if (const HrtfProfile* preferred = findById(catalog, "subject_003")) {
        return *preferred;
    }
    return catalog.front();
}

}  // namespace

std::optional<AppSettings> loadSettings(const std::filesystem::path& settingsPath) {
    std::ifstream in(settingsPath, std::ios::binary);
    if (!in.is_open()) {
        return std::nullopt;
    }

    nlohmann::json parsed;
    try {
        in >> parsed;
    } catch (const nlohmann::json::parse_error&) {
        return std::nullopt;
    }

    if (!parsed.is_object()) {
        return std::nullopt;
    }
    auto it = parsed.find("hrtf_profile");
    if (it == parsed.end() || !it->is_string()) {
        return std::nullopt;
    }

    AppSettings settings;
    settings.hrtfProfile = it->get<std::string>();
    return settings;
}

void saveSettings(const AppSettings& settings, const std::filesystem::path& settingsPath) {
    std::error_code ec;
    fs::create_directories(settingsPath.parent_path(), ec);
    if (ec) {
        throw std::runtime_error("Impossible de creer le dossier de configuration " +
                                  settingsPath.parent_path().string() + " : " + ec.message());
    }

    nlohmann::json out;
    out["hrtf_profile"] = settings.hrtfProfile;

    const fs::path tmpPath = settingsPath.string() + ".tmp";
    {
        std::ofstream tmp(tmpPath, std::ios::binary | std::ios::trunc);
        if (!tmp.is_open()) {
            throw std::runtime_error("Impossible d'ecrire dans " + tmpPath.string());
        }
        tmp << out.dump(2);
        if (!tmp.good()) {
            throw std::runtime_error("Echec d'ecriture dans " + tmpPath.string());
        }
    }

    fs::rename(tmpPath, settingsPath, ec);
    if (ec) {
        fs::remove(tmpPath, ec);
        throw std::runtime_error("Impossible de renommer " + tmpPath.string() + " vers " +
                                  settingsPath.string() + " : " + ec.message());
    }
}

StartupResolution resolveStartupProfile(const std::vector<HrtfProfile>& catalog,
                                         const std::filesystem::path& settingsPath) {
    if (catalog.empty()) {
        throw std::runtime_error("Aucun profil HRTF disponible dans le catalogue.");
    }

    const std::optional<AppSettings> settings = loadSettings(settingsPath);
    if (!settings) {
        StartupResolution resolution;
        resolution.profile = defaultProfile(catalog);
        resolution.usedFallback = true;
        resolution.fallbackReason =
            "Aucun fichier de configuration valide trouve (" + settingsPath.string() + ").";
        return resolution;
    }

    const HrtfProfile* found = findById(catalog, settings->hrtfProfile);
    if (!found) {
        StartupResolution resolution;
        resolution.profile = defaultProfile(catalog);
        resolution.usedFallback = true;
        resolution.fallbackReason = "Profil '" + settings->hrtfProfile +
                                     "' reference dans la configuration introuvable dans le catalogue.";
        return resolution;
    }

    const SofaValidationResult validation = validateSofaFile(found->path);
    if (!validation.ok) {
        StartupResolution resolution;
        resolution.profile = defaultProfile(catalog);
        resolution.usedFallback = true;
        resolution.fallbackReason =
            "Profil '" + found->id + "' invalide : " + validation.error;
        return resolution;
    }

    StartupResolution resolution;
    resolution.profile = *found;
    resolution.usedFallback = false;
    return resolution;
}

}  // namespace nathan::hrtf
