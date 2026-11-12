#pragma once

#include <stdint.h>
#include <istream>
#include <ostream>
#include <vector>
#include <optional>
#include <deque>
#include <unordered_map>
#include <memory>
#include <string>
#include <variant>

#include "frontend/FrontendType.hpp"
#include "ir/IrWriter.hpp"
#include "utils/common.hpp"
#include "utils/error.hpp"

namespace ast
{
	/// @brief What declared a symbol
	enum class SymbolKind
	{
		/// Top level function, can't be shadowed
		Function,
		/// Function parameter, may be shadowed by locals
		Parameter,
		/// Local variable, may shadow parameters and other locals
		Local,
	};

	struct Symbol
	{
		SymbolKind kind;
		std::string name;
		FrontendType type;
		bool mut;
	};

	class SymbolScope
	{
		/// @brief Every symbol ever added to this scope, including shadowed ones. A deque so that pointers into it
		/// (held by resolved identifiers) stay valid as more are added
		std::deque<Symbol> storage;
		/// @brief Name to the latest symbol declared with it in this scope
		std::unordered_map<std::string, Symbol *> symbols;

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

		/// @brief Insert a symbol into this symbol table scope, throwing if it collides with a symbol it isn't
		/// allowed to shadow. Only locals may shadow, and only parameters and other locals (in this scope or an
		/// enclosing one). Code resolved before this keeps pointing at the shadowed symbol
		/// @param symbol Pre-instantiated symbol
		/// @param loc Location of the declaration, for errors
		/// @return The stored symbol, valid for the lifetime of this scope
		Symbol *add(Symbol symbol, CompilerError::LocVariant loc = std::monostate{});

		SymbolScope(SymbolScope *parent);
	};

	class GlobalSymbolTable : public SymbolScope
	{
	public:
		virtual inline bool is_root() { return true; };
		GlobalSymbolTable() : SymbolScope(nullptr) {}
	};

	struct SemanticAnalysisState
	{
		std::optional<FrontendType> cur_fn_return_type = std::nullopt;
	};

	// TLD interface
	class TopLevelDeclaration
	{
	protected:
		TopLevelDeclaration(SourceLocRange src_loc) : src_loc(src_loc) {};

	public:
		/// @brief Location in the source code that this node spans
		SourceLocRange src_loc;

		/// @brief Recursively resolve symbols
		/// @param scope Current symbol scope
		virtual void resolve_symbols(SymbolScope *scope) = 0;

		/// @brief Recursively resolve types and check semantics
		virtual void check_semantics(SemanticAnalysisState &state) const = 0;

		/// @brief Recursively write this node and its children to `out`
		/// @param out Stream to write output to
		/// @param depth Indentation level
		virtual void print(std::ostream &out, unsigned int depth = 0) const = 0;

		virtual std::pair<std::string, FrontendType> declares() const = 0;
		virtual void emit_ir(IrWriter &writer) const = 0;

		virtual ~TopLevelDeclaration() = default;
	};

	// expression interface
	class ExpressionNode
	{
	protected:
		ExpressionNode(SourceLocRange src_loc) : src_loc(src_loc) {};
		ExpressionNode(SourceLocRange src_loc, FrontendType type) : src_loc(src_loc), type(type) {};

	public:
		/// @brief Location in the source code that this node spans
		SourceLocRange src_loc;

		/// @brief Recursively resolve symbols
		/// @param scope Current symbol scope
		virtual void resolve_symbols(SymbolScope *scope) = 0;

		/// @brief Recursively resolve types and check semantics
		virtual void check_semantics(SemanticAnalysisState &state) const = 0;

		/// @brief Recursively write this node and its children to `out`
		/// @param out Stream to write output to
		/// @param depth Indentation level
		virtual void print(std::ostream &out, unsigned int depth = 0) const = 0;

		FrontendType type = FrontendType::unresolved();

		/// @brief Resolve and check the type of this expression. Should only be called after symbols have been
		/// resolved
		virtual void resolve_type() = 0;

		/// @brief Write IR for this node to the provided writer
		/// @return ID of output register
		virtual ir::VRegId emit_ir(IrFunctionWriter &writer) const = 0;

		/// @brief Throw if this expression isn't a place that can be assigned to (or isn't mutable). Called on the
		/// left side of an assignment after symbols are resolved, but before its type is. By default an expression
		/// is a value, not a place
		virtual void check_assignable() const;

