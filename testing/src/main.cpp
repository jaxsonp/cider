#include <chrono>
#include <format>
#include <iostream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <atomic>

#include "testing.hpp"
#include "e2e_testing.hpp"
#include "unit_testing.hpp"
#include "utils/CliParser.hpp"

#define MAX_WORKERS 255

using namespace testing;

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
static unsigned int pid = (unsigned int)GetCurrentProcessId();
#else
#include <unistd.h>
static unsigned int pid = (unsigned int)getpid();
#endif

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

int main(int argc, char **argv)
{
	CliParser cli("ciderc_test_runner", "Runs Cider's unit and e2e tests");
	auto &unit_arg = cli.add_flag("unit", "Run unit tests (if no category is given, all are run)");
	auto &e2e_arg = cli.add_flag("e2e", "Run end to end tests");
	auto &list_arg = cli.add_flag("list", "List the selected tests instead of running them").short_name('l');
	auto &verbose_arg = cli.add_flag("verbose", "Print every test, not just failures").short_name('v');
	auto &ciderc_arg = cli.add_flag_arg("ciderc", "Ciderc executable used for e2e tests. Defaults to './ciderc' if it exists, else 'ciderc'").short_name('C').metavar("path");
	auto &artifacts_arg = cli.add_flag_arg("artifacts", "Output directory for testing artifacts").short_name('A').default_value("./.ciderc_testing/").metavar("path");
	auto &n_workers_arg = cli.add_flag_arg("workers", "Number of parallel test runners").short_name('n').default_value("4").metavar("N");
	auto &noclean_arg = cli.add_flag("noclean", "Don't remove testing artifacts, even if successful");
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
		std::cout << " ~ Preparing " << unit_tests.size() << " unit tests" << std::endl;
		test_queue.reserve(unit_tests.size());
		for (const unit::UnitTest &test : unit_tests)
			test_queue.push_back(&test);
	}

	if (do_e2e_tests)
	{
		std::cout << " ~ Preparing " << e2e::e2e_tests.size() << " end-to-end tests" << std::endl;
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
		// prepare ciderc executable path
		if (ciderc_arg.maybe_value().has_value())
		{
			std::cout << " ~ Using provided ciderc:\n"
					  << ciderc_arg.value() << std::endl;
			e2e::E2eTest::set_ciderc_path(ciderc_arg.value());
		}
		else
		{
			// no ciderc path provided, use "./ciderc" if it exists
			std::filesystem::path default_path = std::filesystem::current_path() / "ciderc";
			if (std::filesystem::exists(default_path))
			{
				std::cout << " ~ Using local ciderc:\n"
						  << default_path.string() << std::endl;
				e2e::E2eTest::set_ciderc_path(default_path);
			}
			else
			{
				std::cout << " ~ Using ciderc from path resolution" << std::endl;
			}
		}
		std::string version_info = e2e::E2eTest::check_ciderc_version();
		std::cout << " ~ Compiler version info:\n"
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

	std::cout << " ~ Test execution starting (" << n_workers << " threads)" << std::endl;
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

	std::cout << std::format(" ~ Test execution completed: {} tests in {:.2f}s\n", tests_ran, elapsed_secs);

	if (noclean_arg || failed != 0)
	{
		std::cout << " ~ Leaving testing artifacts at:\n"
				  << artifacts_dir.string() << "\n";
	}
	else
	{
		std::cout << " ~ Cleaning up artifacts" << std::endl;
		std::filesystem::remove_all(artifacts_dir);
	}

	float pass_rate = (passed + failed > 0) ? ((float)passed) / (float)(passed + failed) : 1.0f;
	std::cout << std::format("\n{} passed, {} failed, {} skipped ({:.1f}%)\n", passed, failed, skipped, pass_rate * 100.0f);
	std::cout.flush();
	return failed == 0 ? 0 : 1;
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
				std::cout << "PASS " << label << "\n";
			}
			break;
		case Outcome::Skip:
			tests_skipped++;
			if (verbose)
			{
				std::scoped_lock printing_lock(printing_mtx);
				std::cout << std::format("SKIP {} ({})\n", label, result.message);
			}
			break;
		case Outcome::Fail:
			tests_failed++;
			{
				std::scoped_lock printing_lock(printing_mtx);
				std::cout << std::format("FAIL {}\n  {}\n", label, result.message);
			}
			break;
		}
	}
}