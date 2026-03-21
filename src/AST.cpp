#include "AST.hpp"

#include <format>
#include <iostream>

#include "utils/error.hpp"

AST::AST(Lexer &lexer)
{
	try
	{
		program = ast::node_types::Program::try_parse(lexer);
	}
	catch (CompileError e)
	{
		e.add_prefix("Error while syntax parsing: ");
		throw e;
	}
}

void AST::debug_print()
{
	program.debug_print();
}

namespace ast
{
	namespace node_types
	{
		void IntegerLiteral::debug_print(unsigned int depth)
		{
			std::cout << std::string(depth * 2, ' ');
			std::cout << "Integer literal (value: \"" << this->value << "\", type annotation: \"" << this->type_annotation << "\")" << std::endl;
		}

		std::optional<IntegerLiteral> IntegerLiteral::try_parse(Lexer &lexer)
		{
			if (lexer.peek().type != TokenType::INT_LITERAL)
				return std::nullopt;
			std::string s = lexer.take().token_str;

			// finding where numbers stop
			auto type_annotation_pos = s.begin();
			while (type_annotation_pos != s.end() && is_numeric(*type_annotation_pos))
				++type_annotation_pos;

			return IntegerLiteral(
				std::string(s.begin(), type_annotation_pos),
				std::string(type_annotation_pos, s.end()));
		}

		void Expression::debug_print(unsigned int depth)
		{
			std::cout << std::string(depth * 2, ' ');
			std::cout << "Expression" << std::endl;
			if (std::holds_alternative<IntegerLiteral>(this->variant))
				std::get<IntegerLiteral>(this->variant).debug_print(depth + 1);
			else
				throw std::logic_error("Unhandled expression variant");
		}

		std::optional<Expression> Expression::try_parse(Lexer &lexer)
		{
			if (auto int_lit = IntegerLiteral::try_parse(lexer))
			{
				return Expression(int_lit.value());
			}
			return std::nullopt;
		}

		void ReturnStatement::debug_print(unsigned int depth)
		{
			std::cout << std::string(depth * 2, ' ');
			std::cout << "Return statement (expression present: " << bool_str(this->expr.has_value()) << ")" << std::endl;
			if (this->expr.has_value())
				this->expr.value().debug_print(depth + 1);
		}

		std::optional<ReturnStatement> ReturnStatement::try_parse(Lexer &lexer)
		{
			if (lexer.peek().type != TokenType::KEYWORD_RETURN)
				return std::nullopt;
			lexer.take();

			if (lexer.peek().type == TokenType::SEMICOLON)
				return ReturnStatement();

			auto expr = Expression::try_parse(lexer);
			if (!expr.has_value())
				throw CompileError("Expected expression", lexer.pos);
			lexer.expect(TokenType::SEMICOLON);

			return ReturnStatement(expr.value());
		}

		void Statement::debug_print(unsigned int depth)
		{
			std::cout << std::string(depth * 2, ' ');
			std::cout << "Statement" << std::endl;
			if (std::holds_alternative<ReturnStatement>(this->variant))
				std::get<ReturnStatement>(this->variant).debug_print(depth + 1);
			else
				throw std::logic_error("Unhandled statement variant");
		}

		std::optional<Statement> Statement::try_parse(Lexer &lexer)
		{
			if (auto return_stmt = ReturnStatement::try_parse(lexer))
				return Statement(return_stmt.value());
			else
				return std::nullopt;
		}

		void Block::debug_print(unsigned int depth)
		{
			std::cout << std::string(depth * 2, ' ');
			std::cout << "Block (statements: " << this->statements.size() << ", expression present: " << bool_str(this->expression.has_value()) << ")" << std::endl;
			for (auto &stmt : this->statements)
			{
				stmt.debug_print(depth + 1);
			}
			if (this->expression.has_value())
			{
				this->expression.value().debug_print(depth + 1);
			}
		}

