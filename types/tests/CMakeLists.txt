add_executable(test_fm_types test_fm_types.cpp)
target_link_libraries(test_fm_types PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_fm_types PRIVATE -Wall -Wextra -Werror)

add_executable(test_fm_core test_fm_core.cpp)
target_link_libraries(test_fm_core PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_fm_core PRIVATE -Wall -Wextra -Werror)

add_executable(test_fm_rounding test_fm_rounding.cpp)
target_link_libraries(test_fm_rounding PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_fm_rounding PRIVATE -Wall -Wextra -Werror)

add_executable(test_fm_parser test_fm_parser.cpp)
target_link_libraries(test_fm_parser PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_fm_parser PRIVATE -Wall -Wextra -Werror)

add_executable(test_fm_solver test_fm_solver.cpp)
target_link_libraries(test_fm_solver PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_fm_solver PRIVATE -Wall -Wextra -Werror)

add_executable(test_fm_disjunction test_fm_disjunction.cpp)
target_link_libraries(test_fm_disjunction PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_fm_disjunction PRIVATE -Wall -Wextra -Werror)

add_executable(test_types test_types.cpp)
target_link_libraries(test_types PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_types PRIVATE -Wall -Wextra -Werror)

add_executable(test_type_env test_type_env.cpp)
target_link_libraries(test_type_env PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_type_env PRIVATE -Wall -Wextra -Werror)

add_executable(test_constraints test_constraints.cpp)
target_link_libraries(test_constraints PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_constraints PRIVATE -Wall -Wextra -Werror)

include(GoogleTest)
gtest_discover_tests(test_fm_types PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_fm_core PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_fm_rounding PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_fm_parser PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_fm_solver PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_fm_disjunction PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_types PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_type_env PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_constraints PROPERTIES TIMEOUT 60)