		/// @brief Write IR storing `value` into the place this expression names. Only called on expressions that
		/// passed `check_assignable`
		virtual void emit_assign(IrFunctionWriter &writer, ir::VRegId value) const;

		virtual ~ExpressionNode() = default;
	};

	// statement interface
	class StatementNode
	{
	protected:
		StatementNode(SourceLocRange src_loc) : src_loc(src_loc) {};

	public:
		/// @brief Location in the source code that this node spans
		SourceLocRange src_loc;

		/// @brief Recursively resolve symbols
		/// @param scope Current symbol scope
		virtual void resolve_symbols(SymbolScope *scope) = 0;

		/// @brief Recursively resolve types and check semantics
		virtual void check_semantics(SemanticAnalysisState &state) const = 0;

		/// @brief Recursively write this node and its children to `out`
		/// @param out Stream to write output to
		/// @param depth Indentation level
		virtual void print(std::ostream &out, unsigned int depth = 0) const = 0;

		/// @brief Write IR for this node to the provided writer
		virtual void emit_ir(IrFunctionWriter &writer) const = 0;

		virtual ~StatementNode() = default;
	};

	/// @brief Pattern interface. A pattern can't resolve or check itself without the type of the value it
	/// destructures, which comes from whatever contains it (a declaration's or parameter's type annotation)
	class PatternNode
	{
	protected:
		PatternNode(SourceLocRange src_loc) : src_loc(src_loc) {};

	public:
		/// @brief Location in the source code that this pattern spans
		SourceLocRange src_loc;

		/// @brief Check this pattern against the type of the value it destructures, and add the symbols it binds to
		/// `scope`
		/// @param scope Scope to declare bindings in
		/// @param type Type of the value this pattern matches
		/// @param kind What is declaring the bindings
		virtual void declare(SymbolScope *scope, const FrontendType &type, SymbolKind kind) = 0;

		/// @brief Bind a value to the names in this pattern
		/// @param value VReg holding the value being matched, of the type given to `declare`
		virtual void emit_ir(IrFunctionWriter &writer, ir::VRegId value) const = 0;

		/// @brief Whether this pattern is refutable (ie, can it fail to match)
		virtual bool is_refutable() const = 0;

		/// @brief Recursively write this pattern and its children to `out`
		virtual void print(std::ostream &out, unsigned int depth = 0) const = 0;

		virtual ~PatternNode() = default;
	};

	// EXPRESSIONS =============================================================

	/// Integer literal
	struct IntegerLiteralExpression : public ExpressionNode
	{
		uint64_t raw_value;

		IntegerLiteralExpression(SourceLocRange src_loc, uint64_t value)
			: ExpressionNode(src_loc, FrontendType::unresolved_int()), raw_value(value) {}

		IntegerLiteralExpression(SourceLocRange src_loc, uint64_t value, FrontendType type_annotation)
			: ExpressionNode(src_loc, type_annotation), raw_value(value) {}

		void resolve_symbols(SymbolScope *scope) override;

		void resolve_type() override;
		void check_semantics(SemanticAnalysisState &state) const override;

		ir::VRegId emit_ir(IrFunctionWriter &writer) const override;

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

		ir::VRegId emit_ir(IrFunctionWriter &writer) const override;
		void print(std::ostream &out, unsigned int depth = 0) const;
	};

	struct IdentifierExpression : public ExpressionNode
	{
		std::string name;
		/// @brief Symbol that this identifier is referencing. nullptr until `.resolved_symbols(...)` is called
		Symbol *symbol = nullptr;
		/// @brief Whether this identifier is directly being called, the only place a function name may appear until
		/// there are function values. Set by the enclosing FunctionCall
		bool is_callee = false;

		IdentifierExpression(SourceLocRange src_loc, std::string_view name, bool is_callee = false)
			: ExpressionNode(src_loc), name(name), is_callee(is_callee) {}

		void resolve_symbols(SymbolScope *scope) override;

		void resolve_type() override;
		void check_semantics(SemanticAnalysisState &state) const override;
		void check_assignable() const override;

		ir::VRegId emit_ir(IrFunctionWriter &writer) const override;
		void emit_assign(IrFunctionWriter &writer, ir::VRegId value) const override;
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

		ir::VRegId emit_ir(IrFunctionWriter &writer) const override;

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

		ir::VRegId emit_ir(IrFunctionWriter &writer) const override;

		void print(std::ostream &out, unsigned int depth = 0) const;
		std::string_view operator_string() const;
	};

