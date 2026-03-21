#pragma once

#include <format>
#include <chrono>
#include <iostream>

#include "settings.hpp"

template <typename... Args>
void log(unsigned short verbosity, std::format_string<Args...> msg_fmt, Args &&...msg_args)
{
	if (verbosity > settings::log_verbosity)
		return;

	auto now = std::chrono::system_clock::now();
	auto ms_since_epoch = duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

	std::cout << std::format("{} {}", ((float)ms_since_epoch) / 1000.0, std::string(verbosity * 2, ' '))
			  << std::format(msg_fmt, std::forward<Args>(msg_args)...)
			  << std::endl;
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