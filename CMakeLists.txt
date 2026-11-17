cmake_minimum_required(VERSION 3.15)
project(CodeSearch CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Предупреждения (кросс-платформенно)
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

find_package(Threads REQUIRED)

# --- Основная утилита ---
add_executable(codesearch
    main.cpp
    RegexBuilder.cpp
    FileSearcher.cpp
    Prompt.cpp
)
target_include_directories(codesearch PRIVATE .)
target_link_libraries(codesearch PRIVATE Threads::Threads)

# --- Тесты ---
enable_testing()

add_executable(tests
    tests.cpp
    RegexBuilder.cpp
    FileSearcher.cpp
)
target_include_directories(tests PRIVATE .)
target_link_libraries(tests PRIVATE Threads::Threads)

add_test(NAME tests COMMAND tests)