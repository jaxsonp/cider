#pragma once

#include <variant>
#include <vector>
#include <optional>

#include "Lexer.hpp"
#include "utils/common.hpp"

// AST nodes stuff
namespace ast
{

	// Node interface
	class Node
	{
	public:
		SourceLoc src_start;
		SourceLoc src_end;

		virtual void debug_print(unsigned int depth) = 0;

		virtual ~Node() = default;
	};

	// AST node types
	namespace node_types
	{

		struct IntegerLiteral : Node
		{
			std::string value;
			std::string type_annotation;

			void debug_print(unsigned int depth = 0) override;

			static std::optional<IntegerLiteral> try_parse(Lexer &lexer);

		private:
			IntegerLiteral(const std::string &value_, const std::string &type_)
				: value(value_), type_annotation(type_) {}
		};

		struct Expression : Node
		{
			std::variant<IntegerLiteral> variant;

			void debug_print(unsigned int depth = 0) override;

			static std::optional<Expression> try_parse(Lexer &lexer);

		private:
			Expression(IntegerLiteral int_lit) : variant(int_lit) {}
		};

		struct ReturnStatement : Node
		{
			std::optional<Expression> expr;

			void debug_print(unsigned int depth = 0) override;

			static std::optional<ReturnStatement> try_parse(Lexer &lexer);

		private:
			ReturnStatement() : expr() {};
			ReturnStatement(Expression expr_) : expr(expr_) {};
		};

		struct Statement : Node
		{
			std::variant<ReturnStatement> variant;

			void debug_print(unsigned int depth = 0) override;

			static std::optional<Statement> try_parse(Lexer &lexer);

		private:
			Statement(ReturnStatement stmt) : variant(stmt) {};
		};

		struct Block : Node
		{
			std::vector<Statement> statements;
			std::optional<Expression> expression;

			void debug_print(unsigned int depth = 0) override;

			static std::optional<Block> try_parse(Lexer &lexer);
		};

		struct ArgDefinition : Node
		{
			std::string type;
			std::string name;

			void debug_print(unsigned int depth = 0) override;

			static std::optional<ArgDefinition> try_parse(Lexer &lexer);
		};

		struct FunctionDefinition : Node
		{
			std::string name;
			std::vector<ArgDefinition> args;
			std::string return_type;
			Block block;

			void debug_print(unsigned int depth = 0) override;

			static std::optional<FunctionDefinition> try_parse(Lexer &lexer);

		private:
			FunctionDefinition(const std::string &name_, std::vector<ArgDefinition> args_, const std::string &return_type_, Block block_)
				: name(name_),
				  args(args_), return_type(return_type_), block(block_) {}
		};

		struct TopLevelDeclaration : Node
		{
			std::variant<FunctionDefinition> variant;

			void debug_print(unsigned int depth = 0) override;

			static std::optional<TopLevelDeclaration> try_parse(Lexer &lexer);

		private:
			TopLevelDeclaration(FunctionDefinition fn)
				: variant(fn)
			{
				src_start = fn.src_start;
				src_end = fn.src_end;
			}
		};

		struct Program : Node
		{
			std::vector<TopLevelDeclaration> tlds;

			void debug_print(unsigned int depth = 0) override;

			static Program try_parse(Lexer &lexer);
		};
	}
}

class AST
{
	ast::node_types::Program program;

public:
	// attempt to create an AST from tokens
	AST(Lexer &lexer);

	void debug_print();
};
