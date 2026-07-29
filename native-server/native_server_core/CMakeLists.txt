add_library(native_server_core STATIC
  lib/server.cpp
)

target_include_directories(native_server_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(native_server_core PUBLIC
  httplib::httplib
  nlohmann_json::nlohmann_json
)
