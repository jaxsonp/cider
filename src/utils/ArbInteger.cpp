#include "ArbInteger.hpp"

#include <algorithm>
#include <stdexcept>

void rm_trailing_zeroes(ArbInteger::Digits &digits)
{
	while (!digits.empty() && digits.back() == 0)
		digits.pop_back();
}

ArbInteger::ArbInteger(Digits magnitude, bool is_negative)
	: digits(std::move(magnitude)), negative(is_negative)
{
	rm_trailing_zeroes(this->digits);
	if (this->digits.empty())
		this->negative = false;
}

ArbInteger::ArbInteger(int64_t value)
{
	uint64_t magnitude = value < 0 ? static_cast<uint64_t>(-(value + 1)) + 1 : static_cast<uint64_t>(value);
	*this = ArbInteger(magnitude);
	this->negative = value < 0 && !this->digits.empty();
}

ArbInteger::ArbInteger(uint64_t value)
{
	while (value > 0)
	{
		this->digits.push_back(static_cast<uint32_t>(value % BASE));
		value /= BASE;
	}
}

ArbInteger ArbInteger::from_str_base10(std::string_view str)
{
	if (str.empty())
		throw std::invalid_argument("ArbInteger: empty string is not a valid integer");

	bool is_negative = false;
	size_t start = 0;
	if (str[0] == '+' || str[0] == '-')
	{
		is_negative = str[0] == '-';
		start = 1;
	}

	if (start == str.size())
		throw std::invalid_argument("ArbInteger: no digits in '" + std::string(str) + "'");

	for (size_t i = start; i < str.size(); i++)
	{
		if (str[i] < '0' || str[i] > '9')
			throw std::invalid_argument("ArbInteger: invalid digit in '" + std::string(str) + "'");
	}

	ArbInteger result;
	std::string_view number_digits = str.substr(start);
	for (size_t end = number_digits.size(); end > 0;)
	{
		size_t chunk_len = std::min<size_t>(9, end);
		size_t chunk_start = end - chunk_len;
		uint32_t chunk = 0;
		for (size_t i = chunk_start; i < end; i++)
			chunk = chunk * 10 + static_cast<uint32_t>(number_digits[i] - '0');
		result.digits.push_back(chunk);
		end = chunk_start;
	}

	rm_trailing_zeroes(result.digits);
	result.negative = is_negative && !result.digits.empty();
	return result;
}

ArbInteger ArbInteger::from_str_radix(std::string_view str, uint32_t radix)
{
	if (str.empty())
		throw std::invalid_argument("ArbInteger: empty string is not a valid integer");

	bool is_negative = false;
	size_t start = 0;
	if (str[0] == '+' || str[0] == '-')
	{
		is_negative = str[0] == '-';
		start = 1;
	}

	if (start == str.size())
		throw std::invalid_argument("ArbInteger: no digits in '" + std::string(str) + "'");

	char max_char = static_cast<char>('0' + (radix - 1));

	//
	for (size_t i = start; i < str.size(); i++)
	{
		if (str[i] < '0' || str[i] > max_char)
			throw std::invalid_argument("ArbInteger: invalid digit in '" + std::string(str) + "'");
	}

	// largest chunk_len such that radix^chunk_len <= BASE, so each chunk's value fits a uint32_t digit
	uint32_t chunk_scale = 1;
	size_t chunk_len = 0;
	while (chunk_scale <= BASE / radix)
	{
		chunk_scale *= radix;
		chunk_len++;
	}

	ArbInteger result;
	std::string_view number_digits = str.substr(start);
	for (size_t pos = 0; pos < number_digits.size();)
	{
		size_t len = std::min<size_t>(chunk_len, number_digits.size() - pos);
		uint32_t scale = 1;
		uint32_t chunk_value = 0;
		for (size_t i = 0; i < len; i++)
		{
			chunk_value = chunk_value * radix + static_cast<uint32_t>(number_digits[pos + i] - '0');
			scale *= radix;
		}
		result.digits = mul_magnitude_small(result.digits, scale);
		result.digits = add_magnitude(result.digits, ArbInteger(static_cast<uint64_t>(chunk_value)).digits);
		pos += len;
	}

	rm_trailing_zeroes(result.digits);
	result.negative = is_negative && !result.digits.empty();
	return result;
}

ArbInteger ArbInteger::from_str_base2(std::string_view str)
{
	return from_str_radix(str, 2);
}

ArbInteger ArbInteger::from_str_base8(std::string_view str)
{
	return from_str_radix(str, 8);
}

std::string ArbInteger::to_string() const
{
	if (this->is_zero())
		return "0";

	std::string result = this->negative ? "-" : "";
	result += std::to_string(this->digits.back());
	for (size_t i = this->digits.size() - 1; i-- > 0;)
	{
		std::string chunk = std::to_string(this->digits[i]);
		result += std::string(9 - chunk.size(), '0');
		result += chunk;
	}
	return result;
}

int ArbInteger::compare_magnitude(const Digits &a, const Digits &b)
{
	if (a.size() != b.size())
		return a.size() < b.size() ? -1 : 1;
	for (size_t i = a.size(); i-- > 0;)
	{
		if (a[i] != b[i])
			return a[i] < b[i] ? -1 : 1;
	}
	return 0;
}

