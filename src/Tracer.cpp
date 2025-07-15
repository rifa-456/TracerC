#include "Tracer.h"
#include "Syscall.h"
#include "spdlog/spdlog.h"
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fmt/core.h>
#include <stdexcept>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

static std::string read_string_from_process(pid_t pid, unsigned long addr)
{
    if (addr == 0)
    {
        return "NULL";
    }
    std::string str;
    str.reserve(256);
    for (int i = 0; i < 256 / sizeof(long); ++i)
    {
        errno = 0;
        long data = ptrace(PTRACE_PEEKDATA, pid, addr + i * sizeof(long), nullptr);
        if (data == -1 && errno != 0)
        {
            return fmt::format("\"<error reading at {:#x}: {}>\"", addr, strerror(errno));
        }
        char *bytes = reinterpret_cast<char *>(&data);
        for (size_t j = 0; j < sizeof(long); ++j)
        {
            if (bytes[j] == '\0')
            {
                return fmt::format("\"{}\"", str);
            }
            str += bytes[j];
        }
    }
    return fmt::format("\"{}...\"", str);
}

// This function is generally fine, no changes needed.
static std::string format_argument(pid_t pid, const std::string &type, long long value)
{
    if (type.find("char") != std::string::npos && type.find('*') != std::string::npos)
    {
        return read_string_from_process(pid, value);
    }
    if (value > 1000000)
    {
        return fmt::format("{:#x}", value);
    }
    return fmt::format("{}", value);
}

void fork_and_trace(const std::vector<std::string> &args)
{
    if (args.empty())
    {
        spdlog::error("No program specified for fork.");
        return;
    }
    pid_t child_pid = fork();
    if (child_pid == -1)
    {
        spdlog::critical("fork failed: {}", strerror(errno));
        return;
    }
    if (child_pid == 0)
    {
        std::vector<char *> c_args;
        for (auto &arg : args)
            c_args.push_back(const_cast<char *>(arg.c_str()));
        c_args.push_back(nullptr);
        ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);
        raise(SIGSTOP); // Crucial for preventing race condition
        execvp(c_args[0], c_args.data());
        _exit(127);
    }
    int status = 0;
    if (waitpid(child_pid, &status, 0) == -1)
    {
        spdlog::critical("waitpid failed: {}", strerror(errno));
        return;
    }
    // Set all options now that we know the child is stopped and waiting
    ptrace(PTRACE_SETOPTIONS, child_pid, nullptr,
           PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK |
               PTRACE_O_TRACEEXEC | PTRACE_O_EXITKILL);

    // Now, continue the child. It will stop at the execve syscall entry.
    ptrace(PTRACE_SYSCALL, child_pid, nullptr, nullptr);
    spdlog::info("Tracing process PID={}…", child_pid);

    Tracer tracer(child_pid, true);
    tracer.run();
}

