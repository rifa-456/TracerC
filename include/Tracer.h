#pragma once

#include <map>
#include <string>
#include <sys/types.h>
#include <vector>

class Tracer
{
  public:
    Tracer(const std::vector<pid_t> &pids);
    void run();

  private:
    void log_syscall_entry(pid_t pid);
    void log_syscall_exit(pid_t pid);
    pid_t m_initial_fork_pid = -1;
    std::map<pid_t, bool> m_threads_in_syscall;
    std::map<pid_t, bool> m_just_execed;
};

void fork_and_trace(const std::vector<std::string> &args);