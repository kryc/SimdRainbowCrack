cmake_minimum_required(VERSION 3.10)

# set the project name
project(simdrainbowcrack VERSION 0.1)

# Set build type default Release
if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

add_subdirectory(./SimdHash/)
add_subdirectory(./libdispatchqueue/)

# specify clang
SET(CMAKE_C_COMPILER clang)
SET(CMAKE_CXX_COMPILER clang++)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# set compiler flags
set(CMAKE_CXX_FLAGS "-Wall -Werror -Wno-deprecated-declarations -Wno-ignored-optimization-argument -Wno-deprecated-declarations -Wno-vla-extension")
set(CMAKE_EXE_LINKER_FLAGS "-lpthread -lgmpxx -lgmp -lcrypto")

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -funroll-loops")
set(CMAKE_CXX_FLAGS_DEBUG "-g -ggdb -O0 -fno-omit-frame-pointer -fsanitize=address -DDEBUG")

# Handle MacOS Homebrew
if(APPLE)
    execute_process(COMMAND brew --prefix OUTPUT_VARIABLE HOMEBREW_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
    message("Homebrew prefix: ${HOMEBREW_PREFIX}")
    include_directories(${HOMEBREW_PREFIX}/include)
    link_directories(${HOMEBREW_PREFIX}/lib)
endif()

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_executable(simdrainbowcrack ${SOURCES})
set_property(TARGET simdrainbowcrack PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
target_include_directories(simdrainbowcrack
                            PUBLIC
                                ./src/
                                ./SimdHash/src/
                                ./libdispatchqueue/include/
                        )
target_link_libraries(simdrainbowcrack simdhash dispatchqueue)

# add the test
add_custom_target(tests)
file(GLOB TESTS "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")
# We will compile and link all files except main so we can use them
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*main\.cpp$")
foreach(TEST ${TESTS})
    get_filename_component(TESTNAME ${TEST} NAME_WE)
    add_executable(${TESTNAME} EXCLUDE_FROM_ALL ${TEST} ${TEST_SOURCES})
    target_compile_definitions(${TESTNAME} PUBLIC TEST)
    target_include_directories(${TESTNAME}
                                PUBLIC
                                    ./SimdHash/src/
                                    ./libdispatchqueue/include/
                                    ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(${TESTNAME} simdhash dispatchqueue)
    add_dependencies(tests ${TESTNAME})
endforeach()