		std::optional<Block> Block::try_parse(Lexer &lexer)
		{
			if (lexer.peek().type != TokenType::L_BRACKET)
				return std::nullopt;
			lexer.take();

			Block block;
			while (true)
			{
				if (auto stmt = Statement::try_parse(lexer))
				{
					block.statements.push_back(stmt.value());
					continue;
				}
				break;
			}
			if (auto expr = Expression::try_parse(lexer))
			{
				block.expression = expr;
			}
			lexer.expect(TokenType::R_BRACKET);
			return block;
		}

		void ArgDefinition::debug_print(unsigned int depth)
		{
			std::cout << std::string(depth * 2, ' ');
			std::cout << "Argument definition (type: \"" << this->type << "\", name: \"" << this->name << "\")" << std::endl;
		}

		std::optional<ArgDefinition> ArgDefinition::try_parse(Lexer &lexer)
		{
			if (lexer.peek().type != TokenType::IDENT)
				return std::nullopt;

			Token ident = lexer.take();

			lexer.expect(TokenType::COLON);
			// TODO
		}

		void FunctionDefinition::debug_print(unsigned int depth)
		{
			std::cout << std::string(depth * 2, ' ');
			std::cout << "Function definition (name: \"" << this->name << "\", args: " << this->args.size() << ", return type: \"" << this->return_type << "\")" << std::endl;
			for (ArgDefinition &arg : this->args)
			{
				arg.debug_print(depth + 1);
			}
			this->block.debug_print(depth + 1);
		}

		std::optional<FunctionDefinition> FunctionDefinition::try_parse(Lexer &lexer)
		{
			if (lexer.peek().type != TokenType::KEYWORD_FN)
				return std::nullopt;
			SourceLoc src_start = lexer.take().start;

			Token name_tok = lexer.expect(TokenType::IDENT);
			lexer.expect(TokenType::L_PAREN);

			// parsing args
			std::vector<ArgDefinition> args;
			while (true)
			{
				if (auto arg = ArgDefinition::try_parse(lexer))
				{
					args.push_back(arg.value());
					TokenType next_type = lexer.peek().type;
					if (next_type == TokenType::R_PAREN)
					{
						lexer.take();
						break;
					}
					else if (next_type == TokenType::COMMA)
					{
						lexer.take();
						continue;
					}
					else
						break;
				}
				else if (lexer.peek().type == TokenType::R_PAREN)
				{

					lexer.take();
					break;
				}
			}

			// return value
			std::string return_type("void");
			if (lexer.peek().type == TokenType::ARROW)
			{
				lexer.take();
				return_type = lexer.expect(TokenType::IDENT).token_str;
			}

			// must be a block here
			auto block = Block::try_parse(lexer);
			if (!block.has_value())
				throw CompileError("Expected a function body (e.g. \"{ ... }\")");

			return FunctionDefinition(name_tok.token_str, args, return_type, block.value());
		}

		void TopLevelDeclaration::debug_print(unsigned int depth)
		{
			std::cout << std::string(depth * 2, ' ');
			std::cout << "Top level declaration" << std::endl;
			if (std::holds_alternative<FunctionDefinition>(this->variant))
				std::get<FunctionDefinition>(this->variant).debug_print(depth + 1);
			else
				throw std::logic_error("Unhandled tld variant");
		}

		std::optional<TopLevelDeclaration> TopLevelDeclaration::try_parse(Lexer &lexer)
		{
			if (auto func = FunctionDefinition::try_parse(lexer))
			{
				return TopLevelDeclaration(func.value());
			}
			else
			{
				return std::nullopt;
			}
		}
		void Program::debug_print(unsigned int depth)
		{
			std::cout << std::string(depth * 2, ' ');
			std::cout << "Program root" << std::endl;
			for (TopLevelDeclaration &tld : this->tlds)
			{
				tld.debug_print(depth + 1);
			}
		}

		Program Program::try_parse(Lexer &lexer)
		{
			Program program;
			while (true)
			{
				if (auto tld = TopLevelDeclaration::try_parse(lexer))
					program.tlds.push_back(tld.value());
				else
					break;
			}
			return program;
		}
	}
}