	struct FunctionCall : public ExpressionNode
	{
		std::unique_ptr<ExpressionNode> callee;
		std::vector<std::unique_ptr<ExpressionNode>> args;

		FunctionCall(SourceLocRange src_loc, std::unique_ptr<ExpressionNode> callee, std::vector<std::unique_ptr<ExpressionNode>> args)
			: ExpressionNode(src_loc), callee(std::move(callee)), args(std::move(args)) {}

		void resolve_symbols(SymbolScope *scope) override;

		void resolve_type() override;
		void check_semantics(SemanticAnalysisState &state) const override;

		ir::VRegId emit_ir(IrFunctionWriter &writer) const override;
		void print(std::ostream &out, unsigned int depth = 0) const;
	};

	/// `place = value`, an expression of type void. Non-associative, so it can't be chained
	struct AssignmentExpression : public ExpressionNode
	{
		/// @brief What's being assigned to, has to pass `check_assignable`
		std::unique_ptr<ExpressionNode> place;
		std::unique_ptr<ExpressionNode> value;

		AssignmentExpression(SourceLocRange src_loc, std::unique_ptr<ExpressionNode> place, std::unique_ptr<ExpressionNode> value)
			: ExpressionNode(src_loc), place(std::move(place)), value(std::move(value)) {}

		void resolve_symbols(SymbolScope *scope) override;

		void resolve_type() override;
		void check_semantics(SemanticAnalysisState &state) const override;

		ir::VRegId emit_ir(IrFunctionWriter &writer) const override;
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

		void emit_ir(IrFunctionWriter &writer) const override;

		void print(std::ostream &out, unsigned int depth = 0) const;
	};

	struct ExpressionStatement : StatementNode
	{
		std::unique_ptr<ExpressionNode> expr;

		ExpressionStatement(SourceLocRange src_loc, std::unique_ptr<ExpressionNode> expr)
			: StatementNode(src_loc), expr(std::move(expr)) {};

		void resolve_symbols(SymbolScope *scope) override;

		void check_semantics(SemanticAnalysisState &state) const override;

		void emit_ir(IrFunctionWriter &writer) const override;

		void print(std::ostream &out, unsigned int depth = 0) const;
	};

	struct DeclarationStatement : StatementNode
	{
		std::unique_ptr<PatternNode> pattern;
		/// @brief Type annotation, the type of the whole value `pattern` matches
		FrontendType declared_type;
		std::unique_ptr<ExpressionNode> init_expr;

		DeclarationStatement(SourceLocRange src_loc, std::unique_ptr<PatternNode> pattern, FrontendType declared_type,
							 std::unique_ptr<ExpressionNode> init_expr)
			: StatementNode(src_loc), pattern(std::move(pattern)), declared_type(declared_type),
			  init_expr(std::move(init_expr)) {};

		void resolve_symbols(SymbolScope *scope) override;

		void check_semantics(SemanticAnalysisState &state) const override;

		void emit_ir(IrFunctionWriter &writer) const override;

		void print(std::ostream &out, unsigned int depth = 0) const;
	};

	// FUNCTION STUFF ==========================================================

	/// @brief A function parameter. Not polymorphic, its methods are called by the `FunctionDefinition` owning it
	struct ArgDefinition
	{
		SourceLocRange src_loc;
		std::unique_ptr<PatternNode> pattern;
		/// @brief Type annotation, the type of the whole value `pattern` matches
		FrontendType type;

		ArgDefinition(SourceLocRange src_loc, std::unique_ptr<PatternNode> pattern, FrontendType type)
			: src_loc(src_loc), pattern(std::move(pattern)), type(type) {};

		void resolve_symbols(SymbolScope *scope);

		void check_semantics(SemanticAnalysisState &state) const;

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

		std::pair<std::string, FrontendType> declares() const override;
	};

	// PATTERNS N STUFF ==========================================================

	/// Binds the whole value to a name, `x` or `mut x`
	struct BindingPattern : PatternNode
	{
		std::string name;
		bool mut;
		/// @brief Symbol this binding declared. nullptr until `.declare(...)` is called
		Symbol *symbol = nullptr;

		BindingPattern(SourceLocRange src_loc, std::string name, bool mut)
			: PatternNode(src_loc), name(std::move(name)), mut(mut) {};

		void declare(SymbolScope *scope, const FrontendType &type, SymbolKind kind) override;

		void emit_ir(IrFunctionWriter &writer, ir::VRegId value) const override;

		bool is_refutable() const override { return false; };

		void print(std::ostream &out, unsigned int depth = 0) const override;
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