Tracer::Tracer(pid_t pid, bool is_forked_process)
{
    if (is_forked_process)
    {
        m_initial_fork_pid = pid;
    }
    m_threads_in_syscall[pid] = false;
    m_just_execed[pid] = false;
    spdlog::info("Attached to PID {}", pid);
}
void Tracer::run()
{
    while (!m_threads_in_syscall.empty())
    {
        int status;
        pid_t pid = waitpid(-1, &status, __WALL);

        if (pid <= 0)
        {
            if (errno == ECHILD)
                break;
            continue;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status))
        {
            spdlog::info("Process {} has exited.", pid);
            m_threads_in_syscall.erase(pid);
            continue;
        }

        if (!WIFSTOPPED(status))
            continue;

        // First, check for the special ptrace events, as they have priority.
        unsigned int ptrace_event = (unsigned int)status >> 16;
        if (ptrace_event != 0)
        {
            switch (ptrace_event)
            {
            case PTRACE_EVENT_EXEC:
            {
                spdlog::info("Process {} successfully executed a new program.", pid);
                m_threads_in_syscall[pid] = false;
                m_just_execed[pid] = true;
                break;
            }
            case PTRACE_EVENT_FORK:
            case PTRACE_EVENT_VFORK:
            case PTRACE_EVENT_CLONE:
            {
                unsigned long new_pid_ul = 0;
                ptrace(PTRACE_GETEVENTMSG, pid, nullptr, &new_pid_ul);
                pid_t new_pid = static_cast<pid_t>(new_pid_ul);

                ptrace(PTRACE_SETOPTIONS, new_pid, nullptr,
                       PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK |
                           PTRACE_O_TRACEVFORK | PTRACE_O_TRACEEXEC | PTRACE_O_EXITKILL);

                spdlog::info("Attached to new thread/process PID={}", new_pid);
                m_threads_in_syscall[new_pid] = false;
                m_just_execed[new_pid] = false;
                break;
            }
            default:
                ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr);
                break;
            }
            continue; // We have handled the event, move on to the next waitpid.
        }

        // If it was not a ptrace event, it must be a normal syscall or signal stop.
        int sig = WSTOPSIG(status);
        if (sig == (SIGTRAP | 0x80)) // It's a syscall stop
        {
            auto in_syscall_it = m_threads_in_syscall.find(pid);
            if (in_syscall_it == m_threads_in_syscall.end())
            {
                ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr);
                continue;
            }

            if (!in_syscall_it->second) // Syscall Entry
            {
                auto just_execed_it = m_just_execed.find(pid);
                if (just_execed_it != m_just_execed.end() && just_execed_it->second)
                {
                    // **THE FIX**: This is the first stop after exec. orig_rax is stale.
                    // We skip logging the entry to avoid confusion, but we must update
                    // our state to reflect that we are now inside a syscall.
                    just_execed_it->second = false; // Consume the flag
                }
                else
                {
                    log_syscall_entry(pid);
                }
                in_syscall_it->second = true; // Mark that we are in a syscall
            }
            else // Syscall Exit
            {
                log_syscall_exit(pid);
                in_syscall_it->second = false;
            }
            ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr);
        }
        else // It's a signal-delivery stop
        {
            ptrace(PTRACE_SYSCALL, pid, nullptr, sig);
        }
    }
}

void Tracer::log_syscall_entry(pid_t pid)
{
    user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, nullptr, &regs) == -1)
    {
        if (errno == ESRCH)
        {
            spdlog::warn("SYSCALL_ENTRY [PID:{}] <-- [Process vanished]", pid);
        }
        else
        {
            spdlog::warn("SYSCALL_ENTRY [PID:{}] could not get registers: {}", pid,
                         strerror(errno));
        }
        return;
    }
    const Syscall::SyscallInfo *info = Syscall::get_syscall_info(regs.orig_rax);
    if (info)
    {
        std::string args_str;
        long long syscall_args[] = {(long long)regs.rdi, (long long)regs.rsi, (long long)regs.rdx,
                                    (long long)regs.r10, (long long)regs.r8,  (long long)regs.r9};
        for (int i = 0; i < info->arg_count; ++i)
        {
            args_str += format_argument(pid, info->arg_types[i], syscall_args[i]);
            if (i < info->arg_count - 1)
            {
                args_str += ", ";
            }
        }
        spdlog::info("SYSCALL_ENTRY [PID:{}] --> {}({})", pid, info->name, args_str);
    }
    else
    {
        spdlog::warn(
            "SYSCALL_ENTRY [PID:{}] --> syscall_{}({:#x}, {:#x}, {:#x}, {:#x}, {:#x}, {:#x})", pid,
            regs.orig_rax, regs.rdi, regs.rsi, regs.rdx, regs.r10, regs.r8, regs.r9);
    }
}

void Tracer::log_syscall_exit(pid_t pid)
{
    user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, nullptr, &regs) == -1)
    {
        if (errno == ESRCH)
        {
            spdlog::info("SYSCALL_EXIT  [PID:{}] <-- [Process vanished]", pid);
        }
        else
        {
            spdlog::warn("SYSCALL_EXIT [PID:{}] could not get registers: {}", pid, strerror(errno));
        }
        return;
    }
    const Syscall::SyscallInfo *info = Syscall::get_syscall_info(regs.orig_rax);
    auto return_val = static_cast<long long>(regs.rax);
    if (pid == m_initial_fork_pid && info && info->name == "execve" && return_val < 0)
    {
        m_initial_fork_pid = -1;
        throw std::runtime_error{strerror(-return_val)};
    }
    std::string name = info ? info->name : fmt::format("syscall_{}", regs.orig_rax);
    std::string return_str;
    if (return_val < 0)
    {
        return_str = fmt::format("{} ({})", return_val, strerror(-return_val));
    }
    else if (return_val > 1000000)
    {
        return_str = fmt::format("{:#x}", return_val);
    }
    else
    {
        return_str = fmt::format("{}", return_val);
    }
    spdlog::info("SYSCALL_EXIT  [PID:{}] <-- {} = {}", pid, name, return_str);
}