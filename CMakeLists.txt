cmake_minimum_required(VERSION 3.20)
project(nathan_audio)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenAL CONFIG REQUIRED)
find_package(mysofa CONFIG REQUIRED)

# Code produit (NATHAN) : voir src/hrtf/ et tools/.
add_subdirectory(src)
add_subdirectory(tools)

# Prototypes de la phase d'exploration, conserves comme reference
# (voir benchmarks/README.md).
add_subdirectory(benchmarks)
