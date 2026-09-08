add_library(dictionary_utils STATIC
  lib/free_dictionary_api_entry.cpp
  lib/wiktionary_api_entry.cpp
  lib/dictionary_entry.cpp
)

target_include_directories(dictionary_utils PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(dictionary_utils PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

target_link_libraries(dictionary_utils PUBLIC
  https_client
  nlohmann_json::nlohmann_json
  server_data
)
