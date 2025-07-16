#include "Tracer.h"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <cxxopts.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <queue>
#include <set>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <vector>

void setup_logger()
{
    try
    {
        auto now = std::chrono::system_clock::now();
        auto tt = std::chrono::system_clock::to_time_t(now);
        tm tm_local{};
        localtime_r(&tt, &tm_local);
        std::stringstream ss;
        ss << std::put_time(&tm_local, "%d-%m-%Y:%H-%M-%S");
        std::string fname = "logs/trace-" + ss.str() + ".log";

        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(fname, true);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        file_sink->set_level(spdlog::level::trace);
        console_sink->set_level(spdlog::level::info);

        spdlog::logger logger("tracer", {console_sink, file_sink});
        logger.set_level(spdlog::level::trace);
        spdlog::set_default_logger(std::make_shared<spdlog::logger>(logger));
        spdlog::flush_on(spdlog::level::info);
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        std::cerr << "Log init failed: " << ex.what() << std::endl;
    }
}

std::vector<pid_t> find_all_descendants(pid_t root_pid)
{
    std::set<pid_t> pids;
    std::queue<pid_t> q;
    q.push(root_pid);
    while (!q.empty())
    {
        pid_t current_pid = q.front();
        q.pop();
        std::string task_path = "/proc/" + std::to_string(current_pid) + "/task";
        if (!std::filesystem::exists(task_path))
        {
            continue;
        }
        for (const auto &entry : std::filesystem::directory_iterator(task_path))
        {
            pid_t tid = std::stoi(entry.path().filename().string());
            pids.insert(tid);
            std::string children_path = entry.path().string() + "/children";
            std::ifstream children_file(children_path);
            pid_t child_pid;
            while (children_file >> child_pid)
            {
                if (pids.find(child_pid) == pids.end())
                {
                    q.push(child_pid);
                }
            }
        }
    }
    return std::vector<pid_t>(pids.begin(), pids.end());
}

int main(int argc, char *argv[])
{
    setup_logger();
    spdlog::info("[main] starting, argc={}", argc);

    cxxopts::Options options("TracerC", "C++ ptrace-based syscall tracer");
    options.add_options()("a,attach", "PID to attach to", cxxopts::value<pid_t>())(
        "f,fork", "Program to fork+trace",
        cxxopts::value<std::vector<std::string>>())("h,help", "Print help");
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
            pid_t root_pid = result["attach"].as<pid_t>();
            spdlog::info("[main] finding all descendants for root PID {}", root_pid);
            std::vector<pid_t> pids_to_trace = find_all_descendants(root_pid);
            spdlog::info("[main] found {} processes in tree to attach to.", pids_to_trace.size());
            for (pid_t pid : pids_to_trace)
            {
                spdlog::info("[main] attaching to PID {}", pid);
                if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) == -1)
                {
                    spdlog::warn("ptrace attach {} failed: {}", pid, strerror(errno));
                    continue;
                }
                waitpid(pid, nullptr, 0);
                spdlog::info("[main] setting PTRACE options on pid {}", pid);
                ptrace(PTRACE_SETOPTIONS, pid, nullptr,
                       PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK |
                           PTRACE_O_TRACEVFORK | PTRACE_O_TRACEEXEC | PTRACE_O_EXITKILL);
            }
            Tracer tracer(pids_to_trace);
            for (pid_t pid : pids_to_trace)
            {
                ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr);
            }
            tracer.run();
        }
        else if (result.count("fork"))
        {
            auto args = result["fork"].as<std::vector<std::string>>();
            fork_and_trace(args);
        }
        else
        {
            spdlog::error("[main] no valid mode selected");
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("[main] unhandled exception: {}", e.what());
        return 1;
    }

    spdlog::info("[main] done");
    spdlog::shutdown();
    return 0;
}