add_executable(native_server_unit_tests
  unit/test_config.cpp
)

target_link_libraries(native_server_unit_tests PRIVATE
  native_server_cli_lib
  Catch2::Catch2WithMain
)

add_test(NAME native_server_unit_tests COMMAND native_server_unit_tests)

add_executable(native_server_integration_tests
  integration/test_static_serving.cpp
)

target_include_directories(native_server_integration_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(native_server_integration_tests PRIVATE
  native_server_core
  Catch2::Catch2WithMain
)

target_compile_definitions(native_server_integration_tests PRIVATE
  NATIVE_SERVER_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

add_test(NAME native_server_integration_tests COMMAND native_server_integration_tests)

add_executable(native_server_concurrency_tests
  concurrency/test_concurrency.cpp
)

target_include_directories(native_server_concurrency_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(native_server_concurrency_tests PRIVATE
  native_server_core
  Catch2::Catch2WithMain
)

target_compile_definitions(native_server_concurrency_tests PRIVATE
  NATIVE_SERVER_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

add_test(NAME native_server_concurrency_tests COMMAND native_server_concurrency_tests)
