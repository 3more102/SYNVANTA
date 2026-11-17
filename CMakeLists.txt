cmake_minimum_required(VERSION 3.20)
project(SYNVANTA VERSION 0.1.0 LANGUAGES CXX)

option(SYNVANTA_BUILD_TESTS "Build SYNVANTA unit tests" ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(synvanta_core
    src/lexer.cpp
    src/parser.cpp
    src/semantic.cpp
    src/lowering.cpp
    src/rtl_ir.cpp
    src/optimizer.cpp
    src/aig.cpp
    src/compiler.cpp
)
target_include_directories(synvanta_core PUBLIC include)

if(MSVC)
    target_compile_options(synvanta_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(synvanta_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

add_executable(synvanta src/main.cpp)
target_link_libraries(synvanta PRIVATE synvanta_core)

if(SYNVANTA_BUILD_TESTS)
    enable_testing()
    add_executable(synvanta_tests
        tests/test_main.cpp
        tests/test_lexer.cpp
        tests/test_parser.cpp
        tests/test_semantic.cpp
        tests/test_lowering.cpp
        tests/test_optimizer.cpp
        tests/test_aig.cpp
    )
    target_link_libraries(synvanta_tests PRIVATE synvanta_core)
    add_test(NAME synvanta_tests COMMAND synvanta_tests)
endif()
