#include "e2e_testing.hpp"

#include <stdexcept>
#include <regex>
#include <chrono>
#include <string>
#include <iostream>

#include "cpp-subprocess/subprocess.hpp"

#include "utils/logging.hpp"
#include "compile.hpp"

namespace testing::e2e
{
	std::string E2eTest::ciderc_executable = "ciderc";

	std::filesystem::path &ensure_test_dir()
	{
		static auto init_time = std::chrono::system_clock::now();
		static std::string init_timestamp = std::format("{:%Y%m%d%H%M%S}", init_time);
		static std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "cider-testing" / init_timestamp;
		// we don't care if it created or not, only that it now exists
		bool _ = std::filesystem::create_directories(test_dir);
		return test_dir;
	}

	constexpr std::string_view make_test_name(std::string_view file_path)
	{
		if (!file_path.ends_with(".cdr"))
			throw std::logic_error("e2e test files must end with .cdr");
		return file_path.substr(0, file_path.size() - 4);
	}

	E2eTest::E2eTest(std::string_view file_path, const char *file_contents)
		: Test(make_test_name(file_path), Test::Category::EndToEnd), source(file_contents) {}

	Result E2eTest::run() const
	{
		static const std::regex test_parameter_pattern("^//!\\s*([^=]+)=(.*)");

		// set up directory structure
		std::filesystem::path test_dir = ensure_test_dir();
		std::filesystem::path output_dir = test_dir / this->name;
		if (std::filesystem::exists(output_dir))
			std::filesystem::remove_all(output_dir);
		std::filesystem::create_directories(output_dir);

		std::filesystem::path bin_path = output_dir / "out";
		std::filesystem::path build_stdout_path = output_dir / "build_stdout.txt";
		std::filesystem::path build_stderr_path = output_dir / "build_stderr.txt";
		std::filesystem::path run_stdout_path = output_dir / "stdout.txt";
		std::filesystem::path run_stderr_path = output_dir / "stderr.txt";

		std::vector<std::string> build_cmd{E2eTest::ciderc_executable};

		auto proc = subprocess::Popen(
			build_cmd,
			subprocess::output{"cat_fredirect.txt"},
			subprocess::error{"cat_fredirect.txt"});

		return Result(Outcome::Pass, "todo");
	}

	void E2eTest::set_ciderc_path(std::filesystem::path path)
	{
		if (!std::filesystem::exists(path))
			throw std::runtime_error(std::format("ciderc path \"{}\" does not exist", path.string()));

		E2eTest::ciderc_executable = path.string();
		std::cout << "Ciderc executable: " << E2eTest::ciderc_executable << std::endl;

		// TODO get version/test executable
	}
}