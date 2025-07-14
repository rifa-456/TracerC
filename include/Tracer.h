#pragma once

#include <map>
#include <string>
#include <sys/types.h>
#include <vector>

class Tracer
{
  public:
    explicit Tracer(pid_t pid, bool is_forked_process = false);
    ~Tracer() = default;
    Tracer(const Tracer &) = delete;
    Tracer &operator=(const Tracer &) = delete;
    void run();

  private:
    void log_syscall_entry(pid_t pid);
    void log_syscall_exit(pid_t pid);
    std::map<pid_t, bool> m_threads_in_syscall;
    pid_t m_initial_fork_pid = -1;
};

void fork_and_trace(const std::vector<std::string> &args);