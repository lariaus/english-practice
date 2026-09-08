add_library(data_packs STATIC
  lib/data_packs.cpp
)

target_include_directories(data_packs PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
