add_executable(01_basic_refinement 01_basic_refinement.cpp)
target_link_libraries(01_basic_refinement PRIVATE reftype::reftype)
target_compile_options(01_basic_refinement PRIVATE -Wall -Wextra -Werror)
