cmake_minimum_required(VERSION 3.10)

# set the project name
project(rainbowcrack VERSION 0.1)

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
set(STD_COMPILE_FLAGS "-Wall -Werror -g -O1")
set(STD_LINKER_FLAGS "-lpthread -lgmpxx -lgmp")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${STD_COMPILE_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${STD_COMPILE_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${STD_LINKER_FLAGS}")

set(CMAKE_CXX_FLAGS_DEBUG "-g -ggdb -O0 -fno-omit-frame-pointer -DDEBUG")

if(APPLE)
    set(HOMEBREW_PREFIX "/usr/local" CACHE PATH "Path to Homebrew installation")
    set(HOMEBREW_INCLUDE ${HOMEBREW_PREFIX}/include)
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
else()
    set(HOMEBREW_INCLUDE "")
endif()

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_executable(rainbowcrack ${SOURCES})
target_include_directories(rainbowcrack
                            PUBLIC
                                ./src/
                                ./SimdHash/SimdHash/
                                ./libdispatchqueue/include/
                        )
target_link_libraries(rainbowcrack simdhash dispatchqueue)

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
                                    ./SimdHash/SimdHash/
                                    ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_options(${TESTNAME} PRIVATE "LINKER:-lcrypto")
    target_link_libraries(${TESTNAME} simdhash)
    add_dependencies(tests ${TESTNAME})
endforeach()