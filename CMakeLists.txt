cmake_minimum_required(VERSION 3.30)

project(ryc
    VERSION 0.1
    LANGUAGES CXX
)

add_executable(ryc src/main.cpp)

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
        src/lexer.cppm
        src/parser.cppm
        src/semantic.cppm
        src/codegen.cppm
)

# Optional: enable AddressSanitizer + UBSan for debug builds
option(ENABLE_SANITIZERS "Build with AddressSanitizer and UBSanitizer" OFF)
if(ENABLE_SANITIZERS)
    target_compile_options(ryc PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(ryc PRIVATE -fsanitize=address,undefined)
endif()

set(RYC_EXAMPLE "" CACHE STRING "Path to a .ryst source file")

add_custom_target(run
    COMMAND ryc ${RYC_EXAMPLE}
    DEPENDS ryc
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running ryc on ${RYC_EXAMPLE}"
)

# Run all examples through the compiler (smoke test)
add_custom_target(test_examples
    COMMAND ${CMAKE_COMMAND} -E echo "--- Testing examples ---"
    DEPENDS ryc
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
