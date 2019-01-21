cmake_minimum_required (VERSION 2.6)
project ("JSECoin OpenCL Hasher")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(hasher src/main.c)
target_link_libraries(hasher OpenCL)
include_directories(include)
configure_file(src/sha256.cl ${CMAKE_BINARY_DIR}/bin/sha256.cl)
