cmake_minimum_required (VERSION 2.6)
project ("JSECoin OpenCL Miner")

find_package(OpenCL 1.2 REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(READ src/sha256.cl SHA256_CL_SOURCE)

configure_file(src/sha256.cl.c.in ${CMAKE_SOURCE_DIR}/src/sha256.cl.c)

add_executable(miner src/main.c
                     src/sha256.c
                     src/sha256.cl.c
                     src/miner.c
                     src/socket.c)

include_directories(include)

if(WIN32)
    target_link_libraries(miner OpenCL
                                Ws2_32)
else()
    target_link_libraries(miner OpenCL)
endif()