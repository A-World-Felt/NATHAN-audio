cmake_minimum_required(VERSION 3.20)
project(nathan_audio_benchmark)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenAL CONFIG REQUIRED)
find_package(mysofa CONFIG REQUIRED)

add_library(nathan_support STATIC
    src/wav_loader.cpp
    src/hrtf_profile.cpp
    src/al_hrtf_device.cpp
    src/input_keys.cpp
)
target_include_directories(nathan_support PUBLIC src)
target_link_libraries(nathan_support PUBLIC OpenAL::OpenAL mysofa::mysofa-shared)

add_executable(profile_selector profile_selector.cpp)
target_link_libraries(profile_selector PRIVATE nathan_support)

add_executable(spatial_tests spatial_tests.cpp)
target_link_libraries(spatial_tests PRIVATE nathan_support)

add_executable(game_simulation game_simulation.cpp)
target_link_libraries(game_simulation PRIVATE nathan_support)

add_executable(audio_benchmark audio_benchmark.cpp)
target_link_libraries(audio_benchmark PRIVATE nathan_support)