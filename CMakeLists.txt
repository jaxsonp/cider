cmake_minimum_required(VERSION 3.10)
project(
	cider-compiler
	VERSION 0.0.0
	LANGUAGES CXX
)

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
	src/backend/codegen/RegAllocator.cpp
	src/backend/codegen/riscv/RiscvAssembler.cpp
	src/backend/codegen/riscv/RiscvCodeGenerator.cpp
	src/backend/codegen/riscv/RiscvInstruction.cpp
	src/backend/codegen/x86/x86.cpp
	src/backend/codegen/x86/x86Assembler.cpp
	src/backend/codegen/x86/x86CodeGenerator.cpp
	src/backend/codegen/x86/x86Instruction.cpp
	src/backend/objwriter/elf/ObjectWriter_ELF.cpp
	src/utils/CliParser.cpp
	src/utils/common.cpp
	src/utils/error.cpp
	src/utils/logging.cpp
)
# include headers in src
target_include_directories(ciderc_lib SYSTEM PUBLIC src/)

target_compile_definitions(ciderc_lib PRIVATE
	CIDER_VERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
	CIDER_VERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
	CIDER_VERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
	CIDER_VERSION="${CMAKE_PROJECT_VERSION}"
)

# Main compiler executable
add_executable(ciderc src/main.cpp)
target_link_libraries(ciderc PRIVATE ciderc_lib)

# Copy ciderc into root (for convenience/test runner)
add_custom_command(TARGET ciderc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:ciderc>
            ${CMAKE_SOURCE_DIR}/
)

# Testing
add_subdirectory(testing)