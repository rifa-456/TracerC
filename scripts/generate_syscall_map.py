import os
import re
from pathlib import Path

SCRIPT_PATH = Path(__file__).resolve()
PROJECT_ROOT = SCRIPT_PATH.parent.parent
HOME_PATH = Path.home()
KERNEL_SOURCE_PATH = HOME_PATH / "kernel_files"
OUTPUT_FILE = PROJECT_ROOT / "src" / "SyscallMap.cpp"
KERNEL_SOURCE_PATH = str(KERNEL_SOURCE_PATH)
OUTPUT_FILE = str(OUTPUT_FILE)


def parse_syscall_table(kernel_path):
    """Analisa a tabela de syscalls de um kernel Linux e retorna um dicionário com os detalhes.

    Args:
        kernel_path (str): O caminho para o diretório do código-fonte do kernel Linux.

    Returns:
        dict: Um dicionário onde as chaves são os nomes das syscalls e os valores são
              dicionários contendo o número e o ponto de entrada da syscall.
    """
    syscall_table_path = os.path.join(kernel_path,
                                      "arch/x86/entry/syscalls/syscall_64.tbl")  # Caminho da tabela de syscalls do linux
    # Exemplo do arquivo syscall_64.tbl:
    # ...
    # 0	common	read			sys_read
    # 1	common	write			sys_write
    # ...
    syscalls = {}
    pattern = re.compile(
        r"^\d+\s+(\w+)\s+([a-zA-Z0-9_]+)\s+([a-zA-Z0-9_]+)")  # Regex que pegas os nomes e pontos de entrada das syscalls do arquivo syscall_64.tbl
    #
    # ^: Faz com que o regex só analise linhas novas
    # \d+: Procura por um ou mais números, o número da syscall
    # \s+: Procura por um ou mais espaços, o primeiro espaço
    # (\w+): Captura uma palavra (letras, números ou underscore), no exemplo "common", a ABI da chamada
    # \s+: Procura por mais um ou mais espaços, o segundo espaço
    # ([a-zA-Z0-9_]+): Captura o nome da syscall, que pode conter letras, números ou underscore
    # \s+: Procura por mais um ou mais espaços, o terceiro espaço
    # ([a-zA-Z0-9_]+): Captura o ponto de entrada da syscall, que também pode conter letras, números ou underscore

    with open(syscall_table_path, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):  # Ignora linhas vazias ou comentários no .tbl
                continue
            m = pattern.match(line)  # Verifica se a linha sendo analisada bate com o regex
            if m:

                # Ignorar syscalls de sistema x32 para não entrar em conflito com as atuais e mais utilizadas
                abi = m.group(1)
                if abi == 'x32':
                    continue

                num = int(line.split()[0])  # Pega o número da syscall, que é o primeiro elemento da linha
                name = m.group(2)  # Pega o nome da syscall, que é o segundo grupo do regex
                entry_point = m.group(3)  # Pega o ponto de entrada da syscall, que é o terceiro grupo do regex
                if name not in syscalls:
                    syscalls[name] = {"number": num,
                                      "entry_point": entry_point}  # Adiciona o nome, número e ponto de entrada da syscall no dicionário syscalls
    return syscalls


