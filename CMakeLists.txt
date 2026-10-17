cmake_minimum_required(VERSION 3.13)

project(codecrafters-redis)

file(GLOB_RECURSE SOURCE_FILES src/*.c src/*.h)

set(CMAKE_C_STANDARD 23) # Enable the C23 standard

add_executable(redis ${SOURCE_FILES})

include(CTest)

if(BUILD_TESTING)
    add_executable(clrf_tests tests/test_clrf.c src/clrf.c src/utils.c)
    target_include_directories(clrf_tests PRIVATE src)
    add_test(NAME clrf_tests COMMAND clrf_tests)

    add_executable(utils_tests tests/test_utils.c src/utils.c)
    target_include_directories(utils_tests PRIVATE src)
    add_test(NAME utils_tests COMMAND utils_tests)
endif()
