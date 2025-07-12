#pragma once

#include <map>
#include <string>
#include <sys/types.h>
#include <vector>

class Tracer
{
  public:
    explicit Tracer(pid_t pid);
    ~Tracer();
    Tracer(const Tracer &) = delete;
    Tracer &operator=(const Tracer &) = delete;
    void run();

  private:
    void log_syscall_entry(pid_t pid);
    void log_syscall_exit(pid_t pid);
    std::map<pid_t, bool> m_threads_in_syscall;
};

void fork_and_trace(const std::vector<std::string> &args);