// src/Tracer.cpp
#include "Tracer.h"
#include "Syscall.h"
#include "spdlog/spdlog.h"
#include <cerrno>
#include <csignal>
#include <cstring>
#include <execinfo.h>
#include <fmt/core.h>
#include <stdexcept>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

static void segv_handler(int sig)
{
    void *buf[64];
    int n = backtrace(buf, 64);
    char **syms = backtrace_symbols(buf, n);
    spdlog::critical("[SIGSEGV] signal={}, backtrace:", sig);
    for (int i = 0; i < n; ++i)
    {
        spdlog::critical("  #{} {}", i, syms[i]);
    }
    free(syms);
    _exit(1);
}

static std::string read_string_from_process(pid_t pid, unsigned long addr)
{
    if (addr == 0)
        return "NULL";
    std::string out;
    out.reserve(256);
    for (int i = 0; i < 256 / sizeof(long); ++i)
    {
        errno = 0;
        long word = ptrace(PTRACE_PEEKDATA, pid, addr + i * sizeof(long), nullptr);
        if (word == -1 && errno)
        {
            std::string err = strerror(errno);
            return fmt::format("\"<error at {:#x}: {}>\"", addr, err);
        }
        char *bytes = reinterpret_cast<char *>(&word);
        for (size_t j = 0; j < sizeof(long); ++j)
        {
            if (bytes[j] == '\0')
                return fmt::format("\"{}\"", out);
            out += bytes[j];
        }
    }
    return fmt::format("\"{}...\"", out);
}

static std::string format_argument(pid_t pid, const std::string &type, long long value)
{
    // only log very top-level; skip per-byte trace
    if (type.find("char") != std::string::npos && type.find('*') != std::string::npos)
        return read_string_from_process(pid, (unsigned long)value);
    if (value > 1000000)
        return fmt::format("{:#x}", value);
    return fmt::format("{}", value);
}

void fork_and_trace(const std::vector<std::string> &args)
{
    signal(SIGSEGV, segv_handler);
    spdlog::info("[fork_and_trace] argv size={}", args.size());
    if (args.empty())
    {
        spdlog::error("No program specified for fork.");
        return;
    }
    pid_t child = fork();
    spdlog::info("[fork_and_trace] fork() -> {}", child);
    if (child < 0)
    {
        spdlog::critical("fork failed: {}", strerror(errno));
        return;
    }
    if (child == 0)
    {
        ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);
        raise(SIGSTOP);
        std::vector<char *> cargs;
        for (auto &s : args)
            cargs.push_back(const_cast<char *>(s.c_str()));
        cargs.push_back(nullptr);
        execvp(cargs[0], cargs.data());
        _exit(127);
    }

    int status = 0;
    waitpid(child, &status, 0);
    spdlog::info("[parent] child stopped, setting PTRACE options");
    ptrace(PTRACE_SETOPTIONS, child, nullptr,
           PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK |
               PTRACE_O_TRACEEXEC | PTRACE_O_EXITKILL);
    ptrace(PTRACE_SYSCALL, child, nullptr, nullptr);
    spdlog::info("Tracing process PID={}", child);
    std::vector<pid_t> pids_to_trace = {child};
    Tracer tracer(pids_to_trace);
    tracer.run();
}

Tracer::Tracer(const std::vector<pid_t> &pids)
{
    signal(SIGSEGV, segv_handler);
    spdlog::info("[Tracer ctor] preparing to trace {} processes", pids.size());
    for (pid_t pid : pids)
    {
        m_threads_in_syscall[pid] = false;
        m_just_execed[pid] = false;
        spdlog::info("Tracking PID {}", pid);
    }
}

