#include "frontend/AST.hpp"

#include <format>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <bit>

#include "utils/error.hpp"
#include "utils/logging.hpp"
#include "AST.hpp"

AST::AST(std::istream &input)
{
	log_vv("Initializing lexer on input stream");
	Lexer lexer(input);

	log_vv("Attempting to parse AST");
	while (true)
	{
		if (auto parsed = ast::TopLevelDeclaration::try_parse(lexer))
		{
			this->tlds.push_back(std::move(parsed.value()));
			continue;
		}
		break;
	}
	lexer.expect(TokenType::END_OF_FILE);
	log_vv("Parse successful");

	log_vv("Hoisting top level symbols");
	this->symbols = new ast::GlobalSymbolTable();
	ast::SemanticAnalysisState state(this->symbols);
	// top level hoisting
	for (const std::unique_ptr<ast::TopLevelDeclaration> &tld : this->tlds)
	{
		auto [name, type] = tld->declares();
		state.symbols->add(name, type);
	}

	// semantic checks
	log_vv("Performing semantic analysis");
	for (const std::unique_ptr<ast::TopLevelDeclaration> &tld : this->tlds)
	{
		tld->check_semantics(state);
	}
	log_vv("AST complete");
}

void AST::debug_print() const
{
	for (const std::unique_ptr<ast::TopLevelDeclaration> &tld : this->tlds)
	// semantic checks
	{
		tld->debug_print(0);
	}
}

IrObject *AST::emitIr() const
{
	IrWriter writer;

	for (const std::unique_ptr<ast::TopLevelDeclaration> &tld : this->tlds)
		tld->emitIr(writer);

	return writer.get_obj();
}

namespace ast
{
	void SymbolScope::add(std::string name, FrontendType type)
	{
		if (this->symbols.contains(name))
			throw NameError(std::format("Name \"{}\" is already defined at this point", name));
		auto [_, success] = this->symbols.insert({name, Symbol(name, type)});
		if (!success)
			throw InternalError(std::format("Failed to insert symbol \"{}\": {}", name, type.to_string()));
	}
	void SymbolScope::add(std::pair<std::string, FrontendType> pair)
	{
		this->add(pair.first, pair.second);
	}

	SymbolScope::SymbolScope(SymbolScope *_parent)
		: parent(_parent) {}

	SemanticAnalysisState::SemanticAnalysisState(GlobalSymbolTable *_symbols)
		: symbols(_symbols),
		  cur_scope(_symbols),
		  fn_return_type(std::nullopt)
	{
		if (_symbols == nullptr)
			throw InternalError("Attempted to initialize semantic analysis state with nullptr");
	}
	void IntegerLiteralExpression::check_semantics(SemanticAnalysisState state) const
	{
	}

