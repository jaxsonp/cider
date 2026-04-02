#pragma once

#include <stdint.h>
#include <istream>
#include <vector>
#include <optional>
#include <unordered_map>
#include <memory>
#include <string>

#include "Lexer.hpp"
#include "IR.hpp"
#include "types.hpp"
#include "utils/common.hpp"

namespace ast
{
	enum class SymbolType
	{
		Variable,
	};

	struct Symbol
	{
		std::string name;
		FrontendType variable_type;

		Symbol(std::string _name, FrontendType _variable_type) : name(_name), variable_type(_variable_type) {}
	};

	class SymbolScope
	{
		std::unordered_map<std::string, Symbol> symbols;

	public:
		/// @brief Parent (if not root)
		SymbolScope *parent;

		virtual SymbolScope *get_parent() { return this->parent; };
		virtual inline bool is_root() { return false; };

		// std::optional<Type> find_symbol(const std::string &name, bool recursive = true);

		/// @brief Insert a symbol into this symbol table scope, throwing on collision
		/// @param name Symbol name
		/// @param type Symbol type
		void add(std::string name, FrontendType type);
		/// @brief Insert a symbol into this symbol table scope, throwing on collision
		/// @param symbol Name/type pair
		inline void add(std::pair<std::string, FrontendType> symbol) { this->add(symbol.first, symbol.second); }

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
		std::optional<FrontendType> fn_return_type;
		SymbolScope *cur_scope;
		GlobalSymbolTable *symbols;

		SemanticAnalysisState(GlobalSymbolTable *symbols);
	};

	// Node interface
	class Node
	{
	public:
		SourceLocRange src_loc;

		virtual void check_semantics(SemanticAnalysisState state) const = 0;
		virtual void debug_print(unsigned int depth) const = 0;

		virtual ~Node() = default;
	};

	// TLD interface
	class TopLevelDeclaration : public Node
	{
	public:
		virtual std::pair<std::string, FrontendType> declares() const = 0;
		virtual void emitIr(IrWriter &writer) const = 0;

		static std::optional<std::unique_ptr<TopLevelDeclaration>> try_parse(Lexer &lexer);
	};

	// expression interface
	class ExpressionNode : public Node
	{
	public:
		static std::optional<std::unique_ptr<ExpressionNode>> try_parse(Lexer &lexer);
		/// @return ID of output register
		virtual ir::VRegId emitIr(IrWriter &writer) const = 0;

		virtual FrontendType get_type() const = 0;
	};

	// statement interface
	class StatementNode : public Node
	{
	public:
		static std::optional<std::unique_ptr<StatementNode>> try_parse(Lexer &lexer);
		virtual void emitIr(IrWriter &writer) const = 0;
	};

	// EXPRESSIONS =============================================================

	struct IntegerLiteralExpression : public ExpressionNode
	{
		uint32_t raw_value;
		FrontendType type;

		void check_semantics(SemanticAnalysisState state) const override;
		void debug_print(unsigned int depth = 0) const override;
		ir::VRegId emitIr(IrWriter &writer) const override;
		static std::optional<std::unique_ptr<IntegerLiteralExpression>> try_parse(Lexer &lexer);
		inline FrontendType get_type() const override { return this->type; };
	};

	struct LogicalOrExpression : public ExpressionNode
	{
		std::unique_ptr<ExpressionNode> l_expr;
		std::unique_ptr<ExpressionNode> r_expr;

		void check_semantics(SemanticAnalysisState state) const override;
		void debug_print(unsigned int depth = 0) const override;
		ir::VRegId emitIr(IrWriter &writer) const override;
		static std::optional<std::unique_ptr<ExpressionNode>> try_parse(Lexer &lexer);
		inline FrontendType get_type() const override { return l_expr->get_type(); };
	};

	struct LogicalAndExpression : public ExpressionNode
	{
		std::unique_ptr<ExpressionNode> l_expr;
		std::unique_ptr<ExpressionNode> r_expr;

		void check_semantics(SemanticAnalysisState state) const override;
		void debug_print(unsigned int depth = 0) const override;
		ir::VRegId emitIr(IrWriter &writer) const override;
		static std::optional<std::unique_ptr<ExpressionNode>> try_parse(Lexer &lexer);
		inline FrontendType get_type() const override { return l_expr->get_type(); };
	};

	struct EqualityExpression : public ExpressionNode
	{
		std::unique_ptr<ExpressionNode> l_expr;
		std::unique_ptr<ExpressionNode> r_expr;
		/// True if not equals
		bool notted;

		void check_semantics(SemanticAnalysisState state) const override;
		void debug_print(unsigned int depth = 0) const override;
		ir::VRegId emitIr(IrWriter &writer) const override;
		static std::optional<std::unique_ptr<ExpressionNode>> try_parse(Lexer &lexer);
		inline FrontendType get_type() const override { return l_expr->get_type(); };
	};

	struct ComparisonExpression : public ExpressionNode
	{
		enum class ComparisonKind
		{
			GREATER_THAN,
			GREATER_THAN_OR_EQUAL,
			LESS_THAN,
			LESS_THAN_OR_EQUAL,
		};

		std::unique_ptr<ExpressionNode> l_expr;
		std::unique_ptr<ExpressionNode> r_expr;
		ComparisonKind kind;

		void check_semantics(SemanticAnalysisState state) const override;
		void debug_print(unsigned int depth = 0) const override;
		ir::VRegId emitIr(IrWriter &writer) const override;
		static std::optional<std::unique_ptr<ExpressionNode>> try_parse(Lexer &lexer);
		inline FrontendType get_type() const override { return l_expr->get_type(); };
	};

