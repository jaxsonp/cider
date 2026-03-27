#pragma once

#include <format>
#include <iostream>

namespace logging
{
	int global_log_verbosity();
	void set_global_log_verbosity(int val);

	namespace detail
	{
		extern int log_verbosity;
		double timestamp();

		std::string generate_msg_prefix(int verbosity);

	}
}

template <typename... Args>
void log(int verbosity, std::format_string<Args...> msg_fmt, Args &&...msg_args)
{
	if (verbosity > logging::detail::log_verbosity)
		return;

	std::cout << logging::detail::generate_msg_prefix(verbosity) << std::format(msg_fmt, std::forward<Args>(msg_args)...) << std::endl;
}

template <typename... Args>
inline void log(std::format_string<Args...> msg_fmt, Args &&...msg_args)
{
	log(0, msg_fmt, std::forward<Args>(msg_args)...);
}

template <typename... Args>
inline void log_v(std::format_string<Args...> msg_fmt, Args &&...msg_args)
{
	log(1, msg_fmt, std::forward<Args>(msg_args)...);
}

template <typename... Args>
inline void log_vv(std::format_string<Args...> msg_fmt, Args &&...msg_args)
{
	log(2, msg_fmt, std::forward<Args>(msg_args)...);
}

template <typename... Args>
inline void log_vvv(std::format_string<Args...> msg_fmt, Args &&...msg_args)
{
	log(3, msg_fmt, std::forward<Args>(msg_args)...);
}

template <typename... Args>
inline void log_vvvv(std::format_string<Args...> msg_fmt, Args &&...msg_args)
{
	log(4, msg_fmt, std::forward<Args>(msg_args)...);
}