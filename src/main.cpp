#include "Tracer.h"
#include <cxxopts.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <vector>

void setup_logger()
{
    try
    {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);
        auto file_sink =
            std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/trace.log", true);
        file_sink->set_level(spdlog::level::trace);
        spdlog::sinks_init_list sink_list = {console_sink, file_sink};
        auto logger = std::make_shared<spdlog::logger>("tracer", sink_list);
        logger->set_level(spdlog::level::trace);
        spdlog::set_default_logger(logger);
        spdlog::flush_on(spdlog::level::info);
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        std::cerr << "Log init failed: " << ex.what() << std::endl;
    }
}

int main(int argc, char *argv[])
{
    setup_logger();

    cxxopts::Options options("trace", "A C++ syscall tracer using ptrace");
    options.add_options()("a,attach", "PID to attach to", cxxopts::value<pid_t>())(
        "f,fork", "Program to fork and trace",
        cxxopts::value<std::vector<std::string>>())("h,help", "Print usage");

    options.parse_positional({"fork"});
    options.positional_help("<program> [args...]");

    try
    {
        auto result = options.parse(argc, argv);

        if (result.count("help") || argc == 1)
        {
            std::cout << options.help() << std::endl;
            return 0;
        }

        if (result.count("attach"))
        {
            pid_t pid = result["attach"].as<pid_t>();
            spdlog::info("Attempting to attach to PID: {}", pid);

            if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) == -1)
            {
                spdlog::critical("Failed to attach to PID {}: {}", pid, strerror(errno));
                return 1;
            }
            waitpid(pid, nullptr, 0);
            ptrace(PTRACE_SETOPTIONS, pid, nullptr, PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACECLONE);
            ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr);
            Tracer tracer(pid);
            tracer.run();
        }
        else if (result.count("fork"))
        {
            const auto &fork_args = result["fork"].as<std::vector<std::string>>();
            fork_and_trace(fork_args);
        }
        else
        {
            std::cout << options.help() << std::endl;
        }
    }
    catch (const cxxopts::exceptions::exception &e)
    {
        spdlog::error("Error parsing options: {}", e.what());
        return 1;
    }
    catch (const std::exception &e)
    {
        spdlog::error("A critical error occurred: {}", e.what());
        return 1;
    }
    spdlog::info("Tracer finished.");
    spdlog::shutdown();
    return 0;
}