cmake_minimum_required(VERSION 3.10)
project(cider-compiler-tests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(test_runner
	${PROJECT_SOURCE_DIR}/test_runner.cpp
	${PROJECT_SOURCE_DIR}/testing.cpp

	${CMAKE_SOURCE_DIR}/src/frontend/tests/lexer_tests.cpp
)
target_include_directories(test_runner SYSTEM PRIVATE
	${PROJECT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/src
)