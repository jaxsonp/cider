#pragma once

#include <compare>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/// @brief Arbitrary-precision integer.
///
/// Stored as sign + vector of base-1billion digits, chosen so decimal conversion is
/// trivial is fast
class ArbInteger
{
public:
	/// @brief One "digit" of the magnitude, base 1,000,000,000, least-significant first
	using Digits = std::vector<uint32_t>;

private:
	static constexpr uint32_t BASE = 1000000000u;

	Digits digits;
	bool negative = false;

	// helpers that operate on digits, no signs

	static int compare_magnitude(const Digits &a, const Digits &b);
	static Digits add_magnitude(const Digits &a, const Digits &b);
	static Digits sub_magnitude(const Digits &a, const Digits &b);
	static Digits mul_magnitude(const Digits &a, const Digits &b);
	static Digits mul_magnitude_small(const Digits &a, uint32_t k);
	static void divmod_magnitude(const Digits &a, const Digits &b, Digits &quotient, Digits &remainder);

	/// @brief Construct directly from a magnitude and sign, normalizing zero to non-negative
	ArbInteger(Digits magnitude, bool is_negative);

	/// @brief Shared implementation for parsing from arbitrary based string.
	/// valid digit characters are assumed to be '0'..('0' + (radix - 1)).
	static ArbInteger from_str_radix(std::string_view str, uint32_t radix);

public:
	/// @brief Default constructor, value zero
	ArbInteger() = default;
	ArbInteger(int64_t value);
	ArbInteger(uint64_t value);

	/// @brief Parse a base-10 integer string, optionally prefixed with '+' or '-'.
	static ArbInteger from_str_base10(std::string_view str);
	/// @brief Parse a base-2 integer string, optionally prefixed with '+' or '-'. Digits must be '0' or '1'.
	static ArbInteger from_str_base2(std::string_view str);
	/// @brief Parse a base-8 integer string, optionally prefixed with '+' or '-'. Digits must be '0'-'7'.
	static ArbInteger from_str_base8(std::string_view str);

	bool is_zero() const noexcept { return digits.empty(); }
	bool is_negative() const noexcept { return negative; }

	std::string to_string() const;

	ArbInteger operator-() const;

	ArbInteger operator+(const ArbInteger &other) const;
	ArbInteger operator-(const ArbInteger &other) const;
	ArbInteger operator*(const ArbInteger &other) const;
	/// @brief Truncating division (rounds toward zero)
	ArbInteger operator/(const ArbInteger &other) const;
	/// @brief Remainder with the sign of the dividend (matches operator/ truncating toward zero)
	ArbInteger operator%(const ArbInteger &other) const;

	ArbInteger &operator+=(const ArbInteger &other) { return *this = *this + other; }
	ArbInteger &operator-=(const ArbInteger &other) { return *this = *this - other; }
	ArbInteger &operator*=(const ArbInteger &other) { return *this = *this * other; }
	ArbInteger &operator/=(const ArbInteger &other) { return *this = *this / other; }
	ArbInteger &operator%=(const ArbInteger &other) { return *this = *this % other; }

	bool operator==(const ArbInteger &other) const noexcept;
};
