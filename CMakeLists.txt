cmake_minimum_required(VERSION 3.20)
project(nathan_audio)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenAL CONFIG REQUIRED)
find_package(mysofa CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Code produit (NATHAN) : voir src/hrtf/ et tools/.
add_subdirectory(src)
add_subdirectory(tools)

# Prototypes de la phase d'exploration, conserves comme reference
# (voir benchmarks/README.md).
add_subdirectory(benchmarks)

# Demo web (vitrine) : reutilise src/hrtf + benchmarks/src via une API HTTP
# locale. Ni code de production ni prototype d'exploration — voir
# docs/superpowers/specs/2026-08-06-web-audio-demo-design.md.
add_subdirectory(examples/web-demo/server)
