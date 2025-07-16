#include "Tracer.h" // Header do projeto

#include <cxxopts.hpp> // Usado para analisar os argumentos da linha de comando.

#include <filesystem> // Usado para interagir com o sistema de arquivos, especificamente para navegar em /proc.

#include <fstream> // Usado para ler arquivos, como o arquivo 'children' em /proc/[pid]/task/[tid]/.

#include <iostream> // Usado para imprimir a mensagem de ajuda na saída padrão (std::cout).

#include <queue> // Usado pela função "find_all_related" para realizar uma busca em largura na árvore de processos.

#include <set> // Usado para armazenar PIDs únicos e evitar o reprocessamento na busca de processos.

#include <spdlog/sinks/basic_file_sink.h> // Usado para criar um sink do spdlog que redireciona a saída para um arquivo.

#include <spdlog/sinks/stdout_color_sinks.h> // Usado para criar um sink do spdlog que redireciona a saída colorida para o console.

#include <spdlog/spdlog.h> // Usado para a funcionalidade principal de logging com a biblioteca spdlog.

#include <sys/ptrace.h> // Usado pela chamada de sistema ptrace para anexar e controlar outros processos.

#include <sys/wait.h> // Usado pela função waitpid para aguardar por mudanças de estado nos processos filhos.

#include <vector> // Usado para armazenar a lista de argumentos do programa e os PIDs a serem rastreados.

/**
 * @brief Configura o logger global spdlog para saída em arquivo e no console.
 * @details Inicializa um logger que escreve logs de nível `info` (e superiores) no console
 * e logs de nível "trace" (e superiores) em um arquivo com data e hora no diretório `logs/`.
 */
void setup_logger()
{
    // Bloco de código para configar o logger para criar os novos arquivos de logger na pasta
    // logs com o nome de trace-DATAATUAL.log
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    tm tm_local{};
    localtime_r(&tt, &tm_local);
    std::stringstream ss;
    ss << std::put_time(&tm_local, "%d-%m-%Y:%H-%M-%S");
    std::string fname = "logs/trace-" + ss.str() + ".log";

    // Configurar para o logger mandar os logs de nivel trace para o arquivo .log e os de info
    // para o console
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(fname, true);
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    file_sink->set_level(spdlog::level::trace);
    console_sink->set_level(spdlog::level::info);
    spdlog::logger logger("tracer", {console_sink, file_sink});

    // Configurações gerais do loger, setar o nivel do log no gera para trace e fazer ele sempre
    // da flush em logs de nivel info
    logger.set_level(spdlog::level::trace);
    spdlog::set_default_logger(std::make_shared<spdlog::logger>(logger));
    spdlog::flush_on(spdlog::level::info);
}

/**
 * @brief Encontra todos os processos e threads descendentes de um determinado PID raiz.
 * @param root_pid O ID do processo a partir do qual a busca deve ser iniciada.
 * @return Um vetor contendo todos os PIDs e TIDs relacionados.
 */
std::vector<pid_t> find_all_related(pid_t root_pid)
{
    // Bloco de código que inicializa o conjunto de pids relacionados a um pid
    std::set<pid_t> pids;
    std::queue<pid_t> q;
    q.push(root_pid);

    // Loop que vai percorrer procurar todos pids relacionados ao pid raiz
    while (!q.empty())
    {
        pid_t current_pid = q.front();
        q.pop();
        std::string task_path = "/proc/" + std::to_string(current_pid) + "/task";
        if (!std::filesystem::exists(task_path))
        {
            continue;
        }

        // Para o processo atual, itera sobre suas threads em /proc/[pid]/task.
        // Adiciona o ID de cada thread (TID) à lista e verifica se a thread possui
        // processos filhos, adicionando-os à fila de busca se ainda não foram vistos.
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

    // Converte o conjunto de PIDs em um vetor e o retorna.
    return {pids.begin(), pids.end()};
}

int main(int argc, char *argv[])
{
    setup_logger(); // Configurar o logger

    // Bloco de código que configurar o cxxopts e as opções do programa, -h (ajuda), -a/-attach
    // (anexar), -f/-fork/nenhum (forkear)
    cxxopts::Options options("TracerC", "C++ ptrace-based syscall tracer");
    options.add_options()("a,attach", "PID to attach to", cxxopts::value<pid_t>())(
        "f,fork", "Program to fork+trace",
        cxxopts::value<std::vector<std::string>>())("h,help", "Print help");
    options.parse_positional({"fork"});
    options.positional_help("<program> [args...]");
    auto result = options.parse(argc, argv);

    // Se o argumento foi help ou nenhum
    if (result.count("help") || argc == 1)
    {
        std::cout << options.help() << std::endl;
        return 0;
    }

    if (result.count("attach"))
    {

        pid_t root_pid = result["attach"].as<pid_t>();
        std::vector<pid_t> pids_to_trace =
            find_all_related(root_pid); // Cria um vetor de pids para escutar
        if (pids_to_trace.empty())
        {
            spdlog::critical("Não foi possível encontrar nenhum processo para escutar!");
            return 0;
        }

        // Loop para configurar o ptrace de cada um dos pids a serem escutados
        for (pid_t pid : pids_to_trace)
        {
            if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) ==
                -1) // Inicializa o Ptrace no pid atual
            {
                spdlog::warn("Attach do processo {} falhou: {}", pid, strerror(errno));
                continue;
            }
            waitpid(pid, nullptr, 0); // Espera o processo entrar no processo SIGSTOP por conta do
                                      // PTRACE_ATTACH anterior, basicamente esperando que a
                                      // anexação foi concluída antes de continuar com a lógica
            ptrace(PTRACE_SETOPTIONS, pid, nullptr,
                   PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK |
                       PTRACE_O_TRACEVFORK |
                       PTRACE_O_TRACEEXEC); // Setar as configurações do ptrace, basicamente fazendo
                                            // ele escutar chamadas de sistema, clones, forks,
                                            // vforks, execs e mortes
        }

        Tracer tracer(pids_to_trace); // Cria um objeto tracer passando os pids a serem escutados

        // Inicializar o ptrace para a proxima chamada de sistema em cada um dos pids a serem
        // escutados
        for (pid_t pid : pids_to_trace)
        {
            ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr);
        }

        tracer.run(); // Começar loop do objeto tracer
    }
    else if (result.count("fork"))
    {
        auto args = result["fork"].as<std::vector<std::string>>();
        fork_and_trace(args); // Chamar função static fork_and_trace da classe Tracer passando o
                              // vetor de strings (para lidar com coisas do tipo Python3 ~/main.py)
    }
    spdlog::shutdown();
    return 0;
}