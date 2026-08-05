# Convention: one binary (add_executable + add_test) per test file - never
# group multiple test .cpp files into one executable. Keeps ctest's output
# granular (one line per file) and each binary linking only what its one
# file actually needs.

# Only native_server_test_youtube_utils_parity needs Python (to regenerate
# its reference data via the real youtube_transcript_api) - non-fatal if
# missing, since nothing else in the build depends on it; that one test
# would just fail if actually run without Python available.
find_package(Python3 COMPONENTS Interpreter)
if(NOT Python3_FOUND)
  message(WARNING "Python3 not found - native_server_test_youtube_utils_parity will fail if run")
endif()

add_executable(native_server_test_storage_map
  unit/test_storage_map.cpp
)
target_link_libraries(native_server_test_storage_map PRIVATE
  storage_map
  Catch2::Catch2WithMain
)
add_test(NAME native_server_test_storage_map COMMAND native_server_test_storage_map)

add_executable(native_server_test_config
  unit/test_config.cpp
)
target_link_libraries(native_server_test_config PRIVATE
  native_server_cli_lib
  Catch2::Catch2WithMain
)
add_test(NAME native_server_test_config COMMAND native_server_test_config)

add_executable(native_server_test_http_client
  unit/test_http_client.cpp
)
target_link_libraries(native_server_test_http_client PRIVATE
  https_client
  nlohmann_json::nlohmann_json
  Catch2::Catch2WithMain
)
add_test(NAME native_server_test_http_client COMMAND native_server_test_http_client)

add_executable(native_server_test_https_client
  unit/test_https_client.cpp
)
target_link_libraries(native_server_test_https_client PRIVATE
  https_client
  nlohmann_json::nlohmann_json
  Catch2::Catch2WithMain
)
add_test(NAME native_server_test_https_client COMMAND native_server_test_https_client)

