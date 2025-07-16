#include "Tracer.h"  // Headers do projeto
#include "Syscall.h" // Headers do projeto

#include "spdlog/spdlog.h" // Usado para fazer o logging do tracer

#include <sys/ptrace.h> // Usado pelo Ptrace

#include <sys/user.h> // Usado pela struct "user_regs_struct" para ler os registradores da CPU.

#include <sys/wait.h> // Usado pelos macros associadas (WIFEXITED, etc.).

/**
 * @brief Lê uma string terminada por nulo da memória de um processo rastreado.
 * @param pid O PID do processo alvo.
 * @param addr O endereço de memória da string no processo alvo.
 * @return A string lida do processo, ou uma mensagem de erro.
 */
static std::string read_string_from_process(pid_t pid, unsigned long addr)
{
    // Um endereço de ponteiro nulo é simplesmente retornado como "NULL".
    if (addr == 0)
        return "NULL";

    // Bloco para ler a memória do processo rastreado (tracee) palavra por palavra.
    std::string out;
    for (int i = 0; i < 256 / sizeof(long); ++i)
    {
        // PEEKDATA lê uma palavra (long) da memória do tracee.
        errno = 0;
        long word = ptrace(PTRACE_PEEKDATA, pid, addr + i * sizeof(long), nullptr);

        // Extrai os bytes da palavra e os anexa à string de saída
        // até que um terminador nulo seja encontrado.
        // isso é necessário por que só a syscall write possui "count", logo para ser possível
        // imprimir outras possíveis string essa abordagem é necessaria
        char *bytes = reinterpret_cast<char *>(&word);
        for (size_t j = 0; j < sizeof(long); ++j)
        {
            if (bytes[j] == '\0')
                return fmt::format(
                    "\"{}\"", out); // Retorna a string entre aspas ao encontrar o terminador nulo.
            out += bytes[j];
        }
    }
    // Se a string for muito longa, ela é truncada.
    return fmt::format("\"{}...\"", out);
}

/**
 * @brief Formata um argumento de syscall para logging com base em seu tipo e valor.
 * @param pid O PID do processo, necessário para ler strings.
 * @param type O tipo em estilo C do argumento (ex: "char*").
 * @param value O valor inteiro do argumento vindo do registrador.
 * @return Uma representação do argumento em string formatada.
 */
static std::string format_argument(pid_t pid, const std::string &type, long long value)
{
    if (type.find("char") != std::string::npos &&
        type.find('*') != std::string::npos) // Condicional que procura 'char' E '*'
        // dentro da string "type"
        return read_string_from_process(pid, (unsigned long)value);
    // Para números grandes, formata como hexadecimal para legibilidade (provavelmente um endereço
    // ou flags).
    if (value > 1000000)
        return fmt::format("{:#x}", value);
    // Caso contrário, formata como um número decimal simples.
    return fmt::format("{}", value);
}

/**
 * @brief Cria um fork do processo atual para gerar um filho, que é então rastreado.
 * @param args Os argumentos da linha de comando do programa a ser executado no filho.
 */
