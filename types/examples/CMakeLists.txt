add_executable(01_basic_refinement 01_basic_refinement.cpp)
target_link_libraries(01_basic_refinement PRIVATE reftype::reftype)
target_compile_options(01_basic_refinement PRIVATE -Wall -Wextra -Werror)

add_executable(02_builtin_type_rules 02_builtin_type_rules.cpp)
target_link_libraries(02_builtin_type_rules PRIVATE reftype::reftype)
target_compile_options(02_builtin_type_rules PRIVATE -Wall -Wextra -Werror)

add_executable(03_custom_type_rules 03_custom_type_rules.cpp)
target_link_libraries(03_custom_type_rules PRIVATE reftype::reftype)
target_compile_options(03_custom_type_rules PRIVATE -Wall -Wextra -Werror)

add_executable(04_forth_dsl 04_forth_dsl.cpp)
target_link_libraries(04_forth_dsl PRIVATE reftype::reftype)
target_compile_options(04_forth_dsl PRIVATE -Wall -Wextra -Werror)
