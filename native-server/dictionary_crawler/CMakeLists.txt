add_executable(dictionary-crawler
  main.cpp
)

target_link_libraries(dictionary-crawler PRIVATE
  dictionary_utils
)
