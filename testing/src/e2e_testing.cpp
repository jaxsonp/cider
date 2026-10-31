#include "e2e_testing.hpp"

#include <stdexcept>
#include <regex>
#include <string>
#include <fstream>
#include <sstream>
#ifndef _WIN32
#include <sys/wait.h>
#endif

#include "cpp-subprocess/subprocess.hpp"

#include "utils/logging.hpp"
#include "compile.hpp"

/// Waits for a process to exit, returning its exit code, or the negated signal number if it was killed by one
static int wait_for_exit(subprocess::Popen &proc)
{
#ifdef _WIN32
	return proc.wait();
#else
	// on posix, .wait() returns the signum if the process was killed by a signal, so lets wait for it manually
	int status = 0;
	if (waitpid(proc.pid(), &status, 0) == -1)
		throw std::runtime_error("waitpid failed");
	return WIFSIGNALED(status) ? -WTERMSIG(status) : WEXITSTATUS(status);
#endif
}

namespace testing::e2e
{

	std::string E2eTest::ciderc_executable = "ciderc";

	constexpr std::string_view make_test_name(std::string_view file_path)
	{
		if (!file_path.ends_with(".cdr"))
			throw std::logic_error("e2e test files must end with .cdr");
		return file_path.substr(0, file_path.size() - 4);
	}

	E2eTest::E2eTest(std::string_view file_path, const char *file_contents)
		: Test(make_test_name(file_path), Test::Category::EndToEnd), source(file_contents)
	{
		static const std::regex test_parameter_pattern("^//!\\s*([^=]+)=(.*)$");

		std::string line;
		std::istringstream source_stream(source);
		while (std::getline(source_stream, line))
		{
			std::smatch match_results;
			if (!std::regex_match(line, match_results, test_parameter_pattern))
				break;

			std::string key = match_results[1].str();
			std::string value = match_results[2].str();
			if (key == "BUILD_EXIT_CODE")
				this->expected_build_exit_code = std::stoi(value);
			else if (key == "EXIT_CODE")
				this->expected_exit_code = std::stoi(value);
			else if (key == "STDOUT")
				this->expected_stdout = value;
			else if (key == "STDERR")
				this->expected_stderr = value;
			else if (key == "REQUIRES")
			{
				// TODO: bit_requirement is stored, but run() doesn't check it yet
				if (value == "32bit")
					this->bit_requirement = BitRequirement::NEEDS_32_BIT;
				else if (value == "64bit")
					this->bit_requirement = BitRequirement::NEEDS_64_BIT;
				else
					std::cerr << "WARNING: Unrecognized 'REQUIRES' keyword: " << value << std::endl;
			}
			else
				throw std::logic_error(std::format("Unrecognized e2e test parameter: {}", line));
		}
	}

	Result E2eTest::run(std::filesystem::path &artifacts_dir) const
	{
		// set up directory structure
		std::filesystem::path output_dir = artifacts_dir / "e2e" / this->name;
		std::filesystem::create_directories(output_dir);

		std::filesystem::path bin_path = output_dir / "program";
		std::filesystem::path src_path = output_dir / "source.cdr";
		std::filesystem::path build_stdout_path = output_dir / "build_stdout.txt";
		std::filesystem::path build_stderr_path = output_dir / "build_stderr.txt";
		std::filesystem::path run_stdout_path = output_dir / "stdout.txt";
		std::filesystem::path run_stderr_path = output_dir / "stderr.txt";

		// create source file
		std::ofstream src_file(src_path);
		if (!src_file.is_open())
			return Result(Outcome::Fail, std::format("failed to open file to write: {}", src_path.string()));
		src_file << this->source;
		src_file.close();

		// start compilation
		std::string build_cmd = std::format("{} --emit exe --out {} -vv {}", E2eTest::ciderc_executable, bin_path.string(), src_path.string());
		subprocess::Popen build_proc(
			build_cmd,
			subprocess::output{build_stdout_path.string().c_str()},
			subprocess::error{build_stderr_path.string().c_str()},
			subprocess::defer_spawn{true});
		int build_exit_code;
		try
		{
			build_proc.start_process();
			build_exit_code = wait_for_exit(build_proc);
		}
		catch (const subprocess::CalledProcessError &err)
		{
			return Result(Outcome::Fail, std::format("failed to run build command \"{}\": {}", build_cmd, err.what()));
		}

		// check return code
		if (build_exit_code != this->expected_build_exit_code)
			return Result(Outcome::Fail, std::format("build exited with {}, expected {}", build_exit_code, this->expected_build_exit_code));

		// pass if build failure was expected
		if (this->expected_build_exit_code != 0)
			return Result(Outcome::Pass, "");

		// run the produced executable
		std::string run_cmd = bin_path.string();
		subprocess::Popen run_proc(
			run_cmd,
			subprocess::output{run_stdout_path.string().c_str()},
			subprocess::error{run_stderr_path.string().c_str()},
			subprocess::defer_spawn{true});
		int run_exit_code;
		try
		{
			run_proc.start_process();
			run_exit_code = wait_for_exit(run_proc);
		}
		catch (const subprocess::CalledProcessError &err)
		{
			return Result(Outcome::Fail, std::format("failed to run program \"{}\": {}", run_cmd, err.what()));
		}

		// check return code
		if (run_exit_code != this->expected_exit_code)
			return Result(Outcome::Fail, std::format("program exited with {}, expected {}", run_exit_code, this->expected_exit_code));

		// check stdout/stderr
		std::ifstream stdout_file(run_stdout_path);
		if (!stdout_file.is_open())
			return Result(Outcome::Fail, std::format("failed to open file to read: {}", run_stdout_path.string()));
		std::string stdout_contents((std::istreambuf_iterator<char>(stdout_file)), std::istreambuf_iterator<char>());
		stdout_file.close();
		if (stdout_contents != this->expected_stdout)
			return Result(Outcome::Fail, "program stdout did not match expected stdout");
		std::ifstream stderr_file(run_stderr_path);
		if (!stderr_file.is_open())
			return Result(Outcome::Fail, std::format("failed to open file to read: {}", run_stderr_path.string()));
		std::string stderr_contents((std::istreambuf_iterator<char>(stderr_file)), std::istreambuf_iterator<char>());
		stderr_file.close();
		if (stderr_contents != this->expected_stderr)
			return Result(Outcome::Fail, "program stderr did not match expected stderr");

		// yay
		return Result(Outcome::Pass, "");
	}

	void E2eTest::set_ciderc_path(std::filesystem::path path)
	{
		if (!std::filesystem::exists(path))
			throw std::runtime_error(std::format("ciderc path \"{}\" does not exist", path.string()));

		if (!std::filesystem::is_regular_file(path) && !std::filesystem::is_symlink(path))
			throw std::runtime_error(std::format("ciderc path \"{}\" is not a regular file", path.string()));

		E2eTest::ciderc_executable = path.string();
	}

	std::string E2eTest::check_ciderc_version()
	{
		try
		{
			auto proc = subprocess::Popen(
				{E2eTest::ciderc_executable, "--version"},
				subprocess::output{subprocess::PIPE},
				subprocess::error{subprocess::PIPE});
			subprocess::OutBuffer proc_out = proc.communicate().first;
			return proc_out.buf.data();
		}
		catch (const subprocess::CalledProcessError &)
		{
			throw std::runtime_error(std::format("Error with \"{}\"", E2eTest::ciderc_executable));
		}
	}
}