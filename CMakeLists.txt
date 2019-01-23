cmake_minimum_required (VERSION 2.6)
project ("JSECoin OpenCL Miner")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(READ src/sha256.cl SHA256_CL_SOURCE)

configure_file(src/sha256.cl.c.in ${CMAKE_SOURCE_DIR}/src/sha256.cl.c)

add_executable(miner src/main.c
                     src/sha256.c
                     src/sha256.cl.c)

include_directories(include)
target_link_libraries(miner OpenCL)
