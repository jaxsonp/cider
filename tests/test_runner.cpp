#include <chrono>
#include <format>
#include <iostream>

#include "testing.hpp"
#include "utils/CliParser.hpp"

namespace
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

	Result run_test(const Test &test)
	{
		try
		{
			test.fn();
			return {Outcome::Pass, ""};
		}
		catch (const TestFailure &e)
		{
			return {Outcome::Fail, e.what()};
		}
		catch (const TestSkipped &e)
		{
			return {Outcome::Skip, e.what()};
		}
		catch (const std::exception &e)
		{
			return {Outcome::Fail, std::format("uncaught exception: {}", e.what())};
		}
		catch (...)
		{
			return {Outcome::Fail, "uncaught non-std exception"};
		}
	}
}

int main(int argc, char **argv)
{
	CliParser cli("test_runner", "Runs Cider's unit and e2e tests");
	cli.add_help_flag();
	auto &unit_arg = cli.add_flag("unit", "Run unit tests (if no category is given, all are run)");
	auto &e2e_arg = cli.add_flag("e2e", "Run end to end tests");
	auto &list_arg = cli.add_flag("list", "List the selected tests instead of running them").short_name('l');
	auto &verbose_arg = cli.add_flag("verbose", "Print every test, not just failures").short_name('v');

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
		std::cerr << "Error: " << e.what() << "\n";
		return 2;
	}

	std::vector<const Test *> selected_tests;
	for (const Test &test : get_test_registry())
	{
		if ((!unit_arg.present() && !e2e_arg.present()) ||
			test.category == TestCategory::Unit && unit_arg.present() ||
			test.category == TestCategory::End2End && e2e_arg.present())
			selected_tests.push_back(&test);
	}

	if (list_arg)
	{
		for (const Test *test : selected_tests)
			std::cout << std::format("[{}] {}\n", to_string(test->category), test->full_name());
		return 0;
	}

	int passed = 0, failed = 0, skipped = 0;
	std::vector<std::string> failures;
	auto start = std::chrono::steady_clock::now();
	for (const Test *test : selected_tests)
	{
		Result result = run_test(*test);
		std::string label = std::format("[{}] {}", to_string(test->category), test->full_name());
		switch (result.outcome)
		{
		case Outcome::Pass:
			passed++;
			if (verbose_arg)
				std::cout << "PASS " << label << "\n";
			break;
		case Outcome::Skip:
			skipped++;
			if (verbose_arg)
				std::cout << std::format("SKIP {} ({})\n", label, result.message);
			break;
		case Outcome::Fail:
			failed++;
			std::cout << std::format("FAIL {}\n  {}\n", label, result.message);
			failures.push_back(label);
			break;
		}
	}
	auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start);

	if (!failures.empty())
	{
		std::cout << "\nFailed tests:\n";
		for (const std::string &f : failures)
			std::cout << "  " << f << "\n";
	}
	std::cout << std::format("\n{} passed, {} failed, {} skipped ({:.2f}s)\n", passed, failed, skipped, elapsed.count());
	return failed == 0 ? 0 : 1;
}
