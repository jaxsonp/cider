cmake_minimum_required(VERSION 3.10)
project(
	cider-compiler
	VERSION 0.0.0
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# enable debugging output
add_compile_definitions(
	CIDER_VERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
	CIDER_VERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
	CIDER_VERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
	CIDER_VERSION="${CMAKE_PROJECT_VERSION}"
)

# include headers
include_directories("src")

# find all source files
file(GLOB_RECURSE SRCS "src/*.cpp")

# Define target executable name
add_executable(ciderc ${SRCS})