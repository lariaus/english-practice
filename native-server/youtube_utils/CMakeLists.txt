add_library(youtube_utils STATIC
  lib/youtube_transcript_api.cpp
  lib/transcript.cpp
  lib/html_unescape.cpp
  lib/html_entities_generated.cpp
)

target_include_directories(youtube_utils PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(youtube_utils PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

target_link_libraries(youtube_utils PUBLIC
  https_client
  nlohmann_json::nlohmann_json
)

target_link_libraries(youtube_utils PRIVATE
  pugixml::pugixml
)
