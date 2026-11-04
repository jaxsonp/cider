#include "frontend/AST.hpp"

#include <format>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <bit>

#include "utils/error.hpp"
#include "utils/logging.hpp"
#include "frontend/Parser.hpp"
#include "AST.hpp"

AST::AST(std::istream &input)
{
	log_vv("Initializing lexer on input stream");
	Lexer lexer(input);

	log_vv("Attempting to parse AST");
	while (true)
	{
		if (auto parsed = parse::try_parse_top_level_decl(lexer))
		{
			this->tlds.push_back(std::move(parsed.value()));
			continue;
		}
		break;
	}
	lexer.expect(TokenType::END_OF_FILE);
	log_vv("Parse successful, performing semantic analysis");

	log_vvv("Building global symbol table");
	this->symbols = new ast::GlobalSymbolTable();
	// top level hoisting
	for (const std::unique_ptr<ast::TopLevelDeclaration> &tld : this->tlds)
	{
		auto [name, type] = tld->declares();
		this->symbols->add(ast::Symbol(name, type, ast::SymbolKind::Function), tld->src_loc);
	}

	log_vvv("Resolving symbols");
	for (const std::unique_ptr<ast::TopLevelDeclaration> &tld : this->tlds)
	{
		tld->resolve_symbols(this->symbols);
	}
	log_vvv("Performing semantic analysis/type checking");
	ast::SemanticAnalysisState state;
	for (const std::unique_ptr<ast::TopLevelDeclaration> &tld : this->tlds)
	{
		tld->check_semantics(state);
	}
	log_vv("AST complete");
}

void AST::print(std::ostream &out) const
{
	for (const std::unique_ptr<ast::TopLevelDeclaration> &tld : this->tlds)
	{
		tld->print(out);
	}
}

ir::Object AST::emit_ir() const
{
	IrWriter writer;

	for (const auto &tld : this->tlds)
	{
		tld->emit_ir(writer);
	}

	return writer.get_obj();
}

namespace ast
{
	Symbol *SymbolScope::find(const std::string &name, bool recursive)
	{
		auto it = this->symbols.find(name);
		if (it != this->symbols.end())
			return it->second;
		if (recursive && this->parent != nullptr)
			return this->parent->find(name, recursive);
		return nullptr;
	}

	void SymbolScope::add(Symbol symbol, CompilerError::LocVariant loc)
	{
		if (const Symbol *existing = this->find(symbol.name))
		{
			if (existing->kind == SymbolKind::Function)
				throw CompilerError::name_error(
					std::format("Name \"{}\" is already defined as a function, functions can't be shadowed", symbol.name), loc);
			if (symbol.kind != SymbolKind::Local)
				throw CompilerError::name_error(std::format("Name \"{}\" is already defined at this point", symbol.name), loc);
		}

		// shadowing replaces the name's entry, the shadowed symbol stays alive in storage
		Symbol &stored = this->storage.emplace_back(std::move(symbol));
		this->symbols.insert_or_assign(stored.name, &stored);
	}

	SymbolScope::SymbolScope(SymbolScope *parent)
		: parent(parent) {}

}