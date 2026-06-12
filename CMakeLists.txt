cmake_minimum_required(VERSION 3.30)

project(ryc
    VERSION 0.1
    LANGUAGES CXX
)

add_executable(ryc main.cpp)

target_compile_features(ryc PRIVATE cxx_std_23)

target_compile_options(ryc PRIVATE
    -Wall -Wextra -Wpedantic
    -stdlib=libc++
)

target_link_options(ryc PRIVATE
    -stdlib=libc++
    -lc++abi
)

target_sources(ryc
    PRIVATE FILE_SET CXX_MODULES FILES
        lexer.cppm
        parser.cppm
        semantic.cppm
        codegen.cppm
)

set(RYC_EXAMPLE "" CACHE STRING "Path to a .ryst source file")

add_custom_target(run
    COMMAND ryc ${RYC_EXAMPLE}
    DEPENDS ryc
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running ryc on ${RYC_EXAMPLE}"
)
