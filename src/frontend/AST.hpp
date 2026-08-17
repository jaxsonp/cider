#pragma once

#include <stdint.h>
#include <istream>
#include <ostream>
#include <vector>
#include <optional>
#include <unordered_map>
#include <memory>
#include <string>
#include <variant>

#include "frontend/FrontendType.hpp"
#include "ir/IrWriter.hpp"
#include "utils/common.hpp"
#include "utils/error.hpp"
#include "utils/ArbInteger.hpp"

namespace ast
{
	enum class SymbolType
	{
		Variable,
	};

	struct Symbol
	{
		std::string name;
		FrontendType type;

		Symbol(std::string name, FrontendType type) : name(name), type(type) {}
	};

	class SymbolScope
	{
		std::unordered_map<std::string, Symbol> symbols;

		/// @brief Parent (if not root)
		SymbolScope *parent;

	public:
		virtual SymbolScope *get_parent() { return this->parent; };
		virtual inline bool is_root() { return false; };

		/// @brief Look up a symbol by name
		/// @param name Name to look up
		/// @param recursive If true (default), search enclosing scopes when not found locally
		/// @return Pointer to the symbol, nullptr if not found
		Symbol *find(const std::string &name, bool recursive = true);

		/// @brief Insert a symbol into this symbol table scope, throwing on collision
		/// @param name Symbol name
		/// @param type Symbol type
		void add(std::string name, FrontendType type);
		/// @brief Insert a symbol into this symbol table scope, throwing on collision
		/// @param symbol Name/type pair
		void add(std::pair<std::string, FrontendType> symbol) { this->add(symbol.first, symbol.second); }

		SymbolScope(SymbolScope *parent);
	};

	class GlobalSymbolTable : public SymbolScope
	{
	public:
		virtual inline bool is_root() { return false; };
		GlobalSymbolTable() : SymbolScope(nullptr) {}
	};

	struct SemanticAnalysisState
	{
		std::optional<FrontendType> cur_fn_return_type = std::nullopt;
	};

	// Node interface
	class Node
	{
	public:
		/// @brief Location in the source code that this AST node spans
		SourceLocRange src_loc;

		Node(SourceLocRange src_loc) : src_loc(src_loc) {};

		/// @brief Recursively resolve symbols
		/// @param scope Current symbol scope
		virtual void resolve_symbols(SymbolScope *scope) = 0;

		/// @brief Recursively resolve types and check semantics
		virtual void check_semantics(SemanticAnalysisState &state) const = 0;

		/// @brief Recursively write this node and its children to `out`
		/// @param out Stream to write output to
		/// @param depth Indentation level
		virtual void print(std::ostream &out, unsigned int depth = 0) const = 0;

		virtual ~Node() = default;
	};

	// TLD interface
	class TopLevelDeclaration : public Node
	{
	protected:
		TopLevelDeclaration(SourceLocRange src_loc) : Node(src_loc) {};

	public:
		virtual std::pair<std::string, FrontendType> declares() const = 0;
		virtual void emit_ir(IrWriter &writer) const = 0;
	};

	// expression interface
	class ExpressionNode : public Node
	{
	protected:
		ExpressionNode(SourceLocRange src_loc) : Node(src_loc) {};
		ExpressionNode(SourceLocRange src_loc, FrontendType type) : Node(src_loc), type(type) {};

	public:
		FrontendType type = FrontendType::unresolved();

		/// @brief Resolve and check the type of this expression. Should only be called after symbols have been
		/// resolved
		virtual void resolve_type() = 0;

		/// @brief Write IR for this node to the provided writer
		/// @return ID of output register
		virtual ir::VRegId emit_ir(IrWriter &writer) const = 0;
	};

	// statement interface
	class StatementNode : public Node
	{
	protected:
		StatementNode(SourceLocRange src_loc) : Node(src_loc) {};

	public:
		/// @brief Write IR for this node to the provided writer
		virtual void emit_ir(IrWriter &writer) const = 0;
	};

	// EXPRESSIONS =============================================================

