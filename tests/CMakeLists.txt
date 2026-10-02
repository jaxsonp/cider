cmake_minimum_required(VERSION 3.10)
project(cider-compiler-tests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(test_runner
	${CMAKE_CURRENT_SOURCE_DIR}/test_runner.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/testing.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/unit/dummy.cpp
)
target_include_directories(test_runner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})