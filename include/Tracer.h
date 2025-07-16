#pragma once

#include <map>    // Utilizado para controlar os estados do PTrace das threads
#include <string> // Utilizado em vários lugares
#include <vector> // Utilizado nos vetores de PID e argumentos

/**
 * @class Tracer
 * @brief Gerencia o rastreamento de processos usando ptrace.
 *
 * Esta classe encapsula a lógica para se anexar a processos,
 * monitorar suas chamadas de sistema e lidar com eventos de processo como forks e execs.
 */
class Tracer
{
  public:
    /**
     * @brief Constrói um objeto Tracer.
     * @param pids Um vetor de PIDs/TIDs iniciais para rastrear.
     */
    explicit Tracer(const std::vector<pid_t> &pids);

    /**
     * @brief Inicia o loop principal de rastreamento.
     * Este método aguarda os processos rastreados (tracees) pararem e processa os eventos do
     * ptrace.
     */
    void run();

  private:
    /**
     * @brief Registra a entrada de uma chamada de sistema.
     * @param pid O PID do processo que fez a chamada de sistema.
     */
    static void log_syscall_entry(pid_t pid);

    /**
     * @brief Registra a saída de uma chamada de sistema.
     * @param pid O PID do processo que fez a chamada de sistema.
     */
    static void log_syscall_exit(pid_t pid);

    /// @brief (Não utilizado) Destinado a armazenar o PID do processo inicial criado com fork.
    pid_t m_initial_fork_pid = -1;

    /// @brief Rastreia se uma thread está atualmente dentro de uma chamada de sistema (entre a
    /// entrada e a saída). A chave é o PID/TID, e o valor é verdadeiro se estiver dentro de uma
    /// chamada de sistema, falso caso contrário. Necessário para distinguir as paradas de entrada e
    /// saída da chamada de sistema.
    std::map<pid_t, bool> m_threads_in_syscall;

    /// @brief Rastreia se um processo acabou de executar `execve` com sucesso.
    /// A chave é o PID/TID. Isso é necessário para registrar corretamente o resultado do `execve`,
    /// que reporta sucesso na parada de *saída* da chamada de sistema.
    std::map<pid_t, bool> m_just_execed;
};

/**
 * @brief Cria um novo processo com fork e o rastreia.
 * @param args O comando e os argumentos para o programa a ser executado.
 * Esta função lida com o código padrão (boilerplate) de criar um fork, configurar o ptrace no
 * processo filho, e então iniciar uma instância do Tracer.
 */
void fork_and_trace(const std::vector<std::string> &args);