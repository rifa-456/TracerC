import os
import re
import sys
PROJECT_ROOT = os.getcwd()
OUTPUT_FILE = os.path.join(PROJECT_ROOT, "src", "SyscallMap.cpp")


def find_kernel_source_path():
    wsl_kernel_path = os.path.expanduser("~/WSL2-Linux-Kernel")
    if os.path.isdir(wsl_kernel_path) and os.path.isfile(os.path.join(wsl_kernel_path, "include/linux/syscalls.h")):
        print(f"Found WSL kernel source at: {wsl_kernel_path}")
        return wsl_kernel_path
    return "/tmp/kernel_files"


def parse_syscall_table(kernel_path):
    syscall_table_path = os.path.join(kernel_path, "arch/x86/entry/syscalls/syscall_64.tbl")
    if not os.path.isfile(syscall_table_path):
        print(f"ERROR: Syscall table not found at {syscall_table_path}", file=sys.stderr)
        return None
    syscalls = {}
    pattern = re.compile(r"^\d+\s+\w+\s+([a-zA-Z0-9_]+)\s+([a-zA-Z0-9_]+)")
    with open(syscall_table_path, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            m = pattern.match(line)
            if m:
                num = int(line.split()[0])
                name = m.group(1)
                entry_point = m.group(2)
                syscalls[name] = {"number": num, "entry_point": entry_point}
    return syscalls


def parse_syscall_signatures(kernel_path):
    signatures_header_path = os.path.join(kernel_path, "include/linux/syscalls.h")
    if not os.path.isfile(signatures_header_path):
        print(f"ERROR: Syscall signatures header not found at {signatures_header_path}", file=sys.stderr)
        return None
    signatures = {}
    pattern = re.compile(r"asmlinkage\s+long\s+([a-zA-Z0-9_]+)\s*\((.*?)\);")
    with open(signatures_header_path, "r") as f:
        content = f.read().replace('\n', ' ')
        matches = pattern.finditer(content)
        for m in matches:
            entry_point = m.group(1)
            arg_string = m.group(2).strip()
            arg_types = []
            if arg_string == "void" or not arg_string:
                arg_count = 0
            else:
                args = arg_string.split(',')
                arg_count = len(args)
                for arg in args:
                    arg_with_spaces = arg.strip().replace('*', ' * ')
                    parts = arg_with_spaces.split()
                    if len(parts) > 1:
                        arg_types.append(" ".join(parts[:-1]))
                    else:
                        arg_types.append(parts[0])
            signatures[entry_point] = {"arg_count": arg_count, "arg_types": arg_types}
    return signatures


def generate_cpp_file(final_syscall_data):
    os.makedirs(os.path.dirname(OUTPUT_FILE), exist_ok=True)
    with open(OUTPUT_FILE, "w") as f:
        f.write("#include \"Syscall.h\"\n")
        f.write("#include <string>\n#include <vector>\n\n")
        f.write("namespace Syscall {\n\n")
        f.write("const std::map<long, SyscallInfo> g_syscall_map = {\n")
        for num, data in sorted(final_syscall_data.items(), key=lambda item: item[0]):
            name = data['name']
            arg_count = data['arg_count']
            arg_types_str = ", ".join([f'"{t}"' for t in data['arg_types']])
            f.write(f'    {{ {num}, {{ "{name}", {arg_count}, {{ {arg_types_str} }} }} }},\n')
        f.write("};\n\n")
        f.write("} // namespace Syscall\n")
    print(f"Successfully generated {OUTPUT_FILE} with {len(final_syscall_data)} syscalls.")


if __name__ == "__main__":
    kernel_source_path = find_kernel_source_path()
    if not kernel_source_path:
        sys.exit(1)
    syscall_table = parse_syscall_table(kernel_source_path)
    if not syscall_table:
        sys.exit(1)
    signatures = parse_syscall_signatures(kernel_source_path)
    if not signatures:
        sys.exit(1)
    final_data = {}
    for name, table_data in syscall_table.items():
        num = table_data['number']
        entry_point = table_data['entry_point']
        sig_data = signatures.get(entry_point)
        if sig_data:
            final_data[num] = {"name": name, "arg_count": sig_data['arg_count'], "arg_types": sig_data['arg_types']}
        else:
            final_data[num] = {"name": name, "arg_count": 6, "arg_types": []}
            print(f"Warning: Could not find signature for {entry_point}. Defaulting to 6 arguments.", file=sys.stderr)
    generate_cpp_file(final_data)