	/// Integer literal
	struct IntegerLiteralExpression : public ExpressionNode
	{
		ArbInteger value;

		IntegerLiteralExpression(SourceLocRange src_loc, ArbInteger value)
			: ExpressionNode(src_loc, FrontendType::unresolved_int()), value(value) {}

		IntegerLiteralExpression(SourceLocRange src_loc, ArbInteger value, FrontendType type_annotation)
			: ExpressionNode(src_loc, type_annotation), value(value) {}

		void resolve_symbols(SymbolScope *scope) override;

		void resolve_type() override;
		void check_semantics(SemanticAnalysisState &state) const override;

		ir::VRegId emit_ir(IrWriter &writer) const override;

		void print(std::ostream &out, unsigned int depth = 0) const;
	};

	/// Boolean literal
	struct BooleanLiteralExpression : public ExpressionNode
	{
		bool value;

		BooleanLiteralExpression(SourceLocRange src_loc, bool value)
			: ExpressionNode(src_loc, FrontendType::boolean()), value(value) {}

		void resolve_symbols(SymbolScope *scope) override;

		void resolve_type() override;
		void check_semantics(SemanticAnalysisState &state) const override;

		ir::VRegId emit_ir(IrWriter &writer) const override;
		void print(std::ostream &out, unsigned int depth = 0) const;
	};

	struct IdentifierExpression : public ExpressionNode
	{
		std::string name;
		/// @brief Symbol that this identifier is referencing. nullptr until `.resolved_symbols(...)` is called
		Symbol *symbol = nullptr;

		IdentifierExpression(SourceLocRange src_loc, std::string_view name)
			: ExpressionNode(src_loc), name(name) {}

		void resolve_symbols(SymbolScope *scope) override;

		void resolve_type() override;
		void check_semantics(SemanticAnalysisState &state) const override;

		ir::VRegId emit_ir(IrWriter &writer) const override;
		void print(std::ostream &out, unsigned int depth = 0) const;
	};

	struct BinaryExpression : public ExpressionNode
	{
		enum class BinaryOperation
		{
			LogicalOr,
			LogicalAnd,
			Equal,
			NotEqual,
			LessThan,
			LessThanOrEqual,
			GreaterThan,
			GreaterThanOrEqual,
			BitwiseOr,
			BitwiseXor,
			BitwiseAnd,
			ShiftLeft,
			ShiftRight,
			Add,
			Subtract,
			Multiply,
			Divide,
			Modulus,
		};

		std::unique_ptr<ExpressionNode> l_expr;
		std::unique_ptr<ExpressionNode> r_expr;
		BinaryOperation operation;

		BinaryExpression(SourceLocRange src_loc, std::unique_ptr<ExpressionNode> l_expr, std::unique_ptr<ExpressionNode> r_expr, BinaryOperation operation)
			: ExpressionNode(src_loc), l_expr(std::move(l_expr)), r_expr(std::move(r_expr)), operation(operation) {}

		void resolve_symbols(SymbolScope *scope) override;

		void resolve_type() override;
		void check_semantics(SemanticAnalysisState &state) const override;

		ir::VRegId emit_ir(IrWriter &writer) const override;

		void print(std::ostream &out, unsigned int depth = 0) const;
		std::string_view operator_string() const;
	};

	struct UnaryExpression : public ExpressionNode
	{
		enum class UnaryOperation
		{
			LogicalNot,
			BitwiseNot,
			Negation,
		};
		std::unique_ptr<ExpressionNode> expr;
		UnaryOperation operation;

		UnaryExpression(SourceLocRange src_loc, std::unique_ptr<ExpressionNode> expr, UnaryOperation operation)
			: ExpressionNode(src_loc), expr(std::move(expr)), operation(operation) {}

		void resolve_symbols(SymbolScope *scope) override;

		void resolve_type() override;
		void check_semantics(SemanticAnalysisState &state) const override;

		ir::VRegId emit_ir(IrWriter &writer) const override;

		void print(std::ostream &out, unsigned int depth = 0) const;
		std::string_view operator_string() const;
	};

