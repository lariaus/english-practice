add_executable(native_server_cli
  main.cpp
)

target_link_libraries(native_server_cli PRIVATE
  native_server_core
  native_server_cli_lib
)
