#include <chrono>
#include <format>
#include <iostream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstdlib>

#include "testing.hpp"
#include "e2e_testing.hpp"
#include "unit_testing.hpp"
#include "utils/CliParser.hpp"
#include "utils/error.hpp"

#define MAX_WORKERS 255

using namespace testing;

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <io.h>
static unsigned int pid = (unsigned int)GetCurrentProcessId();
#else
#include <unistd.h>
static unsigned int pid = (unsigned int)getpid();
#endif

/// Whether stdout/stderr should be colored, set once from --color before any worker starts
static bool use_color = false;

// ANSI COLOR codes
static constexpr std::string_view RED = "31";
static constexpr std::string_view GREEN = "32";
static constexpr std::string_view YELLOW = "33";
static constexpr std::string_view MAGENTA = "35";
static constexpr std::string_view CYAN = "36";

/// Wraps `text` in the given color code *if* color is enabled
static std::string colored(std::string_view text, std::string_view code)
{
	if (!use_color)
		return std::string(text);
	return std::format("\x1b[{}m{}\x1b[0m", code, text);
}

/// The common bullet point in front of the runner's status lines
static std::string bullet()
{
	return colored(" ~ ", MAGENTA);
}

/// Worker thread pull tests from this
static std::vector<const Test *> test_queue;
/// Threads must hold this when reading/writing to the test_queue list
static std::mutex test_queue_mtx;
/// Threads must hold this when writing to stdout/stderr
static std::mutex printing_mtx;

static std::atomic<unsigned int> tests_passed = 0;
static std::atomic<unsigned int> tests_failed = 0;
static std::atomic<unsigned int> tests_skipped = 0;

void test_runner_thread(std::filesystem::path &, bool);
bool stdout_supports_color();