void Tracer::run()
{
    spdlog::info("[run] entering main loop");
    while (!m_threads_in_syscall.empty())
    {
        int status = 0;
        pid_t pid = waitpid(-1, &status, __WALL);
        if (pid <= 0)
        {
            if (errno == ECHILD)
            {
                spdlog::info("[run] no more traced processes");
                break;
            }
            spdlog::warn("[run] waitpid error: {}", strerror(errno));
            continue;
        }
        if (WIFEXITED(status) || WIFSIGNALED(status))
        {
            spdlog::info("[run] PID {} exited (status={:#x})", pid, status);
            m_threads_in_syscall.erase(pid);
            m_just_execed.erase(pid);
            continue;
        }
        if (!WIFSTOPPED(status))
        {
            spdlog::warn("[run] pid {} stopped, not WIFSTOPPED", pid);
            continue;
        }
        if (!m_threads_in_syscall.count(pid))
        {
            spdlog::debug("[run] skipping untracked pid={}", pid);
            ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr);
            continue;
        }
        unsigned event = (unsigned)status >> 16;
        if (event)
        {
            switch (event)
            {
            case PTRACE_EVENT_EXEC:
                spdlog::info("[run] execve event on pid={}", pid);
                m_just_execed[pid] = true;
                break;

            case PTRACE_EVENT_FORK:
            case PTRACE_EVENT_VFORK:
            case PTRACE_EVENT_CLONE:
            {
                unsigned long np = 0;
                ptrace(PTRACE_GETEVENTMSG, pid, nullptr, &np);
                pid_t newpid = (pid_t)np;
                spdlog::info("[run] fork/clone: parent={} new={}", pid, newpid);
                ptrace(PTRACE_SETOPTIONS, newpid, nullptr,
                       PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK |
                           PTRACE_O_TRACEVFORK | PTRACE_O_TRACEEXEC | PTRACE_O_EXITKILL);
                m_threads_in_syscall[newpid] = false;
                m_just_execed[newpid] = false;
                ptrace(PTRACE_SYSCALL, newpid, nullptr, nullptr);
                break;
            }
            default:
                break;
            }
        }
        int sig = WSTOPSIG(status);
        if (sig == (SIGTRAP | 0x80))
        {
            bool &in = m_threads_in_syscall[pid];
            if (!in)
            {
                log_syscall_entry(pid);
                in = true;
            }
            else
            {
                if (m_just_execed[pid])
                {
                    spdlog::info("SYSCALL_EXIT  [PID:{}] execve successful", pid);
                    m_just_execed[pid] = false;
                }
                else
                {
                    log_syscall_exit(pid);
                }
                in = false;
            }
            ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr);
        }
        else
        {
            ptrace(PTRACE_SYSCALL, pid, nullptr, sig);
        }
    }
    spdlog::info("[run] exiting main loop");
}

void Tracer::log_syscall_entry(pid_t pid)
{
    user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, nullptr, &regs) == -1)
    {
        spdlog::warn("[entry] GETREGS failed for {}: {}", pid, strerror(errno));
        return;
    }
    auto info = Syscall::get_syscall_info(regs.orig_rax);
    if (info)
    {
        std::string args_str;
        long long vals[] = {(long long)regs.rdi, (long long)regs.rsi, (long long)regs.rdx,
                            (long long)regs.r10, (long long)regs.r8,  (long long)regs.r9};
        if (!info->arg_types.empty())
        {
            for (int i = 0; i < info->arg_count; ++i)
            {
                if (i > 0)
                    args_str += ", ";
                args_str += format_argument(pid, info->arg_types[i], vals[i]);
            }
        }
        else
        {
            for (int i = 0; i < info->arg_count; ++i)
            {
                if (i > 0)
                    args_str += ", ";
                args_str += fmt::format("{:#x}", vals[i]);
            }
        }
        spdlog::info("SYSCALL_ENTRY [PID:{}] {}({})", pid, info->name, args_str);
    }
    else
    {
        spdlog::warn("SYSCALL_ENTRY [PID:{}] unknown {}", pid, regs.orig_rax);
    }
}

void Tracer::log_syscall_exit(pid_t pid)
{
    user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, pid, nullptr, &regs) == -1)
    {
        spdlog::warn("[exit] GETREGS failed for {}: {}", pid, strerror(errno));
        return;
    }
    auto info = Syscall::get_syscall_info(regs.orig_rax);
    long long ret = (long long)regs.rax;
    const char *name = info ? info->name.c_str() : "syscall";
    char buf[64];
    if (ret < 0)
    {
        snprintf(buf, sizeof(buf), "%lld (%s)", ret, strerror(-ret));
    }
    else if (ret > 1000000)
    {
        snprintf(buf, sizeof(buf), "%#llx", ret);
    }
    else
    {
        snprintf(buf, sizeof(buf), "%lld", ret);
    }
    spdlog::info("SYSCALL_EXIT  [PID:{}] {} = {}", pid, name, buf);
}
