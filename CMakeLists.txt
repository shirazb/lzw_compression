cmake_minimum_required(VERSION 3.9)
project(lzw_compression)

# Setup output dir.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Setup compiler.
# TODO: Configure for debug and production?.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-Wall -g -pedantic")

# Include src dir.
include_directories(src)

# Setup src files, header files, executable file, and add executable.

set(LZW_SOURCE_FILES
        src/lzw_decompressor.c)

set(LZW_HEADER_FILES
        src/lzw_decompressor.h)

set(LZW_EXECUTABLE src/main.c)


add_executable(lzw_decompressor ${LZW_EXECUTABLE} ${LZW_SOURCE_FILES}
        ${LZW_HEADER_FILES})