ArbInteger::Digits ArbInteger::add_magnitude(const Digits &a, const Digits &b)
{
	Digits result;
	result.resize(std::max(a.size(), b.size()));

	uint64_t carry = 0;
	for (size_t i = 0; i < result.size(); i++)
	{
		uint64_t sum = carry;
		if (i < a.size())
			sum += a[i];
		if (i < b.size())
			sum += b[i];
		result[i] = static_cast<uint32_t>(sum % BASE);
		carry = sum / BASE;
	}
	if (carry > 0)
		result.push_back(static_cast<uint32_t>(carry));

	rm_trailing_zeroes(result);
	return result;
}

// requires compare_magnitude(a, b) >= 0
ArbInteger::Digits ArbInteger::sub_magnitude(const Digits &a, const Digits &b)
{
	Digits result;
	result.resize(a.size());

	int64_t borrow = 0;
	for (size_t i = 0; i < a.size(); i++)
	{
		int64_t diff = static_cast<int64_t>(a[i]) - borrow - (i < b.size() ? static_cast<int64_t>(b[i]) : 0);
		if (diff < 0)
		{
			diff += BASE;
			borrow = 1;
		}
		else
		{
			borrow = 0;
		}
		result[i] = static_cast<uint32_t>(diff);
	}

	rm_trailing_zeroes(result);
	return result;
}

ArbInteger::Digits ArbInteger::mul_magnitude_small(const Digits &a, uint32_t k)
{
	if (k == 0 || a.empty())
		return {};

	Digits result;
	result.resize(a.size());

	uint64_t carry = 0;
	for (size_t i = 0; i < a.size(); i++)
	{
		uint64_t cur = static_cast<uint64_t>(a[i]) * k + carry;
		result[i] = static_cast<uint32_t>(cur % BASE);
		carry = cur / BASE;
	}
	while (carry > 0)
	{
		result.push_back(static_cast<uint32_t>(carry % BASE));
		carry /= BASE;
	}

	rm_trailing_zeroes(result);
	return result;
}

ArbInteger::Digits ArbInteger::mul_magnitude(const Digits &a, const Digits &b)
{
	if (a.empty() || b.empty())
		return {};

	Digits result;
	result.resize(a.size() + b.size(), 0);

	for (size_t i = 0; i < a.size(); i++)
	{
		uint64_t carry = 0;
		for (size_t j = 0; j < b.size(); j++)
		{
			uint64_t cur = result[i + j] + static_cast<uint64_t>(a[i]) * b[j] + carry;
			result[i + j] = static_cast<uint32_t>(cur % BASE);
			carry = cur / BASE;
		}
		size_t k = i + b.size();
		while (carry > 0)
		{
			uint64_t cur = result[k] + carry;
			result[k] = static_cast<uint32_t>(cur % BASE);
			carry = cur / BASE;
			k++;
		}
	}

	rm_trailing_zeroes(result);
	return result;
}

void ArbInteger::divmod_magnitude(const Digits &a, const Digits &b, Digits &quotient, Digits &remainder)
{
	if (b.empty())
		throw std::domain_error("ArbInteger: division by zero");

	quotient.assign(a.size(), 0);
	remainder.clear();

	for (size_t i = a.size(); i-- > 0;)
	{
		remainder.insert(remainder.begin(), a[i]);
		rm_trailing_zeroes(remainder);

		uint32_t lo = 0, hi = BASE - 1;
		while (lo < hi)
		{
			uint32_t mid = lo + (hi - lo + 1) / 2;
			if (compare_magnitude(mul_magnitude_small(b, mid), remainder) <= 0)
				lo = mid;
			else
				hi = mid - 1;
		}

		quotient[i] = lo;
		remainder = sub_magnitude(remainder, mul_magnitude_small(b, lo));
	}

	rm_trailing_zeroes(quotient);
}

ArbInteger ArbInteger::operator-() const
{
	return ArbInteger(this->digits, !this->negative);
}

ArbInteger ArbInteger::operator+(const ArbInteger &other) const
{
	if (this->negative == other.negative)
		return ArbInteger(add_magnitude(this->digits, other.digits), this->negative);

	int cmp = compare_magnitude(this->digits, other.digits);
	if (cmp == 0)
		return ArbInteger();
	if (cmp > 0)
		return ArbInteger(sub_magnitude(this->digits, other.digits), this->negative);
	return ArbInteger(sub_magnitude(other.digits, this->digits), other.negative);
}

ArbInteger ArbInteger::operator-(const ArbInteger &other) const
{
	return *this + (-other);
}

ArbInteger ArbInteger::operator*(const ArbInteger &other) const
{
	return ArbInteger(mul_magnitude(this->digits, other.digits), this->negative != other.negative);
}

ArbInteger ArbInteger::operator/(const ArbInteger &other) const
{
	Digits quotient, remainder;
	divmod_magnitude(this->digits, other.digits, quotient, remainder);
	return ArbInteger(std::move(quotient), this->negative != other.negative);
}

ArbInteger ArbInteger::operator%(const ArbInteger &other) const
{
	Digits quotient, remainder;
	divmod_magnitude(this->digits, other.digits, quotient, remainder);
	return ArbInteger(std::move(remainder), this->negative);
}

bool ArbInteger::operator==(const ArbInteger &other) const noexcept
{
	return this->negative == other.negative && this->digits == other.digits;
}
