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

add_executable(test_fm_tester_bugs test_fm_tester_bugs.cpp)
target_link_libraries(test_fm_tester_bugs PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_fm_tester_bugs PRIVATE -Wall -Wextra -Werror)

add_executable(test_types test_types.cpp)
target_link_libraries(test_types PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_types PRIVATE -Wall -Wextra -Werror)

add_executable(test_type_env test_type_env.cpp)
target_link_libraries(test_type_env PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_type_env PRIVATE -Wall -Wextra -Werror)

add_executable(test_constraints test_constraints.cpp)
target_link_libraries(test_constraints PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_constraints PRIVATE -Wall -Wextra -Werror)

add_executable(test_subtype test_subtype.cpp)
target_link_libraries(test_subtype PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_subtype PRIVATE -Wall -Wextra -Werror)

add_executable(test_type_checker test_type_checker.cpp)
target_link_libraries(test_type_checker PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_type_checker PRIVATE -Wall -Wextra -Werror)

add_executable(test_type_rules test_type_rules.cpp)
target_link_libraries(test_type_rules PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_type_rules PRIVATE -Wall -Wextra -Werror)

add_executable(test_strip test_strip.cpp)
target_link_libraries(test_strip PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_strip PRIVATE -Wall -Wextra -Werror)

add_executable(test_tester_findings test_tester_findings.cpp)
target_link_libraries(test_tester_findings PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(test_tester_findings PRIVATE -Wall -Wextra -Werror)

add_executable(reftype_test_integration test_integration.cpp)
target_link_libraries(reftype_test_integration PRIVATE reftype::reftype GTest::gtest_main)
target_compile_options(reftype_test_integration PRIVATE -Wall -Wextra -Werror)

# --- Compile-fail tests ---
# These targets should FAIL to compile (consteval throw = compile error).
# EXCLUDE_FROM_ALL prevents building with the default target.
# ctest builds them on demand and WILL_FAIL means success = compile failure.

function(add_compile_fail_test name source)
    add_executable(${name} EXCLUDE_FROM_ALL ${source})
    target_link_libraries(${name} PRIVATE reftype::reftype)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
    add_test(
        NAME ${name}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                --target ${name}
                --config $<CONFIG>
    )
    set_tests_properties(${name} PROPERTIES
        WILL_FAIL TRUE
        TIMEOUT 120
    )
endfunction()

add_compile_fail_test(compile_fail_mixed_arith compile_fail_mixed_arith.cpp)
add_compile_fail_test(compile_fail_bool_arith compile_fail_bool_arith.cpp)
add_compile_fail_test(compile_fail_standalone_lambda compile_fail_standalone_lambda.cpp)
add_compile_fail_test(compile_fail_apply_non_function compile_fail_apply_non_function.cpp)
add_compile_fail_test(compile_fail_unbound_var compile_fail_unbound_var.cpp)
add_compile_fail_test(compile_fail_bare_type_node compile_fail_bare_type_node.cpp)
add_compile_fail_test(compile_fail_typed_compile_invalid compile_fail_typed_compile_invalid.cpp)
add_compile_fail_test(compile_fail_wider_base_nonbase compile_fail_wider_base_nonbase.cpp)
# Regression test for the former compile-fail case (F1 bug: false UNSAT
# with non-integer coefficients). Now that the rounding bug is fixed, the
# static_assert succeeds and the test compiles and runs cleanly.
add_executable(test_fm_rounding_f1_regression test_fm_rounding_f1_regression.cpp)
target_link_libraries(test_fm_rounding_f1_regression PRIVATE reftype::reftype)
target_compile_options(test_fm_rounding_f1_regression PRIVATE -Wall -Wextra -Werror)
add_test(NAME test_fm_rounding_f1_regression COMMAND test_fm_rounding_f1_regression)

include(GoogleTest)
gtest_discover_tests(test_fm_types PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_fm_core PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_fm_rounding PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_fm_parser PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_fm_solver PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_fm_disjunction PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_fm_tester_bugs PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_types PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_type_env PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_constraints PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_subtype PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_type_checker PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_type_rules PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_strip PROPERTIES TIMEOUT 60)
gtest_discover_tests(test_tester_findings PROPERTIES TIMEOUT 60)
gtest_discover_tests(reftype_test_integration PROPERTIES TIMEOUT 120)
