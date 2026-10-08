cmake_minimum_required(VERSION 3.15)
project(CodeSearch CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(codesearch
    main.cpp
    RegexBuilder.cpp
    FileSearcher.cpp
    Prompt.cpp
)

target_include_directories(codesearch PRIVATE .)

# Линковка потоков
find_package(Threads REQUIRED)
target_link_libraries(codesearch PRIVATE Threads::Threads)