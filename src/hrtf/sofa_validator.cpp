#include "sofa_validator.h"

#include <mysofa.h>

#include <system_error>

namespace fs = std::filesystem;

namespace nathan::hrtf {

SofaValidationResult validateSofaFile(const fs::path& sofaPath) {
    std::error_code ec;
    if (!fs::is_regular_file(sofaPath, ec)) {
        return {false, "Fichier .sofa introuvable : " + sofaPath.string()};
    }

    int err = MYSOFA_OK;
    MYSOFA_HRTF* hrtf = mysofa_load(sofaPath.string().c_str(), &err);
    if (!hrtf || err != MYSOFA_OK) {
        if (hrtf) mysofa_free(hrtf);
        return {false, "Fichier .sofa illisible (mysofa_load, code " + std::to_string(err) +
                            ") : " + sofaPath.string()};
    }

    int checkErr = mysofa_check(hrtf);
    mysofa_free(hrtf);
    if (checkErr != MYSOFA_OK) {
        return {false, "Fichier .sofa non conforme (mysofa_check, code " +
                            std::to_string(checkErr) + ") : " + sofaPath.string()};
    }

    return {true, ""};
}

}  // namespace nathan::hrtf
