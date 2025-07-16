#pragma once

#include <map>    // Utilizado para o mapa de chamadas de sistema
#include <string> // Utilizado para as strings de nomes e tipos de argumentos das chamadas de sistema
#include <vector> // Utilizado para os vetores dos argumentos das chamadas de sistema

/**
 * @brief Define estruturas e funções para recuperar informações de chamadas de sistema.
 * Este namespace contém as estruturas de dados para armazenar detalhes de syscalls
 * e um mapa global para procurá-las por seu número.
 */
namespace Syscall
{
/**
 * @struct SyscallInfo
 * @brief Armazena informações sobre uma única chamada de sistema.
 * Isso inclui seu nome, o número de argumentos que ela recebe e os tipos desses argumentos.
 */
struct SyscallInfo
{
    /// @brief O nome da chamada de sistema (ex: "read", "write").
    std::string name;
    /// @brief O número de argumentos que a chamada de sistema recebe.
    int arg_count;
    /// @brief Um vetor de strings representando o tipo em estilo C de cada argumento.
    std::vector<std::string> arg_types;
};

/// @brief Um mapa global e constante que mapeia o número da syscall para a sua SyscallInfo
/// correspondente. Este mapa é preenchido externamente e usado para consultar detalhes da syscall.
extern const std::map<long, SyscallInfo> g_syscall_map;

/**
 * @brief Recupera informações para um determinado número de syscall.
 * @param number O número da chamada de sistema a ser procurada.
 * @return Um ponteiro para a struct SyscallInfo se encontrada, caso contrário, nullptr.
 */
inline const SyscallInfo *get_syscall_info(long number)
{
    // Procura pelo número da syscall no mapa global.
    // "const auto" é usado porque o iterador "it" não será modificado após a inicialização.
    const auto it = g_syscall_map.find(number);

    // Se o iterador não estiver no final do mapa, a syscall foi encontrada.
    if (it != g_syscall_map.end())
    {
        // Retorna um ponteiro para o objeto SyscallInfo encontrado.
        return &it->second;
    }
    // Retorna nullptr se o número da syscall não estiver no mapa.
    return nullptr;
}
} // namespace Syscall