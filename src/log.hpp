#ifndef LOG_HPP
#define LOG_HPP

#include <memory>
#include <iostream>
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>

extern std::shared_ptr<spdlog::logger> console;

inline void log_init()
{
	spdlog::set_pattern("[%n] [%L] [thread %t] %v");
	console->set_level(spdlog::level::trace);
	console->set_error_handler([](const std::string& msg) {
		std::cerr << msg << std::endl;
		std::abort();
	});

	console->info("Hello World!");
}

#endif
