cmake_minimum_required(VERSION 3.10)
project(cider-compiler-tests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# in-process tests live alongside the code they test, as <Name>.tests.cpp
file(GLOB_RECURSE CIDER_SOURCE_TESTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.tests.cpp)

add_executable(test_runner
	${PROJECT_SOURCE_DIR}/test_runner.cpp
	${PROJECT_SOURCE_DIR}/testing.cpp
	${CIDER_SOURCE_TESTS}
)
target_include_directories(test_runner SYSTEM PRIVATE
	${PROJECT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_runner PRIVATE ciderc_lib)
