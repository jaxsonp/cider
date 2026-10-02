cmake_minimum_required(VERSION 3.10)
project(cider-compiler CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ciderc_lib STATIC
	src/compile.cpp
	src/frontend/AST.cpp
	src/frontend/AST_emitIr.cpp
	src/frontend/AST_semantics.cpp
	src/frontend/AST_print.cpp
	src/frontend/FrontendType.cpp
	src/frontend/Lexer.cpp
	src/frontend/Parser.cpp
	src/ir/IR.cpp
	src/ir/IrPrinter.cpp
	src/ir/IrType.cpp
	src/ir/IrWriter.cpp
	src/backend/Target.cpp
	src/backend/codegen/CodeGenerator.cpp
	src/backend/codegen/riscv/CodeGenerator_riscv32.cpp
	src/backend/objwriter/elf/ObjectWriter_ELF32.cpp
	src/utils/CliParser.cpp
	src/utils/common.cpp
	src/utils/error.cpp
	src/utils/logging.cpp
)
# include headers in src
target_include_directories(ciderc_lib SYSTEM PUBLIC src/)

# Main compiler executable
add_executable(ciderc src/main.cpp)
target_link_libraries(ciderc PRIVATE ciderc_lib)

# Testing
add_subdirectory(tests)