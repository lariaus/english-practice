add_library(storage_map STATIC
  lib/storage_map.cpp
)

target_include_directories(storage_map PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(storage_map PUBLIC
  nlohmann_json::nlohmann_json
)