	void IntegerLiteralExpression::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Integer literal expression (value: " << this->raw_value << ", type annotation: " << this->type.to_string() << ")";
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
	}

	ir::VRegId IntegerLiteralExpression::emitIr(IrWriter &writer) const
	{
		ir::VRegId result_reg(writer.new_vreg());
		writer.emit(new ir::instr::LoadImmInstruction(result_reg, std::bit_cast<uint32_t>(this->raw_value)));
		return result_reg;
	}

	std::optional<std::unique_ptr<IntegerLiteralExpression>> IntegerLiteralExpression::try_parse(Lexer &lexer)
	{
		if (lexer.peek().type != TokenType::INT_LITERAL)
			return std::nullopt;

		auto ret = std::make_unique<IntegerLiteralExpression>();
		Token tok = lexer.take();
		ret->src_loc = tok.loc;

		// finding where numbers stop
		auto type_annotation_pos = tok.str.begin();
		while (type_annotation_pos != tok.str.end() && is_numeric(*type_annotation_pos))
			++type_annotation_pos;

		std::string value_str(tok.str.begin(), type_annotation_pos);
		std::string type_str(type_annotation_pos, tok.str.end());

		// parsing type and value
		ret->type = FrontendType(type_str, tok.loc);
		if (ret->type == ConcreteType::I32)
		{
			long long value_ll = std::stoll(value_str);
			if (!std::in_range<int32_t>(value_ll))
				throw TypeError("Integer literal out of bounds for type: i32");
			uint32_t value_int32 = uint32_t(value_ll);
			ret->raw_value = std::bit_cast<uint32_t, int32_t>(value_int32);
		}
		else if (ret->type == ConcreteType::U32)
		{
			long long value_ll = std::stoll(value_str);
			if (!std::in_range<uint32_t>(value_ll))
				throw TypeError("Integer literal out of bounds for type: u32");
			ret->raw_value = uint32_t(value_ll);
		}
		else
			throw TypeError(std::format("Invalid type for integer literal: {}", ret->type.to_string()), ret->src_loc);

		return ret;
	}

	void ReturnStatement::check_semantics(SemanticAnalysisState state) const
	{
		// make sure type matches current function
		if (state.fn_return_type.has_value() && this->return_type() != state.fn_return_type.value())
		{
			throw TypeError(
				std::format(
					"Invalid return type, function requires {}, found: {}",
					state.fn_return_type.value().to_string(),
					this->return_type().to_string()),
				this->src_loc);
		}
	}

	void ReturnStatement::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Return statement (expression present: " << bool_str(this->expr != nullptr) << ")";
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
		if (this->expr != nullptr)
			this->expr->debug_print(depth + 1);
	}

	void ReturnStatement::emitIr(IrWriter &writer) const
	{
		if (this->expr != nullptr)
		{
			ir::VRegId result_reg = this->expr->emitIr(writer);
			writer.emit(new ir::instr::ReturnInstruction(result_reg));
		}
		else
		{
			writer.emit(new ir::instr::ReturnInstruction());
		}
	}

	std::optional<std::unique_ptr<ReturnStatement>> ReturnStatement::try_parse(Lexer &lexer)
	{
		if (lexer.peek().type != TokenType::KEYWORD_RETURN)
			return std::nullopt;

		auto ret = std::make_unique<ReturnStatement>();
		ret->src_loc.start = lexer.take().loc.start;

		if (lexer.peek().type == TokenType::SEMICOLON)
		{
			lexer.take();
			return ret;
		}

		SourceLoc expr_start = lexer.peek().loc.start;
		if (auto parsed_expr = ExpressionNode::try_parse(lexer))
			ret->expr = std::move(parsed_expr.value());
		else
			throw SyntaxError("Expected expression", expr_start);

		ret->src_loc.end = lexer.expect(TokenType::SEMICOLON).loc.end;

		return ret;
	}

	FrontendType ReturnStatement::return_type() const
	{
		if (this->expr != nullptr)
			return this->expr->get_type();
		else
			return FrontendType(ConcreteType::VOID);
	}

	/*void Block::check_semantics(SemanticAnalysisState state) const
	{
		// new scope
		state.cur_scope->children.push_back(this->symbols);
		this->symbols->parent = state.cur_scope;

		state.cur_scope = this->symbols;

		for (const std::unique_ptr<StatementNode> &stmt : this->statements)
		{
			stmt->check_semantics(state);
		}

		if (this->expr != nullptr)
		{
			this->expr->check_semantics(state);
			FrontendType expr_type = this->expr->get_type();
			if (state.fn_return_type.has_value() && expr_type != state.fn_return_type.value())
				throw TypeError(
					std::format(
						"Invalid return type, function requires {}, found: {}",
						state.fn_return_type.value().to_string(),
						expr_type.to_string()),
					this->src_loc);
		}
	}

	void Block::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Block (statements: " << this->statements.size() << ", expression present: " << bool_str(this->expr != nullptr) << ")";
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
		for (const auto &stmt : this->statements)
		{
			stmt->debug_print(depth + 1);
		}

		if (this->expr != nullptr)
			this->expr->debug_print(depth + 1);
	}

	void Block::emitIr(IrWriter &writer) const
	{
		int new_bblock = writer.new_bblock();
		for (const auto &stmt : this->statements)
		{
			// emit to this block
			writer.choose_bblock(new_bblock);
			stmt->emitIr(writer);
		}
		// TODO
	}

	std::optional<std::unique_ptr<Block>> Block::try_parse(Lexer &lexer)
	{
		if (lexer.peek().type != TokenType::L_BRACKET)
			return std::nullopt;

		auto ret = std::make_unique<Block>();
		ret->src_loc.start = lexer.take().loc.start;

		while (true)
		{
			if (auto stmt = StatementNode::try_parse(lexer))
			{
				ret->statements.push_back(std::move(stmt.value()));
				continue;
			}
			break;
		}
		if (auto expr = ExpressionNode::try_parse(lexer))
		{
			ret->expr = std::move(expr.value());
		}

		ret->src_loc.end = lexer.expect(TokenType::R_BRACKET).loc.end;
		return std::move(ret);
	}

	Block::Block()
	{
		this->symbols = std::make_shared<SymbolTable>();
	}*/

	void ArgDefinition::check_semantics(SemanticAnalysisState state) const
	{
		state.cur_scope->add({this->name, this->type});
	}

	void ArgDefinition::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Argument definition (type: " << this->type.to_string() << ", name: \"" << this->name << "\")";
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
	}

	std::optional<ArgDefinition> ArgDefinition::try_parse(Lexer &lexer)
	{
		if (lexer.peek().type != TokenType::IDENT)
			return std::nullopt;

		ArgDefinition ret;

		Token name_tok = lexer.take();
		ret.name = name_tok.str;
		ret.src_loc.start = name_tok.loc.start;

		lexer.expect(TokenType::COLON);

		Token type_tok = lexer.expect(TokenType::IDENT);
		ret.type = FrontendType(type_tok);
		ret.src_loc.end = type_tok.loc.end;

		return ret;
	}

	void FunctionDefinition::check_semantics(SemanticAnalysisState state) const
	{
		// TODO check function name
		// TODO add function to symbol table

		// new scope whose parent scope is global
		this->scope->parent = state.cur_scope;
		state.cur_scope = this->scope;

		state.fn_return_type = this->return_type;

		// check args
		for (const ArgDefinition &arg : this->args)
		{
			arg.check_semantics(state);
		}
		// check return type
		if (std::optional<FrontendType::Unknown> unknown = this->return_type.is_unknown())
			throw TypeError(std::format("Unknown type: \"{}\"", unknown->str), unknown->loc);

		// check body
		for (const std::unique_ptr<StatementNode> &stmt : this->body_statements)
		{
			stmt->check_semantics(state);
		}

		if (this->body_return_expr != nullptr)
		{
			this->body_return_expr->check_semantics(state);
			FrontendType expr_type = this->body_return_expr->get_type();
			if (state.fn_return_type.has_value() && expr_type != state.fn_return_type.value())
				throw TypeError(
					std::format(
						"Invalid return expression type, function requires {}, found: {}",
						state.fn_return_type.value().to_string(),
						expr_type.to_string()),
					this->src_loc);
		}
	}

	void FunctionDefinition::debug_print(unsigned int depth) const
	{
		std::cout << std::string(depth * 2, ' ');
		std::cout << "Function definition (name: \"" << this->name << "\", args: " << this->args.size() << ", return type: " << this->return_type.to_string() << ")";
		std::cout << " [" << this->src_loc.to_string() << "]" << std::endl;
		for (const ArgDefinition &arg : this->args)
			arg.debug_print(depth + 1);
		for (const std::unique_ptr<StatementNode> &stmt : this->body_statements)
			stmt->debug_print(depth + 1);
		if (this->body_return_expr != nullptr)
			this->body_return_expr->debug_print(depth + 1);
	}

	void FunctionDefinition::emitIr(IrWriter &writer) const
	{
		writer.new_function(this->name);

		// args
		unsigned short arg_index = 0;
		for (const ArgDefinition &arg : this->args)
		{
			writer.emit(new ir::instr::LoadArgInstruction(writer.new_vreg(), arg_index));
			++arg_index;
		}

		// body
		for (const std::unique_ptr<StatementNode> &stmt : this->body_statements)
			stmt->emitIr(writer);
		if (this->body_return_expr != nullptr)
			this->body_return_expr->emitIr(writer);
	}

	std::optional<FunctionDefinition> FunctionDefinition::try_parse(Lexer &lexer)
	{
		// fn keyword
		if (lexer.peek().type != TokenType::KEYWORD_FN)
			return std::nullopt;

		FunctionDefinition ret;
		ret.src_loc.start = lexer.take().loc.start;

		// name
		Token name_tok = lexer.expect(TokenType::IDENT);
		ret.name = name_tok.str;

		// args
		lexer.expect(TokenType::L_PAREN);
		while (true)
		{
			if (auto arg = ArgDefinition::try_parse(lexer))
			{
				ret.args.push_back(arg.value());
				if (lexer.peek().type == TokenType::COMMA)
				{
					lexer.take();
					continue;
				}
				else if (lexer.peek().type == TokenType::R_PAREN)
					lexer.take();
			}
			else if (lexer.peek().type == TokenType::R_PAREN)
				lexer.take();

			break;
		}

		// return type
		ret.return_type = FrontendType(ConcreteType::VOID);
		if (lexer.peek().type == TokenType::ARROW)
		{
			lexer.take();
			ret.return_type = FrontendType(lexer.expect(TokenType::IDENT));
		}

		// body
		lexer.expect(TokenType::L_CURLY_BRACKET);
		while (true)
		{
			if (auto stmt = StatementNode::try_parse(lexer))
			{
				ret.body_statements.push_back(std::move(stmt.value()));
				continue;
			}
			break;
		}

		// optional return expression
		if (auto expr = ExpressionNode::try_parse(lexer))
		{
			ret.body_return_expr = std::move(expr.value());
		}

		ret.src_loc.end = lexer.expect(TokenType::R_CURLY_BRACKET).loc.end;
		return ret;
	}

	inline std::pair<std::string, FrontendType> FunctionDefinition::declares() const
	{
		return {this->name, this->return_type};
	}

	FunctionDefinition::FunctionDefinition()
		: scope(new SymbolScope(nullptr)) {}

	std::optional<std::unique_ptr<TopLevelDeclaration>> TopLevelDeclaration::try_parse(Lexer &lexer)
	{
		if (auto function_def = FunctionDefinition::try_parse(lexer))
		{
			auto ret = std::make_unique<FunctionDefinition>(std::move(function_def.value()));
			ret->src_loc = function_def->src_loc;
			return ret;
		}

		return std::nullopt;
	}

	std::optional<std::unique_ptr<ExpressionNode>> ExpressionNode::try_parse(Lexer &lexer)
	{
		if (auto expr = IntegerLiteralExpression::try_parse(lexer))
		{
			return std::move(expr.value());
		}
		return std::nullopt;
	}

	std::optional<std::unique_ptr<StatementNode>> StatementNode::try_parse(Lexer &lexer)
	{
		if (auto stmt = ReturnStatement::try_parse(lexer))
		{
			return std::move(stmt.value());
		}
		return std::nullopt;
	}

}
