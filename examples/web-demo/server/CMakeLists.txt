# Demo web (vitrine) : ni code de production (src/, tools/) ni prototype
# d'exploration (benchmarks/) — voir docs/superpowers/specs/2026-08-06-web-audio-demo-design.md.

add_executable(web_demo_server
    main.cpp
    spatial_math.cpp
    frame_stats.cpp
    state_json.cpp
)
target_include_directories(web_demo_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(web_demo_server PRIVATE nathan_hrtf nathan_support nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(web_demo_server PRIVATE ws2_32)
endif()

add_executable(spatial_math_test spatial_math_test.cpp spatial_math.cpp)
target_include_directories(spatial_math_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(frame_stats_test frame_stats_test.cpp frame_stats.cpp)
target_include_directories(frame_stats_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(state_json_test state_json_test.cpp state_json.cpp frame_stats.cpp)
target_include_directories(state_json_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(state_json_test PRIVATE nlohmann_json::nlohmann_json)
