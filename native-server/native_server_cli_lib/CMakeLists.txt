add_library(native_server_cli_lib STATIC
  lib/cli_config.cpp
)

target_include_directories(native_server_cli_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
