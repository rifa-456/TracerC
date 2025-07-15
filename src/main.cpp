#include "Tracer.h"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <cxxopts.hpp>
#include <iomanip>
#include <iostream>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <vector>

// No changes needed in setup_logger()
void setup_logger()
{
    try
    {
        auto now = std::chrono::system_clock::now();
        auto na_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss_date_hour;
        tm tm_local{};
        localtime_r(&na_time_t, &tm_local);
        ss_date_hour << std::put_time(&tm_local, "%d-%m-%Y:%H-%M-%S");
        std::stringstream ss_file_name;
        ss_file_name << "logs/trace-" << ss_date_hour.str() << ".log";
        auto file_sink =
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(ss_file_name.str(), true);
        file_sink->set_level(spdlog::level::trace);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);
        spdlog::sinks_init_list sink_list = {console_sink, file_sink};
        auto logger = std::make_shared<spdlog::logger>("tracer", sink_list);
        logger->set_level(spdlog::level::trace);
        spdlog::set_default_logger(logger);
        spdlog::flush_on(spdlog::level::info);
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        std::cerr << "Inicialização de Log falhou: " << ex.what() << std::endl;
    }
}

int main(int argc, char *argv[])
{
    setup_logger();
    cxxopts::Options options("trace",
                             "Um tracer de chamadas de sistema escrito em C++ usando ptrace");
    options.add_options()("a,attach", "PID para fazer o tracing", cxxopts::value<pid_t>())(
        "f,fork", "Endereço do arquivo para ser forkeado e depois fazer o traceado",
        cxxopts::value<std::vector<std::string>>())("h,help", "Imprimir ajuda");
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
            spdlog::info("Tentando fazer tracing no programa com PID: {}", pid);
            if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) == -1)
            {
                spdlog::critical("Falha ao escutar o programa com PID {}: {}", pid,
                                 strerror(errno));
                return 1;
            }
            waitpid(pid, nullptr, 0); // Wait for the attach to complete

            // **FIX**: Use the same comprehensive set of options as the fork case.
            ptrace(PTRACE_SETOPTIONS, pid, nullptr,
                   PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK |
                       PTRACE_O_TRACEVFORK | PTRACE_O_TRACEEXEC | PTRACE_O_EXITKILL);

            ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr);
            Tracer tracer(pid, false);
            tracer.run();
        }
        else if (result.count("fork"))
        {
            const auto &fork_args = result["fork"].as<std::vector<std::string>>();
            fork_and_trace(fork_args);
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("Um erro desconhecido ocorreu: {}", e.what());
        return 1;
    }
    spdlog::info("Tracing finalizado.");
    spdlog::shutdown();
    return 0;
}