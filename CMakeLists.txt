cmake_minimum_required(VERSION 3.20)
project(refmacro
    VERSION 0.1.0
    DESCRIPTION "C++26 Compile-Time AST Metaprogramming Framework"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(REFMACRO_BUILD_TESTS "Build test suite" ON)

add_library(refmacro INTERFACE)
add_library(refmacro::refmacro ALIAS refmacro)

target_include_directories(refmacro INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(refmacro INTERFACE -freflection)
endif()

if(REFMACRO_BUILD_TESTS)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(tests)
endif()

option(REFMACRO_BUILD_EXAMPLES "Build examples" ON)

if(REFMACRO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
