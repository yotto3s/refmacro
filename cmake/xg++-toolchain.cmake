# CMake toolchain file for GCC build-tree xg++ with C++26 reflection support.
#
# Usage:
#   cmake -B build --toolchain cmake/xg++-toolchain.cmake \
#         -DGCC_BUILD_DIR=/path/to/gcc/build/gcc
#
# GCC_BUILD_DIR must point to the gcc/ subdirectory of a GCC build tree
# (the directory containing xg++ and cc1plus).

set(GCC_BUILD_DIR "" CACHE PATH "Path to GCC build directory containing xg++")

# Ensure GCC_BUILD_DIR is forwarded to try_compile sub-builds
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES GCC_BUILD_DIR)

if(NOT GCC_BUILD_DIR)
    message(FATAL_ERROR
        "GCC_BUILD_DIR is required when using xg++-toolchain.cmake.\n"
        "Pass -DGCC_BUILD_DIR=/path/to/gcc/build/gcc")
endif()

# --- Locate xg++ ---
find_program(_xg_compiler xg++ PATHS "${GCC_BUILD_DIR}" NO_DEFAULT_PATH)
if(NOT _xg_compiler)
    message(FATAL_ERROR "xg++ not found in GCC_BUILD_DIR: ${GCC_BUILD_DIR}")
endif()
set(CMAKE_CXX_COMPILER "${_xg_compiler}")

# --- -B flag so xg++ can find cc1plus ---
get_filename_component(_xg_dir "${_xg_compiler}" DIRECTORY)
set(CMAKE_CXX_FLAGS_INIT "-B${_xg_dir}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "")

# --- libstdc++ include paths from the build tree ---
# Query the target triple (e.g. x86_64-pc-linux-gnu)
execute_process(
    COMMAND "${_xg_compiler}" "-B${_xg_dir}" -dumpmachine
    OUTPUT_VARIABLE _gcc_target_triple
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Derive build root (parent of gcc/ subdir) and source root (parent of build root)
get_filename_component(_gcc_build_root "${_xg_dir}" DIRECTORY)
get_filename_component(_gcc_source_root "${_gcc_build_root}" DIRECTORY)

set(_libstdcxx_include "${_gcc_build_root}/${_gcc_target_triple}/libstdc++-v3/include")
set(_libstdcxx_platform "${_libstdcxx_include}/${_gcc_target_triple}")
set(_libsupcxx_include "${_gcc_source_root}/libstdc++-v3/libsupc++")

if(EXISTS "${_libstdcxx_include}")
    string(APPEND CMAKE_CXX_FLAGS_INIT " -nostdinc++")
    string(APPEND CMAKE_CXX_FLAGS_INIT " -isystem ${_libstdcxx_platform}")
    string(APPEND CMAKE_CXX_FLAGS_INIT " -isystem ${_libstdcxx_include}")
    if(EXISTS "${_libsupcxx_include}")
        string(APPEND CMAKE_CXX_FLAGS_INIT " -isystem ${_libsupcxx_include}")
    endif()
    message(STATUS "xg++ toolchain: using libstdc++ from ${_libstdcxx_include}")
else()
    message(WARNING
        "libstdc++ headers not found at ${_libstdcxx_include}. "
        "Standard library headers may be missing.")
endif()

# --- libstdc++ link paths ---
set(_libstdcxx_lib "${_gcc_build_root}/${_gcc_target_triple}/libstdc++-v3/src/.libs")
if(EXISTS "${_libstdcxx_lib}")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT " -L${_libstdcxx_lib}")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT " -Wl,-rpath,${_libstdcxx_lib}")
endif()

set(_libsupcxx_lib "${_gcc_build_root}/${_gcc_target_triple}/libstdc++-v3/libsupc++/.libs")
if(EXISTS "${_libsupcxx_lib}")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT " -L${_libsupcxx_lib}")
endif()
