cmake_minimum_required(VERSION 3.10)
project(cider-compiler-tests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(test_runner
	${PROJECT_SOURCE_DIR}/test_runner.cpp
	${PROJECT_SOURCE_DIR}/testing.cpp

	# tests
	${CMAKE_SOURCE_DIR}/src/frontend/Lexer.tests.cpp
)
target_include_directories(test_runner SYSTEM PRIVATE
	${PROJECT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_runner PRIVATE ciderc_lib)

# required for parsing in-source unit tests
target_compile_definitions(test_runner
    PRIVATE CIDER_SRC_DIR="${CMAKE_SOURCE_DIR}/src/"
)
