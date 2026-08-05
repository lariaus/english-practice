find_library(FOUNDATION_LIBRARY Foundation REQUIRED)

add_library(https_client STATIC
  lib/http_client.mm
)

target_include_directories(https_client PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(https_client PRIVATE -fobjc-arc)

target_link_libraries(https_client PRIVATE
  ${FOUNDATION_LIBRARY}
)
