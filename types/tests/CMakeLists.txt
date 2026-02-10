include(GoogleTest)

add_executable(test_fm_types test_fm_types.cpp)
target_link_libraries(test_fm_types PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_fm_types PRIVATE -Wall -Wextra -Werror)
gtest_discover_tests(test_fm_types PROPERTIES TIMEOUT 60)
