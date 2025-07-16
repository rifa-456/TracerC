#pragma once

#include <map>    // Utilizado para controlar os estados do PTrace das threads
#include <string> // Utilizado em vários lugares
#include <vector> // Utilizado nos vetores de PID e argumentos

/**
 * @class Tracer
 * @brief Manages the tracing of processes using ptrace.
 *
 * This class encapsulates the logic for attaching to processes,
 * monitoring their system calls, and handling process events like forks and execs.
 */
class Tracer
{
  public:
    /**
     * @brief Constructs a Tracer object.
     * @param pids A vector of initial PIDs/TIDs to trace.
     */
    explicit Tracer(const std::vector<pid_t> &pids);

    /**
     * @brief Starts the main tracing loop.
     * This method waits for tracees to stop and processes ptrace events.
     */
    void run();

    /**
     * @brief Shuts down the tracer gracefully.
     * Detaches from all currently traced processes.
     */
    void shutdown();

  private:
    /**
     * @brief Logs the entry of a system call.
     * @param pid The PID of the process that made the syscall.
     */
    static void log_syscall_entry(pid_t pid);

    /**
     * @brief Logs the exit of a system call.
     * @param pid The PID of the process that made the syscall.
     */
    static void log_syscall_exit(pid_t pid);

    /// @brief (Unused) Intended to store the PID of the initial forked process.
    pid_t m_initial_fork_pid = -1;

    /// @brief Tracks whether a thread is currently inside a syscall (between entry and exit).
    /// The key is the PID/TID, and the value is true if inside a syscall, false otherwise.
    /// Needed to distinguish syscall entry from exit stops.
    std::map<pid_t, bool> m_threads_in_syscall;

    /// @brief Tracks if a process has just successfully executed execve.
    /// The key is the PID/TID. This is needed to correctly log the result of execve,
    /// which reports success on the syscall *exit* stop.
    std::map<pid_t, bool> m_just_execed;
};

/**
 * @brief Forks a new process and traces it.
 * @param args The command and arguments for the program to be executed.
 * This function handles the boilerplate of forking, setting up ptrace on the child,
 * and then starting a Tracer instance.
 */
void fork_and_trace(const std::vector<std::string> &args);