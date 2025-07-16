#pragma once

#include <map>    // Utilizado para o mapa de chamadas de sistema
#include <string> // Utilizado para as strings de nomes e tipos de argumentos das chamadas de sistema
#include <vector> // Utilizado para os vetores dos argumentos das chamadas de sistema

/**
 * @brief Defines structures and functions for retrieving system call information.
 * This namespace contains the data structures for holding syscall details
 * and a global map for looking them up by their number.
 */
namespace Syscall
{
/**
 * @struct SyscallInfo
 * @brief Holds information about a single system call.
 * This includes its name, the number of arguments it takes, and the types of those arguments.
 */
struct SyscallInfo
{
    /// @brief The name of the system call (e.g., "read", "write").
    std::string name;
    /// @brief The number of arguments the system call takes.
    int arg_count;
    /// @brief A vector of strings representing the C-style type of each argument.
    std::vector<std::string> arg_types;
};

/// @brief A global, constant map from the syscall number to its corresponding SyscallInfo.
/// This map is populated externally and used to look up syscall details.
extern const std::map<long, SyscallInfo> g_syscall_map;

/**
 * @brief Retrieves information for a given syscall number.
 * @param number The number of the system call to look up.
 * @return A pointer to the SyscallInfo struct if found, otherwise nullptr.
 */
inline const SyscallInfo *get_syscall_info(long number)
{
    // Searches for the syscall number in the global map.
    // `const auto` is used because the iterator 'it' will not be modified after initialization.
    const auto it = g_syscall_map.find(number);

    // If the iterator is not at the end of the map, the syscall was found.
    if (it != g_syscall_map.end())
    {
        // Returns a pointer to the found SyscallInfo object.
        return &it->second;
    }
    // Returns nullptr if the syscall number is not in the map.
    return nullptr;
}
} // namespace Syscall