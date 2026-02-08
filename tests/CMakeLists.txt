add_executable(test_ast test_ast.cpp)
target_link_libraries(test_ast PRIVATE refmacro::refmacro GTest::gtest_main)
target_compile_options(test_ast PRIVATE -Wall -Wextra -Werror)

include(GoogleTest)
gtest_discover_tests(test_ast PROPERTIES TIMEOUT 60)

add_executable(test_expr test_expr.cpp)
target_link_libraries(test_expr PRIVATE refmacro::refmacro GTest::gtest_main)
target_compile_options(test_expr PRIVATE -Wall -Wextra -Werror)
gtest_discover_tests(test_expr PROPERTIES TIMEOUT 60)

add_executable(test_compile test_compile.cpp)
target_link_libraries(test_compile PRIVATE refmacro::refmacro GTest::gtest_main)
target_compile_options(test_compile PRIVATE -Wall -Wextra -Werror)
gtest_discover_tests(test_compile PROPERTIES TIMEOUT 60)

add_executable(test_math test_math.cpp)
target_link_libraries(test_math PRIVATE refmacro::refmacro GTest::gtest_main)
target_compile_options(test_math PRIVATE -Wall -Wextra -Werror)
gtest_discover_tests(test_math PROPERTIES TIMEOUT 60)