void fork_and_trace(const std::vector<std::string> &args)
{
    // Cria um processo filho.
    pid_t child = fork();

    // Este bloco é executado apenas pelo processo filho.
    if (child == 0)
    {
        // PTRACE_TRACEME informa ao processo pai que ele deve ser rastreado.
        ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);
        // SIGSTOP pausa o processo filho, permitindo que o pai configure as opções do ptrace antes
        // que ele execute o execvp.
        raise(SIGSTOP);

        // Este bloco converte o std::vector<std::string> em um array de char*, adequado para o
        // execvp.
        std::vector<char *> cargs;
        for (auto &s : args)
            cargs.push_back(const_cast<char *>(s.c_str()));
        cargs.push_back(nullptr); // execvp requer um array terminado por nulo.

        // execvp substitui a imagem do processo filho pelo novo programa.
        execvp(cargs[0], cargs.data());
        // Se execvp retornar, ocorreu um erro.
        _exit(127);
    }

    // Este bloco é executado pelo processo pai.
    // Ele aguarda o filho enviar o SIGSTOP.
    int status = 0;
    waitpid(child, &status, 0);

    // Configura as opções do ptrace no filho para rastrear syscalls e futuros forks/clones/execs.
    ptrace(PTRACE_SETOPTIONS, child, nullptr,
           PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK |
               PTRACE_O_TRACEEXEC | PTRACE_O_EXITKILL);
    // Continua a execução do filho e diz para ele parar na próxima entrada ou saída de syscall.
    ptrace(PTRACE_SYSCALL, child, nullptr, nullptr);

    // Cria e executa a instância do Tracer com o PID do novo filho.
    std::vector<pid_t> pids_to_trace = {child};
    Tracer tracer(pids_to_trace);
    tracer.run();
}

/**
 * @brief Constrói um Tracer e inicializa seu estado interno.
 * @param pids Um vetor de PIDs iniciais para rastrear.
 */
Tracer::Tracer(const std::vector<pid_t> &pids)
{
    // Este loop inicializa o estado para cada PID que está sendo rastreado.
    // Toda thread é inicialmente marcada como não estando em uma syscall e não tendo acabado de
    // executar um exec.
    for (pid_t pid : pids)
    {
        m_threads_in_syscall[pid] = false;
        m_just_execed[pid] = false;
        spdlog::info("Rastreando PID {}", pid);
    }
}

/**
 * @brief O loop de eventos principal para o tracer.
 *
 * Esta função continuamente aguarda os processos rastreados pararem e lida com
 * os eventos que causaram a parada (ex: syscalls, sinais, forks).
 */
void Tracer::run()
{
    spdlog::info("[run] entrando no loop principal");
    // O loop continua enquanto houver threads sendo rastreadas.
    while (!m_threads_in_syscall.empty())
    {
        int status = 0;
        // waitpid com -1 aguarda por qualquer processo filho. __WALL inclui threads.
        pid_t pid = waitpid(-1, &status, __WALL);

        // Se waitpid retornar um erro.
        if (pid <= 0)
        {
            // ECHILD significa que não há mais filhos para esperar, então podemos sair.
            if (errno == ECHILD)
            {
                break;
            }
            continue;
        }

        // Este bloco lida com um processo que terminou ou foi encerrado por um sinal.
        if (WIFEXITED(status) || WIFSIGNALED(status))
        {
            // Remove o PID que saiu dos mapas para parar de rastreá-lo.
            m_threads_in_syscall.erase(pid);
            m_just_execed.erase(pid);
            continue;
        }

        // Nós só nos importamos com processos que estão parados.
        if (!WIFSTOPPED(status))
        {
            continue;
        }

        // Isso pode acontecer se uma nova thread for criada mas ainda não estiver sendo rastreada.
        if (!m_threads_in_syscall.count(pid))
        {
            ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr); // Resume sua execução.
            continue;
        }

        // Eventos do Ptrace (como fork, clone, exec) são codificados no status.
        unsigned event = (unsigned)status >> 16;
        if (event)
        {
            switch (event)
            {
            // Um processo chamou execve.
            case PTRACE_EVENT_EXEC:
                m_just_execed[pid] = true;
                break;

            // Um processo usou fork ou clone para criar um novo processo/thread.
            case PTRACE_EVENT_FORK:
            case PTRACE_EVENT_VFORK:
            case PTRACE_EVENT_CLONE:
            {
                // Obtém o PID do novo processo/thread.
                unsigned long np = 0;
                ptrace(PTRACE_GETEVENTMSG, pid, nullptr, &np);
                pid_t newpid = (pid_t)np;

                // Configura as mesmas opções de ptrace no novo processo para que ele também seja
                // rastreado.
                ptrace(PTRACE_SETOPTIONS, newpid, nullptr,
                       PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK |
                           PTRACE_O_TRACEVFORK | PTRACE_O_TRACEEXEC | PTRACE_O_EXITKILL);

                // Adiciona o novo PID aos nossos mapas para começar a rastreá-lo.
                m_threads_in_syscall[newpid] = false;
                m_just_execed[newpid] = false;

                // Resume o novo processo e o faz parar na próxima syscall.
                ptrace(PTRACE_SYSCALL, newpid, nullptr, nullptr);
                break;
            }
            default:
                break;
            }
        }

        // WSTOPSIG obtém o sinal que causou a parada.
        // PTRACE_O_TRACESYSGOOD faz com que paradas por syscall resultem em (SIGTRAP | 0x80).
        int sig = WSTOPSIG(status);
        if (sig == (SIGTRAP | 0x80))
        {
            // Esta lógica diferencia entre a entrada e a saída de uma syscall.
            bool &in = m_threads_in_syscall[pid];
            if (!in)
            {
                // Se não está em uma syscall, esta é uma entrada de syscall.
                log_syscall_entry(pid);
                in = true; // Marca como dentro de uma syscall.
            }
            else
            {
                // Se já está em uma syscall, esta é uma saída de syscall.
                // Tratamento especial para a mensagem de sucesso do execve.
                if (m_just_execed[pid])
                {
                    m_just_execed[pid] = false;
                }
                else
                {
                    log_syscall_exit(pid);
                }
                in = false; // Marca como não estando mais em uma syscall.
            }
            // Resume o processo e o faz parar no próximo evento de syscall.
            ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr);
        }
        else
        {
            // Se for um sinal diferente, o encaminha para o processo e continua o rastreamento.
            ptrace(PTRACE_SYSCALL, pid, nullptr, sig);
        }
    }
}

