cmake_minimum_required(VERSION 3.10)
project(cider-compiler-tests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Embed the integration test files' content into the runner. Re-globbed every build
set(E2E_DIR ${PROJECT_SOURCE_DIR}/e2e_tests)
set(E2E_EMBED_SCRIPT ${PROJECT_SOURCE_DIR}/embed_e2e_tests.cmake)
set(E2E_EMBED_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/e2e_test_files.cpp)
file(GLOB_RECURSE E2E_TEST_FILES CONFIGURE_DEPENDS ${E2E_DIR}/*.cdr)
add_custom_command(
	OUTPUT ${E2E_EMBED_OUTPUT}
	COMMAND ${CMAKE_COMMAND} -DE2E_DIR=${E2E_DIR} -DOUTPUT=${E2E_EMBED_OUTPUT} -P ${E2E_EMBED_SCRIPT}
	DEPENDS ${E2E_EMBED_SCRIPT} ${E2E_TEST_FILES}
	COMMENT "Embedding e2e test files"
	VERBATIM
)

add_executable(test_runner
	${PROJECT_SOURCE_DIR}/src/testing.cpp
	${PROJECT_SOURCE_DIR}/src/test_runner.cpp
	${PROJECT_SOURCE_DIR}/src/unit_testing.cpp
	${PROJECT_SOURCE_DIR}/src/e2e_testing.cpp
	${E2E_EMBED_OUTPUT}

	# tests
	${CMAKE_SOURCE_DIR}/src/frontend/Lexer.tests.cpp
)
target_include_directories(test_runner SYSTEM PRIVATE
	${PROJECT_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_runner PRIVATE ciderc_lib)

# required for parsing in-source unit tests
target_compile_definitions(test_runner
    PRIVATE CIDER_SRC_DIR="${CMAKE_SOURCE_DIR}/src/"
)
