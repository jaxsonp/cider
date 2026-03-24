#include "logging.hpp"

#include <chrono>

namespace logging::detail
{
	double init_time = timestamp();

	double timestamp()
	{
		std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
		long long ms_since_epoch = duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
		return ((double)ms_since_epoch) / 1000.0;
	}
	std::string generate_msg_prefix(unsigned short verbosity)
	{
		double rel_time = logging::detail::timestamp() - logging::detail::init_time;
		return std::format("\x1b[90m[{: >6.3f}]\x1b[0m {}", rel_time, std::string(verbosity * 2, ' '));
	}
}