int main(int argc, char **argv)
{
	CliParser cli("ciderc_test_runner", "Runs Cider's unit and e2e tests");
	auto &unit_arg = cli.add_flag("unit", "Run unit tests (if no category is given, all are run)");
	auto &e2e_arg = cli.add_flag("e2e", "Run end to end tests");
	auto &list_arg = cli.add_flag("list", "List the selected tests instead of running them").short_name('l');
	auto &verbose_arg = cli.add_flag("verbose", "Print every test, not just failures").short_name('v');
	auto &noclean_arg = cli.add_flag("noclean", "Don't remove testing artifacts, even if successful");

	auto &n_workers_arg = cli.add_flag_arg("workers", "Number of parallel test runners").short_name('n').default_value("4").metavar("N");
	auto &ciderc_arg = cli.add_flag_arg("ciderc", "Ciderc executable used for e2e tests. Defaults to './ciderc' if it exists, else 'ciderc'").short_name('C').metavar("path");
	auto &target_arg = cli.add_flag_arg("target", "Target to compile e2e tests for. Defaults to the host").short_name('t').metavar("name");
	auto &e2e_prefix_arg = cli.add_flag_arg("e2e-prefix", "Command to run e2e test programs through, ie an emulator for other targets").metavar("cmd");
	auto &artifacts_arg = cli.add_flag_arg("artifacts", "Output directory for testing artifacts").short_name('A').default_value("./.ciderc_testing/").metavar("path");
	auto &color_arg = cli.add_flag_arg("color", "Color output: 'auto' (if stdout is a terminal that supports it), 'always' or 'never'").default_value("auto").metavar("when");
	cli.add_help_flag();

	try
	{
		cli.parse(argc, argv);
	}
	catch (const CliHelpRequested &)
	{
		cli.print_help();
		return 0;
	}
	catch (const CliError &e)
	{
		std::cerr << "CLI Error: " << e.what() << "\n";
		return 2;
	}

	if (color_arg.value() == "always")
		use_color = true;
	else if (color_arg.value() == "never")
		use_color = false;
	else if (color_arg.value() == "auto")
		use_color = stdout_supports_color();
	else
	{
		std::cerr << "CLI Error: invalid --color value '" << color_arg.value() << "', expected 'auto', 'always' or 'never'\n";
		return 2;
	}

	bool do_unit_tests = unit_arg.present();
	bool do_e2e_tests = e2e_arg.present();
	if (!(do_unit_tests || do_e2e_tests))
	{
		do_unit_tests = true;
		do_e2e_tests = true;
	}

	if (do_unit_tests)
	{
		auto &unit_tests = unit::get_unit_test_registry();
		std::cout << bullet() << "Preparing " << unit_tests.size() << " unit tests" << std::endl;
		test_queue.reserve(unit_tests.size());
		for (const unit::UnitTest &test : unit_tests)
			test_queue.push_back(&test);
	}

	if (do_e2e_tests)
	{
		std::cout << bullet() << "Preparing " << e2e::e2e_tests.size() << " end-to-end tests" << std::endl;
		test_queue.reserve(e2e::e2e_tests.size());
		for (const e2e::E2eTest &test : e2e::e2e_tests)
			test_queue.push_back(&test);
	}

	if (list_arg)
	{
		for (const Test *test : test_queue)
			std::cout << std::format("[{}] {}\n", to_string(test->category), test->name);

		std::cout << test_queue.size() << " tests total" << std::endl;
		return 0;
	}

	if (do_e2e_tests)
	{
		try
		{
			e2e::E2eTest::set_target(target_arg.value());
		}
		catch (const CompilerError &e)
		{
			std::cerr << e.what() << std::endl;
			return 2;
		}
		e2e::E2eTest::set_run_prefix(e2e_prefix_arg.value());
		if (target_arg.present())
			std::cout << bullet() << "Compiling e2e tests for " << target_arg.value() << std::endl;
		if (!e2e_prefix_arg.value().empty())
			std::cout << bullet() << "Running e2e test programs through: " << e2e_prefix_arg.value() << std::endl;

		// prepare ciderc executable path
		if (ciderc_arg.maybe_value().has_value())
		{
			std::cout << bullet() << "Using provided ciderc:\n"
					  << ciderc_arg.value() << std::endl;
			e2e::E2eTest::set_ciderc_path(ciderc_arg.value());
		}
		else
		{
			// no ciderc path provided, use "./ciderc" if it exists
			std::filesystem::path default_path = std::filesystem::current_path() / "ciderc";
			if (std::filesystem::exists(default_path))
			{
				std::cout << bullet() << "Using local ciderc:\n"
						  << default_path.string() << std::endl;
				e2e::E2eTest::set_ciderc_path(default_path);
			}
			else
			{
				std::cout << bullet() << "Using ciderc from path resolution" << std::endl;
			}
		}
		std::string version_info = e2e::E2eTest::check_ciderc_version();
		std::cout << bullet() << "Compiler version info:\n"
				  << version_info << std::flush;
	}

	// artifacts directory
	auto init_time = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
	std::string artifacts_subdir_name = std::format("{:%Y%m%d%H%M%S}-{:x}", init_time, pid);
	std::filesystem::path artifacts_dir = std::filesystem::path(artifacts_arg.value()) / artifacts_subdir_name;
	bool preexisted = !std::filesystem::create_directories(artifacts_dir);
	if (preexisted)
		std::filesystem::remove_all(artifacts_dir);

	// get workers
	unsigned int n_workers;
	try
	{
		n_workers = std::stoul(n_workers_arg.value());
	}
	catch (const std::invalid_argument &)
	{
		std::cerr << "Invalid worker count: " << n_workers_arg.value() << std::endl;
		return 1;
	}
	if (n_workers > MAX_WORKERS)
		n_workers = MAX_WORKERS;

	std::vector<std::thread> worker_threads;
	worker_threads.reserve(n_workers);

	std::cout << bullet() << "Test execution starting (" << n_workers << " threads)" << std::endl;
	auto start = std::chrono::steady_clock::now();
	for (unsigned int i = 0; i < n_workers; i++)
	{
		worker_threads.push_back(std::thread(test_runner_thread, std::ref(artifacts_dir), (bool)verbose_arg));
	}

	for (auto &t : worker_threads)
	{
		t.join();
	}
	float elapsed_secs = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();

	unsigned int passed = tests_passed;
	unsigned int failed = tests_failed;
	unsigned int skipped = tests_skipped;
	unsigned int tests_ran = passed + failed + skipped;
	bool no_fails = failed == 0;

	std::cout << std::format("{}Test execution completed: {} tests in {:.2f}s\n", bullet(), tests_ran, elapsed_secs);

	if (noclean_arg || !no_fails)
	{
		std::cout << bullet() << "Leaving testing artifacts at:\n"
				  << artifacts_dir.string() << "\n";
	}
	else
	{
		std::cout << bullet() << "Cleaning up artifacts" << std::endl;
		std::filesystem::remove_all(artifacts_dir);
	}

	float pass_percent = (passed + failed > 0) ? 100.0f * ((float)passed) / (float)(passed + failed) : 100.0f;
	// counts are only colored if nonzero
	auto colored_count = [](unsigned int n, std::string_view code)
	{
		return n != 0 ? colored(std::to_string(n), code) : std::to_string(n);
	};
	std::cout << std::format("\n{} passed, {} failed, {} skipped ({:.1f}%)\n", colored_count(passed, GREEN), colored_count(failed, RED), skipped, pass_percent);

	if (no_fails)
		std::cout << colored("All tests passed\n", GREEN);

	std::cout.flush();

	return no_fails ? 0 : 1;
}