	struct BitwiseOrExpression : public ExpressionNode
	{
		std::unique_ptr<ExpressionNode> l_expr;
		std::unique_ptr<ExpressionNode> r_expr;

		void check_semantics(SemanticAnalysisState state) const override;
		void debug_print(unsigned int depth = 0) const override;
		ir::VRegId emitIr(IrWriter &writer) const override;
		static std::optional<std::unique_ptr<ExpressionNode>> try_parse(Lexer &lexer);
		inline FrontendType get_type() const override { return l_expr->get_type(); };
	};

	struct BitwiseXorExpression : public ExpressionNode
	{
		std::unique_ptr<ExpressionNode> l_expr;
		std::unique_ptr<ExpressionNode> r_expr;

		void check_semantics(SemanticAnalysisState state) const override;
		void debug_print(unsigned int depth = 0) const override;
		ir::VRegId emitIr(IrWriter &writer) const override;
		static std::optional<std::unique_ptr<ExpressionNode>> try_parse(Lexer &lexer);
		inline FrontendType get_type() const override { return l_expr->get_type(); };
	};

	struct BitwiseAndExpression : public ExpressionNode
	{
		std::unique_ptr<ExpressionNode> l_expr;
		std::unique_ptr<ExpressionNode> r_expr;

		void check_semantics(SemanticAnalysisState state) const override;
		void debug_print(unsigned int depth = 0) const override;
		ir::VRegId emitIr(IrWriter &writer) const override;
		static std::optional<std::unique_ptr<ExpressionNode>> try_parse(Lexer &lexer);
		inline FrontendType get_type() const override { return l_expr->get_type(); };
	};

	struct BitshiftExpression : public ExpressionNode
	{
		std::unique_ptr<ExpressionNode> l_expr;
		std::unique_ptr<ExpressionNode> r_expr;
		bool left_shift;

		void check_semantics(SemanticAnalysisState state) const override;
		void debug_print(unsigned int depth = 0) const override;
		ir::VRegId emitIr(IrWriter &writer) const override;
		static std::optional<std::unique_ptr<ExpressionNode>> try_parse(Lexer &lexer);
		inline FrontendType get_type() const override { return l_expr->get_type(); };
	};

	struct AdditiveExpression : public ExpressionNode
	{
		std::unique_ptr<ExpressionNode> l_expr;
		std::unique_ptr<ExpressionNode> r_expr;
		bool plus;

		void check_semantics(SemanticAnalysisState state) const override;
		void debug_print(unsigned int depth = 0) const override;
		ir::VRegId emitIr(IrWriter &writer) const override;
		static std::optional<std::unique_ptr<ExpressionNode>> try_parse(Lexer &lexer);
		inline FrontendType get_type() const override { return l_expr->get_type(); };
	};

	struct MultiplicativeExpression : public ExpressionNode
	{
		enum class MultOperation
		{
			Multiplication,
			Division,
			Modulus
		};
		std::unique_ptr<ExpressionNode> l_expr;
		std::unique_ptr<ExpressionNode> r_expr;
		MultOperation operation;

		void check_semantics(SemanticAnalysisState state) const override;
		void debug_print(unsigned int depth = 0) const override;
		ir::VRegId emitIr(IrWriter &writer) const override;
		static std::optional<std::unique_ptr<ExpressionNode>> try_parse(Lexer &lexer);
		inline FrontendType get_type() const override { return l_expr->get_type(); };
	};

	// STATEMENTS ==============================================================

	struct ReturnStatement : StatementNode
	{
		/// Null if there is no expression
		std::unique_ptr<ExpressionNode> expr;

		void check_semantics(SemanticAnalysisState state) const override;
		void debug_print(unsigned int depth = 0) const override;
		void emitIr(IrWriter &writer) const override;

		static std::optional<std::unique_ptr<ReturnStatement>> try_parse(Lexer &lexer);

		FrontendType return_type() const;
	};

	// FUNCTION STUFF ==========================================================

	struct ArgDefinition : Node
	{
		FrontendType type;
		std::string name;

		void check_semantics(SemanticAnalysisState state) const override;
		void debug_print(unsigned int depth = 0) const override;

		static std::optional<ArgDefinition> try_parse(Lexer &lexer);

	private:
		ArgDefinition() = default;
	};

	struct FunctionDefinition : TopLevelDeclaration
	{
		std::string name;
		std::vector<ArgDefinition> args;
		FrontendType return_type;
		std::vector<std::unique_ptr<StatementNode>> body_statements;
		/// null if there is no return expression
		std::unique_ptr<ExpressionNode> body_return_expr;
		SymbolScope *scope;

		void check_semantics(SemanticAnalysisState state) const override;
		void debug_print(unsigned int depth = 0) const override;
		void emitIr(IrWriter &writer) const override;

		static std::optional<FunctionDefinition> try_parse(Lexer &lexer);

		inline std::pair<std::string, FrontendType> declares() const override
		{
			return {this->name, this->return_type};
		};

	private:
		FunctionDefinition()
			: scope(new SymbolScope(nullptr)) {}
	};
}

class AST
{
	std::vector<std::unique_ptr<ast::TopLevelDeclaration>> tlds;
	ast::GlobalSymbolTable *symbols;

public:
	/// attempt to create an AST from input stream
	AST(std::istream &input);

	void debug_print() const;
	IrObject emitIr() const;

	// no need to delete symbol tables here, they are owned and will be deleted by AST nodes
	~AST() = default;
};