cmake_minimum_required(VERSION 3.9)
project(lzw_compression)

# Set up executable and library locations.

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
        ${CMAKE_HOME_DIRECTORY}/bin)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
        ${CMAKE_HOME_DIRECTORY}/lib)

# Setup compiler.
set(CMAKE_C_STANDARD 11)
# TODO: Configure for debug and production?.
set(CMAKE_C_FLAGS "-Wall -g -pedantic")

# Include src dir.
include_directories(src)

# Setup src files, header files, executable file, and add executable.

set(LZW_SOURCE_FILES
        src/lzw_decompressor.c src/lzw_dict.c)

set(LZW_HEADER_FILES
        src/lzw_decompressor.h src/lzw_dict.h)

set(LZW_EXECUTABLE src/main.c)


add_executable(lzw_decompressor ${LZW_EXECUTABLE} ${LZW_SOURCE_FILES}
        ${LZW_HEADER_FILES})
