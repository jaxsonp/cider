#include "source_testing.hpp"

#include <sstream>

#include "frontend/AST.hpp"
#include "ir/IrPrinter.hpp"
#include "utils/error.hpp"

namespace testing::unit
{
	ExitCode check_source(const std::string &source)
	{
		std::istringstream input(source);
		try
		{
			AST ast(input);
			return ExitCode::Success;
		}
		catch (const CompilerError &err)
		{
			return err.exit_code();
		}
		catch (const std::exception &)
		{
			return ExitCode::UncaughtInternalError;
		}
	}

	std::string source_to_ir(const std::string &source)
	{
		std::istringstream input(source);
		AST ast(input);
		ir::Object obj = ast.emit_ir();

		std::ostringstream out;
		ir::print(obj, out);
		return out.str();
	}

	std::string_view exit_code_name(ExitCode code)
	{
		switch (code)
		{
		case ExitCode::Success:
			return "Success";
		case ExitCode::SyntaxError:
			return "SyntaxError";
		case ExitCode::NameError:
			return "NameError";
		case ExitCode::TypeError:
			return "TypeError";
		case ExitCode::SemanticError:
			return "SemanticError";
		case ExitCode::Unsupported:
			return "Unsupported";
		case ExitCode::FileIoError:
			return "FileIoError";
		case ExitCode::UsageError:
			return "UsageError";
		case ExitCode::Unimplemented:
			return "Unimplemented";
		case ExitCode::InternalError:
			return "InternalError";
		case ExitCode::UncaughtInternalError:
			return "UncaughtInternalError";
		}
		return "<unknown>";
	}
}