void test_runner_thread(std::filesystem::path &artifacts_dir, bool verbose)
{
	const Test *test;
	while (true)
	{
		// fetch a test
		{
			std::scoped_lock test_queue_lock(test_queue_mtx);
			if (test_queue.empty())
				return; // done, no more tests
			test = test_queue.back();
			test_queue.pop_back();
		}

		std::string label = test->label();

		// run the test
		Result result;
		try
		{
			result = test->run(artifacts_dir);
		}
		catch (const std::exception &e)
		{
			result = Result(Outcome::Fail, std::format("Uncaught exception thrown: {}", e.what()));
		}
		catch (...)
		{
			result = Result(Outcome::Fail, "Unknown/non-standard exception thrown");
		}

		// record results
		switch (result.outcome)
		{
		case Outcome::Pass:
			tests_passed++;
			if (verbose)
			{
				std::scoped_lock printing_lock(printing_mtx);
				std::cout << colored("PASS", GREEN) << " " << label << "\n";
			}
			break;
		case Outcome::Skip:
			tests_skipped++;
			if (verbose)
			{
				std::scoped_lock printing_lock(printing_mtx);
				std::cout << std::format("{} {} ({})\n", colored("SKIP", YELLOW), label, result.message);
			}
			break;
		case Outcome::Fail:
			tests_failed++;
			{
				std::scoped_lock printing_lock(printing_mtx);
				std::cout << std::format("{} {}\n  {}\n", colored("FAIL", RED), label, result.message);
			}
			break;
		}
	}
}

/// Detects whether stdout is a terminal that can display ANSI colors. Honors the
/// NO_COLOR convention (https://no-color.org) and TERM=dumb
bool stdout_supports_color()
{
	const char *no_color = std::getenv("NO_COLOR");
	if (no_color != nullptr && no_color[0] != '\0')
		return false;

#if defined(_WIN32) || defined(_WIN64)
	if (!_isatty(_fileno(stdout)))
		return false;
	// older consoles only understand ANSI escapes once virtual terminal processing is on
	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD mode;
	if (handle == INVALID_HANDLE_VALUE || !GetConsoleMode(handle, &mode))
		return false;
	return SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
	if (!isatty(STDOUT_FILENO))
		return false;
	const char *term = std::getenv("TERM");
	return term != nullptr && std::string_view(term) != "dumb";
#endif
}
