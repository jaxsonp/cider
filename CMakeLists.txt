cmake_minimum_required(VERSION 3.10)
project(cider-compiler CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# enable debugging output
add_compile_definitions(DEBUG_OUTPUT)

# include headers
include_directories("src")

# find all source files
file(GLOB_RECURSE SRCS "src/*.cpp")

# Define target executable name
add_executable(ciderc ${SRCS})