void Tracer::log_syscall_entry(pid_t pid)
{
    user_regs_struct regs{};
    ptrace(PTRACE_GETREGS, pid, nullptr, &regs); // Pegar os valores nos registradores
    auto info = Syscall::get_syscall_info(
        regs.orig_rax); // O valor do id da chamada de sistema em x86-64 está no registrador 'rax'.
    if (info)
    {
        std::string args_str;
        long long vals[] = {
            (long long)regs.rdi, (long long)regs.rsi, (long long)regs.rdx, (long long)regs.r10,
            (long long)regs.r8,  (long long)regs.r9}; // Os 6 registradores de valor do Ptrace
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
    }
    else
    {
        spdlog::warn("SYSCALL_ENTRY [PID:{}] unknown {}", pid, regs.orig_rax);
    }
}

/**
 * @brief Registra o resultado de uma chamada de sistema em seu ponto de saída.
 * @param pid O PID do processo cuja chamada está retornando.
 *
 * Lê o valor de retorno do registrador 'rax' e o registra no log.
 */
void Tracer::log_syscall_exit(pid_t pid)
{
    user_regs_struct regs{};

    ptrace(PTRACE_GETREGS, pid, nullptr, &regs);

    const auto info =
        Syscall::get_syscall_info(regs.orig_rax); // O número da syscall ainda está em 'orig_rax'.

    auto ret = (long long)regs.rax; // O valor de retorno em x86-64 está no registrador 'rax'.
    const char *name = info ? info->name.c_str() : "syscall";

    char buf[256];
    if (ret < 0) // Valores de retorno negativos geralmente indicam um erro.
    {
        snprintf(buf, sizeof(buf), "%lld (%s)", ret, strerror(-ret));
    }
    else if (ret > 1000000) // Valores grandes são provavelmente ponteiros/handles, logar como hex.
    {
        snprintf(buf, sizeof(buf), "%#llx", ret);
    }
    else // Valores pequenos são provavelmente inteiros ou descritores de arquivo.
    {
        snprintf(buf, sizeof(buf), "%lld", ret);
    }
    spdlog::info("SYSCALL_EXIT  [PID:{}] {} = {}", pid, name, buf);
}
