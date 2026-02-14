add_library(reftype INTERFACE)
add_library(reftype::reftype ALIAS reftype)

target_include_directories(reftype INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(reftype INTERFACE refmacro::refmacro)

if(REFMACRO_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(REFMACRO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
