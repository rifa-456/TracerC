#pragma once

#include <map>
#include <string>
#include <vector>

namespace Syscall
{
struct SyscallInfo
{
    std::string name;
    int arg_count;
    std::vector<std::string> arg_types;
};
extern const std::map<long, SyscallInfo> g_syscall_map;
inline const SyscallInfo *get_syscall_info(long number)
{
    auto it = g_syscall_map.find(number);
    if (it != g_syscall_map.end())
    {
        return &it->second;
    }
    return nullptr;
}
} // namespace Syscall