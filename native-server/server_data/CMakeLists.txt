add_library(server_data STATIC
  lib/server_data_store.cpp
)

target_include_directories(server_data PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
