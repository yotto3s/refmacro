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

add_executable(test_transforms test_transforms.cpp)
target_link_libraries(test_transforms PRIVATE refmacro::refmacro GTest::gtest_main)
target_compile_options(test_transforms PRIVATE -Wall -Wextra -Werror)
gtest_discover_tests(test_transforms PROPERTIES TIMEOUT 60)

add_executable(test_control test_control.cpp)
target_link_libraries(test_control PRIVATE refmacro::refmacro GTest::gtest_main)
target_compile_options(test_control PRIVATE -Wall -Wextra -Werror)
gtest_discover_tests(test_control PROPERTIES TIMEOUT 60)

add_executable(test_integration test_integration.cpp)
target_link_libraries(test_integration PRIVATE refmacro::refmacro GTest::gtest_main)
target_compile_options(test_integration PRIVATE -Wall -Wextra -Werror)
gtest_discover_tests(test_integration PROPERTIES TIMEOUT 120)

add_executable(test_new_macro test_new_macro.cpp)
target_link_libraries(test_new_macro PRIVATE refmacro::refmacro GTest::gtest_main)
target_compile_options(test_new_macro PRIVATE -Wall -Wextra -Werror)
gtest_discover_tests(test_new_macro PROPERTIES TIMEOUT 60)
