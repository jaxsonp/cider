#pragma once

#include <string>
#include <variant>

#include "frontend/Lexer.hpp"

/// @brief A "physical" type
enum class ConcreteType
{
	VOID,
	U32,
	I32,
};

std::string to_string(ConcreteType &t);

/// @brief A type in the context of the source code
class FrontendType
{
public:
	struct Unknown
	{
		std::string str;
		SourceLocRange loc;

		bool operator==(const Unknown &other) const { return this->str == other.str; };
	};

	FrontendType();
	FrontendType(ConcreteType type);
	FrontendType(Token tok);
	FrontendType(std::string s, SourceLocRange loc);

	bool is_concrete() const;
	std::optional<Unknown> is_unknown() const;

	std::string to_string() const;

	friend bool operator==(const FrontendType &lhs, const FrontendType &rhs);
	friend bool operator==(const FrontendType &lhs, const ConcreteType &rhs);
	friend bool operator==(const ConcreteType &lhs, const FrontendType &rhs);

private:
	std::variant<ConcreteType, Unknown> variant;
};