	struct FunctionCall : public ExpressionNode
	{
		std::unique_ptr<ExpressionNode> callee;

		FunctionCall(SourceLocRange src_loc, std::unique_ptr<ExpressionNode> callee)
			: ExpressionNode(src_loc), callee(std::move(callee)) {}

		void resolve_symbols(SymbolScope *scope) override;

		void resolve_type() override;
		void check_semantics(SemanticAnalysisState &state) const override;

		ir::VRegId emit_ir(IrWriter &writer) const override;
		void print(std::ostream &out, unsigned int depth = 0) const;
	};

	// STATEMENTS ==============================================================

	struct ReturnStatement : StatementNode
	{
		/// @brief Optional expression in return statement
		std::optional<std::unique_ptr<ExpressionNode>> expr;

		ReturnStatement(SourceLocRange src_loc)
			: StatementNode(src_loc) {};
		ReturnStatement(SourceLocRange src_loc, std::unique_ptr<ExpressionNode> expr)
			: StatementNode(src_loc), expr(std::move(expr)) {};

		void resolve_symbols(SymbolScope *scope) override;

		void check_semantics(SemanticAnalysisState &state) const override;

		void emit_ir(IrWriter &writer) const override;

		void print(std::ostream &out, unsigned int depth = 0) const;
	};

	// FUNCTION STUFF ==========================================================

	struct ArgDefinition : Node
	{
		FrontendType type;
		std::string name;

		ArgDefinition(SourceLocRange src_loc, std::string name, FrontendType type)
			: Node(src_loc), name(name), type(type) {};

		void resolve_symbols(SymbolScope *scope) override;

		void check_semantics(SemanticAnalysisState &state) const override;

		void print(std::ostream &out, unsigned int depth = 0) const;
	};

	struct FunctionDefinition : TopLevelDeclaration
	{
		std::string name;
		std::vector<ArgDefinition> args;
		FrontendType return_type;
		std::vector<std::unique_ptr<StatementNode>> body_statements;
		/// nullopt if there is no return expression
		std::optional<std::unique_ptr<ExpressionNode>> body_return_expr;

		// nullptr until symbol resolution pass
		std::unique_ptr<SymbolScope> scope = nullptr;

		FunctionDefinition(SourceLocRange src_loc, std::string name, std::vector<ArgDefinition> args,
						   FrontendType return_type, std::vector<std::unique_ptr<StatementNode>> body_statements,
						   std::optional<std::unique_ptr<ExpressionNode>> body_return_expr = std::nullopt)
			: TopLevelDeclaration(src_loc), name(std::move(name)), args(std::move(args)),
			  return_type(std::move(return_type)), body_statements(std::move(body_statements)),
			  body_return_expr(std::move(body_return_expr)) {}

		void resolve_symbols(SymbolScope *scope) override;

		void check_semantics(SemanticAnalysisState &state) const override;

		void emit_ir(IrWriter &writer) const override;

		void print(std::ostream &out, unsigned int depth = 0) const;

		inline std::pair<std::string, FrontendType> declares() const override
		{
			std::vector<FrontendType> param_types;
			param_types.reserve(this->args.size());
			for (const ArgDefinition &arg : this->args)
				param_types.push_back(arg.type);
			return {this->name, FrontendType::function(this->return_type, std::move(param_types))};
		};
	};
}

class AST
{
	std::vector<std::unique_ptr<ast::TopLevelDeclaration>> tlds;
	ast::GlobalSymbolTable *symbols;

	AST() = delete;

public:
	/// @brief Generate an AST from input stream
	///
	/// @details AST generation is done in three stages:
	/// 	1. Recursive descent parsing on tokenized input to create tree structure
	/// 	2. Traverse tree, resolving symbols
	/// 	3. Traverse tree again, performing type checking and semantic analysis
	AST(std::istream &input);

	/// @brief Write a textual representation of the tree to a stream
	void print(std::ostream &out) const;

	/// @brief Generate an IR object from this AST
	ir::Object emit_ir() const;
};