add_executable(native_server_test_static_serving
  integration/test_static_serving.cpp
)
target_include_directories(native_server_test_static_serving PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(native_server_test_static_serving PRIVATE
  native_server_core
  Catch2::Catch2WithMain
)
target_compile_definitions(native_server_test_static_serving PRIVATE
  NATIVE_SERVER_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
add_test(NAME native_server_test_static_serving COMMAND native_server_test_static_serving)

add_executable(native_server_test_health_route
  integration/test_health_route.cpp
)
target_include_directories(native_server_test_health_route PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(native_server_test_health_route PRIVATE
  native_server_core
  nlohmann_json::nlohmann_json
  Catch2::Catch2WithMain
)
target_compile_definitions(native_server_test_health_route PRIVATE
  NATIVE_SERVER_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
add_test(NAME native_server_test_health_route COMMAND native_server_test_health_route)

# Genuinely hits real YouTube through the full route, end to end - not
# mocked. See docs/migration-to-offline-app/step-4.md.
add_executable(native_server_test_subtitles_route
  integration/test_subtitles_route.cpp
)
target_include_directories(native_server_test_subtitles_route PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(native_server_test_subtitles_route PRIVATE
  native_server_core
  nlohmann_json::nlohmann_json
  Catch2::Catch2WithMain
)
target_compile_definitions(native_server_test_subtitles_route PRIVATE
  NATIVE_SERVER_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
add_test(NAME native_server_test_subtitles_route COMMAND native_server_test_subtitles_route)

add_executable(native_server_test_storage_route
  integration/test_storage_route.cpp
)
target_include_directories(native_server_test_storage_route PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(native_server_test_storage_route PRIVATE
  native_server_core
  nlohmann_json::nlohmann_json
  Catch2::Catch2WithMain
)
target_compile_definitions(native_server_test_storage_route PRIVATE
  NATIVE_SERVER_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
add_test(NAME native_server_test_storage_route COMMAND native_server_test_storage_route)

add_executable(native_server_test_concurrency
  concurrency/test_concurrency.cpp
)
target_include_directories(native_server_test_concurrency PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(native_server_test_concurrency PRIVATE
  native_server_core
  Catch2::Catch2WithMain
)
target_compile_definitions(native_server_test_concurrency PRIVATE
  NATIVE_SERVER_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
add_test(NAME native_server_test_concurrency COMMAND native_server_test_concurrency)

# youtube_utils tests below link the library's internal parsing_detail.h
# (under youtube_utils/lib, not the public include/ dir) so the pure-logic
# tests can exercise individual parsing steps without live network access.

add_executable(native_server_test_youtube_utils_api_key
  unit/test_youtube_utils_api_key.cpp
)
target_include_directories(native_server_test_youtube_utils_api_key PRIVATE
  ${CMAKE_SOURCE_DIR}/youtube_utils/lib
)
target_link_libraries(native_server_test_youtube_utils_api_key PRIVATE
  youtube_utils
  Catch2::Catch2WithMain
)
add_test(NAME native_server_test_youtube_utils_api_key COMMAND native_server_test_youtube_utils_api_key)

add_executable(native_server_test_youtube_utils_playability
  unit/test_youtube_utils_playability.cpp
)
target_include_directories(native_server_test_youtube_utils_playability PRIVATE
  ${CMAKE_SOURCE_DIR}/youtube_utils/lib
)
target_link_libraries(native_server_test_youtube_utils_playability PRIVATE
  youtube_utils
  Catch2::Catch2WithMain
)
add_test(NAME native_server_test_youtube_utils_playability COMMAND native_server_test_youtube_utils_playability)

add_executable(native_server_test_youtube_utils_captions_json
  unit/test_youtube_utils_captions_json.cpp
)
target_include_directories(native_server_test_youtube_utils_captions_json PRIVATE
  ${CMAKE_SOURCE_DIR}/youtube_utils/lib
)
target_link_libraries(native_server_test_youtube_utils_captions_json PRIVATE
  youtube_utils
  Catch2::Catch2WithMain
)
add_test(NAME native_server_test_youtube_utils_captions_json COMMAND native_server_test_youtube_utils_captions_json)

add_executable(native_server_test_youtube_utils_transcript_list
  unit/test_youtube_utils_transcript_list.cpp
)
target_include_directories(native_server_test_youtube_utils_transcript_list PRIVATE
  ${CMAKE_SOURCE_DIR}/youtube_utils/lib
)
target_link_libraries(native_server_test_youtube_utils_transcript_list PRIVATE
  youtube_utils
  Catch2::Catch2WithMain
)
add_test(NAME native_server_test_youtube_utils_transcript_list COMMAND native_server_test_youtube_utils_transcript_list)

add_executable(native_server_test_youtube_utils_transcript_xml
  unit/test_youtube_utils_transcript_xml.cpp
)
target_include_directories(native_server_test_youtube_utils_transcript_xml PRIVATE
  ${CMAKE_SOURCE_DIR}/youtube_utils/lib
)
target_link_libraries(native_server_test_youtube_utils_transcript_xml PRIVATE
  youtube_utils
  Catch2::Catch2WithMain
)
add_test(NAME native_server_test_youtube_utils_transcript_xml COMMAND native_server_test_youtube_utils_transcript_xml)

add_executable(native_server_test_youtube_utils_parity
  unit/test_youtube_utils_parity.cpp
)
target_link_libraries(native_server_test_youtube_utils_parity PRIVATE
  youtube_utils
  nlohmann_json::nlohmann_json
  Catch2::Catch2WithMain
)
target_compile_definitions(native_server_test_youtube_utils_parity PRIVATE
  # Generated on demand into the build dir - never committed (build/ is
  # already gitignored) - see generate_youtube_parity_fixture.py and
  # REGENERATE_PYTHON_REF_DATA in docs/migration-to-offline-app/step-4.md.
  YOUTUBE_UTILS_PARITY_FIXTURES_DIR="${CMAKE_BINARY_DIR}/youtube_parity_cache"
  YOUTUBE_UTILS_GENERATE_FIXTURE_SCRIPT="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/generate_youtube_parity_fixture.py"
)
add_test(NAME native_server_test_youtube_utils_parity COMMAND native_server_test_youtube_utils_parity)
