#pragma once

#include <string>
#include <string_view>
#include <filesystem>

namespace testing
{
	enum class Outcome
	{
		Pass,
		Fail,
		Skip,
	};

	struct Result
	{
		Outcome outcome;
		/// Failure message or skip reason
		std::string message;
	};

	/// @brief Abstract class for defining and running tests
	class Test
	{
	public:
		enum class Category
		{
			Unit,
			EndToEnd,
		};

		const std::string name;
		const Category category;

		Test(std::string_view name, Category category);
		virtual ~Test() = default;

		virtual Result run(std::filesystem::path &artifacts_dir) const = 0;

		std::string label() const;
	};

	std::string_view to_string(Test::Category category);
}