def parse_syscall_signatures(kernel_path):
    """Analisa o arquivo de cabeçalho syscalls.h para extrair as assinaturas das chamadas de sistema.

    Args:
        kernel_path (str): O caminho para o diretório do código-fonte do kernel Linux.

    Returns:
        dict: Um dicionário onde as chaves são os pontos de entrada das syscalls e os
              valores são dicionários contendo a contagem ('arg_count') e os tipos
              ('arg_types') dos argumentos da respectiva syscall.
    """
    signatures_header_path = os.path.join(kernel_path,
                                          "include/linux/syscalls.h")  # Caminho do arquivo de assinaturas de syscalls
    # Exemplo do arquivo syscalls.h:
    # ...
    # /*
    # * These syscall function prototypes are kept in the same order as
    # * include/uapi/asm-generic/unistd.h. Architecture specific entries go below,
    # * followed by deprecated or obsolete system calls.
    # *
    # * Please note that these prototypes here are only provided for information
    #     * purposes, for static analysis, and for linking from the syscall table.
    # * These functions should not be called elsewhere from kernel code.
    # *
    # * As the syscall calling convention may be different from the default
    # * for architectures overriding the syscall calling convention, do not
    # * include the prototypes if CONFIG_ARCH_HAS_SYSCALL_WRAPPER is enabled.
    # */
    # #ifndef CONFIG_ARCH_HAS_SYSCALL_WRAPPER
    # asmlinkage long sys_io_setup(unsigned nr_reqs, aio_context_t __user *ctx);
    # asmlinkage long sys_io_destroy(aio_context_t ctx);
    # ...

    signatures = {}
    pattern = re.compile(r"asmlinkage\s+long\s+([a-zA-Z0-9_]+)\s*\((.*?)\);",
                         re.DOTALL)  # re.DOTALL: Modificador que faz o `.` no regex corresponder também a quebras de linha.
    # asmlinkage: Procura pela palavra "asmlinkage"
    # \s+: Procura por um ou mais espaços.
    # long: Procura pela palavra "long", que é o tipo de retorno do protótipo.
    # \s+: Procura por mais um ou mais espaços.
    # ([a-zA-Z0-9_]+): Captura o nome da função (o ponto de entrada da syscall), que pode conter letras, números ou underscore.
    # \s*: Procura por zero ou mais espaços.
    # \(: Procura pelo caractere "(".
    # (.*?): Captura (de forma não gulosa) qualquer caractere, os argumentos e tipos do protótipo.
    # \): Procura pelo caractere ")".
    # ;: Procura pelo caractere ";".

    with open(signatures_header_path, "r") as f:
        content = f.read()
        matches = pattern.finditer(content)
        for m in matches:  # Itera sobre cada correspondência (protótipo de função) encontrada.
            entry_point = m.group(1)  # Pega o nome da função (ponto de entrada), que é o primeiro grupo do regex.

            arg_string = m.group(2).replace('\n',
                                            ' ').strip()  # Pega os argumentos (segundo grupo), substitui quebras de linha por espaços e remove espaços no início/fim.
            arg_types = []
            if arg_string == "void" or not arg_string:
                arg_count = 0  # Se a string de argumentos for "void" ou vazia, define a contagem de argumentos como 0.
            else:
                args = arg_string.split(',')
                arg_count = len(args)
                for arg in args:
                    arg_with_spaces = arg.strip().replace('*',
                                                          ' * ')  # Adiciona espaços ao redor de ponteiros (*) para facilitar a separação do tipo.

                    parts = arg_with_spaces.split()  # Divide o argumento em partes (tipo e nome).

                    if len(parts) > 1:
                        arg_types.append(
                            " ".join(parts[:-1]))  # Pega o tipo do argumento (tudo menos a última parte, que é o nome).

                    elif parts:
                        arg_types.append(parts[0])  # Se tiver apenas uma parte, ela é o tipo.

            signatures[entry_point] = {"arg_count": arg_count,
                                       "arg_types": arg_types}  # Armazena a contagem e os tipos de argumentos no dicionário, usando o nome da função como chave.
    return signatures


def generate_cpp_file(final_syscall_data):
    """Gera um arquivo C++ que define um mapa de informações de chamadas de sistema.

    Esta função cria o arquivo no caminho definido pelas constante OUTPUT_FILE. O arquivo gerado
    contém um std::map que mapeia o número da syscall para uma estrutura contendo o nome,
    a contagem de argumentos e os tipos dos argumentos da syscall.

    Args:
        final_syscall_data (dict): Um dicionário com os dados das syscalls,
                                   onde as chaves são os números das syscalls.
    """
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
    print(f"{OUTPUT_FILE} gerado com sucesso, foram encontradas {len(final_syscall_data)} chamadas de sistema.")


if __name__ == "__main__":
    kernel_source_path = KERNEL_SOURCE_PATH
    syscall_table = parse_syscall_table(kernel_source_path)
    signatures = parse_syscall_signatures(kernel_source_path)
    final_data = {}
    for name, table_data in syscall_table.items():
        num = table_data['number']
        entry_point = table_data['entry_point']
        sig_data = signatures.get(entry_point)
        if sig_data:
            final_data[num] = {"name": name, "arg_count": sig_data['arg_count'], "arg_types": sig_data['arg_types']}
        else:
            final_data[num] = {"name": name, "arg_count": 6,
                               "arg_types": []}  # Caso não encontre a assinatura, assume 6 argumentos e tipos vazios
    generate_cpp_file(final_data)
