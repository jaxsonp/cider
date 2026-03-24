#pragma once

#include <format>
#include <iostream>

#include "settings.hpp"

namespace logging::detail
{
	extern double init_time;
	double timestamp();

	std::string generate_msg_prefix(unsigned short verbosity);
}

template <typename... Args>
void log(unsigned short verbosity, std::format_string<Args...> msg_fmt, Args &&...msg_args)
{
	if (verbosity > settings::log_verbosity)
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