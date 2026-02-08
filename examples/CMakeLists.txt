# Helper function to add example executables
function(add_example name)
    add_executable(example_${name} ${name}.cpp)
    target_link_libraries(example_${name} PRIVATE refmacro::refmacro)
    target_compile_options(example_${name} PRIVATE -Wall -Wextra -Werror)
endfunction()

add_example(01_hello_expr)
add_example(02_custom_macro)
add_example(03_math_pipeline)
add_example(04_transforms)
add_example(05_custom_dsl)
