add_executable(test_ast test_ast.cpp)
target_link_libraries(test_ast PRIVATE refmacro::refmacro GTest::gtest_main)
target_compile_options(test_ast PRIVATE -Wall -Wextra -Werror)

include(GoogleTest)
gtest_discover_tests(test_ast PROPERTIES TIMEOUT 60)
