#include "Tracer.h"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <cxxopts.hpp>
#include <iomanip>
#include <iostream>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <vector>

/**
 * @brief Inicializa e configura o logger global da aplicação.
 *
 * Esta função configura um logger com dois destinos (sinks): um para o console
 * e outro para um arquivo de log. O sink do console registrará mensagens a
 * partir do nível 'info'.
 *
 * O arquivo de log é nomeado dinamicamente com a data e hora de início da
 * aplicação no formato "trace-DD-MM-YYYY:HH-MM-SS.log" e registrará todas as
 * mensagens a partir do nível 'trace'.
 *
 * @note O arquivo de log será criado no diretório "logs/".
 * @throws spdlog::spdlog_ex em caso de falha na inicialização do logger.
 */
void setup_logger()
{
    try
    {
        auto now = std::chrono::system_clock::now();
        auto na_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss_date_hour;
        tm tm_local{};
        localtime_r(&na_time_t, &tm_local);
        ss_date_hour << std::put_time(&tm_local, "%d-%m-%Y:%H-%M-%S");
        std::stringstream ss_file_name;
        ss_file_name << "logs/trace-" << ss_date_hour.str() << ".log";
        auto file_sink =
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(ss_file_name.str(), true);
        file_sink->set_level(spdlog::level::trace);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);
        spdlog::sinks_init_list sink_list = {console_sink, file_sink};
        auto logger = std::make_shared<spdlog::logger>("tracer", sink_list);
        logger->set_level(spdlog::level::trace);
        spdlog::set_default_logger(logger);
        spdlog::flush_on(spdlog::level::info);
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        std::cerr << "Inicialização de Log falhou: " << ex.what() << std::endl;
    }
}

int main(int argc, char *argv[])
{
    setup_logger();
    cxxopts::Options options("trace",
                             "Um tracer de chamadas de sistema escrito em C++ usando ptrace");
    options.add_options()("a,attach", "PID para fazer o tracing", cxxopts::value<pid_t>())(
        "f,fork", "Endereço do arquivo para ser forkeado e depois fazer o traceado",
        cxxopts::value<std::vector<std::string>>())(
        "h,help", "Imprimir ajuda"); // O valor de fork é um vetor de strings para ser possível
                                     // passar argumentos para o programa que será forkeado
    options.parse_positional({"fork"});
    options.positional_help("<program> [args...]");
    try
    {
        auto result = options.parse(argc, argv);
        if (result.count("help") || argc == 1)
        {
            std::cout
                << options.help()
                << std::endl; // Imprimir ajuda se o usuário passar a flag help ou nenhum argumento
            return 0;
        }
        if (result.count("attach"))
        {
            pid_t pid = result["attach"].as<pid_t>();
            spdlog::info("Tentando fazer tracing no programa com PID: {}", pid);
            if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) == -1) // Tenta anexar ao processo
            {
                spdlog::critical("Falha ao escutar o programa com PID {}: {}", pid,
                                 strerror(errno));
                return 1;
            }
            waitpid(pid, nullptr, 0); // Espera o processo ser anexado
            ptrace(PTRACE_SETOPTIONS, pid, nullptr,
                   PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACECLONE); // Configura o ptrace para receber
                                                                 // eventos de sistema e clone
            ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr); // Inicia o tracing do processo PID até a
                                                           // proxíma entrada ou saida de Syscall
            ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr);
            Tracer tracer(pid, false);
            tracer.run();
        }
        else if (result.count("fork"))
        {
            const auto &fork_args = result["fork"].as<std::vector<std::string>>();
            fork_and_trace(fork_args);
        }
    }
    catch (const cxxopts::exceptions::exception &e) // Tratar erro da lib cxxopts
    {
        spdlog::error("Erro ao ler argumentos: {}", e.what());
        return 1;
    }
    catch (const std::exception &e) // Tratar erro genérico do Tracer
    {
        spdlog::error("Um erro desconhecido ocorreu: {}", e.what());
        return 1;
    }
    spdlog::info("Tracing finalizado.");
    spdlog::shutdown();
    return 0;
}