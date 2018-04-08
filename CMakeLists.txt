cmake_minimum_required (VERSION 3.2)
project (8005-asn3)

set (8005-asn3_VERSION_MAJOR 1)
set (8005-asn3_VERSION_MINOR 0)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra -fsanitze=address")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -s -fno-ident  -march=native -flto -DNDEBUG")


add_definitions(-D_POSIX_C_SOURCE=200809L)
add_definitions(-D_DEFAULT_SOURCE)

file(GLOB_RECURSE MAIN
    "src/**.c"
    )

add_executable(8005-asn3 ${MAIN})
target_include_directories(8005-asn3 PUBLIC "src" "src/wrappers")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_link_libraries(8005-asn3 rt Threads::Threads)
