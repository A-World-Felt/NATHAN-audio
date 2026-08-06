# Code de production NATHAN.

add_library(nathan_hrtf STATIC
    hrtf/profile_catalog.cpp
    hrtf/sofa_validator.cpp
    hrtf/openal_paths.cpp
    hrtf/profile_installer.cpp
    hrtf/hrtf_context.cpp
    hrtf/config_paths.cpp
    hrtf/profile_settings.cpp
    hrtf/profile_selector.cpp
    hrtf/wav_mono16.cpp
    hrtf/mp3_mono16.cpp
    hrtf/audition_input.cpp
    hrtf/active_profile.cpp
    hrtf/profile_audition.cpp
)
target_include_directories(nathan_hrtf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nathan_hrtf PUBLIC OpenAL::OpenAL mysofa::mysofa-shared nlohmann_json::nlohmann_json)
