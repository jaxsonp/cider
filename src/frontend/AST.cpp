#include "frontend/AST.hpp"

#include <format>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <bit>

#include "utils/error.hpp"
#include "utils/logging.hpp"

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
	{
		tld->debug_print(0);
	}
}

IrObject AST::emitIr() const
{
	IrWriter writer;

	for (const std::unique_ptr<ast::TopLevelDeclaration> &tld : this->tlds)
	{
		tld->emitIr(writer);
	}

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

	FrontendType ReturnStatement::return_type() const
	{
		if (this->expr != nullptr)
			return this->expr->get_type();
		else
			return FrontendType(ConcreteType::VOID);
	}

}
