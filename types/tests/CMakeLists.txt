add_executable(test_fm_types test_fm_types.cpp)
target_link_libraries(test_fm_types PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_fm_types PRIVATE -Wall -Wextra -Werror)

add_executable(test_fm_core test_fm_core.cpp)
target_link_libraries(test_fm_core PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_fm_core PRIVATE -Wall -Wextra -Werror)

include(GoogleTest)
gtest_discover_tests(test_fm_types PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_fm_core PROPERTIES TIMEOUT 60)
