# TracerC: Um Rastreador de Chamadas de Sistema para Linux

TracerC é uma ferramenta de linha de comando para Linux, desenvolvida em C++, que utiliza a chamada de sistema `ptrace`
para monitorar e registrar as chamadas de sistema (syscalls) realizadas por um ou mais processos em tempo real. Ele pode
tanto se anexar a um processo já em execução (e todos os seus descendentes) quanto iniciar um novo processo e rastreá-lo
desde o início.

O projeto foi desenhado para ser uma ferramenta de depuração e análise, fornecendo logs detalhados sobre a interação dos
processos com o kernel do Linux.

---

## 1. Pré-requisitos e Instalação

### Ambiente

Este projeto foi desenvolvido e testado exclusivamente em ambientes Linux (x86-64). A compatibilidade com outros
sistemas operacionais não é garantida.

### Dependências

TracerC depende de duas bibliotecas externas:

- `spdlog`: Para logging de alta performance.
- `cxxopts`: Para parsing de argumentos de linha de comando.

Para instalar essas dependências em sistemas baseados em Debian/Ubuntu, utilize o seguinte comando:

```bash
sudo apt update
sudo apt install -y libspdlog-dev libcxxopts-dev
```

## 2. Gerando o Mapa de Syscalls (Opcional)

O TracerC utiliza um mapa estático (`SyscallMap.cpp`) para traduzir números de syscalls em nomes e informações de
argumentos. Uma versão pré-gerada deste arquivo já está incluída no diretório `/src`.

No entanto, se desejar gerar este mapa manualmente (por exemplo, para uma versão de kernel diferente), você pode usar o
script Python fornecido. Para isso, primeiro baixe os arquivos de cabeçalho relevantes do código-fonte do kernel Linux:

1. **Crie os diretórios necessários:**
   ```bash
   mkdir -p ~/kernel_files/arch/x86/entry/syscalls
   mkdir -p ~/kernel_files/include/linux
   ```

2. **Baixe a tabela de syscalls e os cabeçalhos:**
   ```bash
   wget -O ~/kernel_files/arch/x86/entry/syscalls/syscall_64.tbl https://raw.githubusercontent.com/torvalds/linux/master/arch/x86/entry/syscalls/syscall_64.tbl

   wget -O ~/kernel_files/include/linux/syscalls.h https://raw.githubusercontent.com/torvalds/linux/master/include/linux/syscalls.h
   ```

3. **Execute o script de geração:**
   O script `scripts/generate_syscall_map.py` está configurado para ler os arquivos a partir do diretório
   `~/kernel_files` e gerar o `src/SyscallMap.cpp`.
   ```bash
   python3 scripts/generate_syscall_map.py
   ```

---

## 3. Como Buildar

O projeto utiliza `CMake` para a sua compilação. Siga os passos abaixo a partir da raiz do repositório:

1. **Crie e acesse um diretório de build:**
   ```bash
   rm -rf build && mkdir build && cd build
   ```

2. **Execute o CMake para gerar os Makefiles:**
   ```bash
   cmake ..
   ```

3. **Compile o projeto:**
   Para uma compilação mais rápida, utilize todos os cores de CPU disponíveis.
   ```bash
   make -j$(nproc)
   ```
   O executável `TracerC` será gerado dentro do diretório `build`.

---

## 4. Como Rodar

A ferramenta requer privilégios de superusuário (`sudo`) para utilizar `ptrace`. Existem duas formas principais de uso:

### Modo de Anexação (`--attach` ou `-a`)

Anexa o rastreador a um processo existente pelo seu PID. O TracerC irá automaticamente identificar e rastrear todas as
threads e processos filhos subsequentes.

**Uso:**

```bash
# A partir do diretório de build
sudo ./TracerC -a <PID>
```

*Substitua `<PID>` pelo ID do processo que deseja monitorar.*

### Modo de Fork (`--fork` ou `-f`)

Inicia um novo comando, o rastreando desde o seu lançamento. Este é o modo padrão se nenhum outro for especificado.

**Uso:**

```bash
# A partir do diretório de build
sudo ./TracerC -f <comando> [argumentos...]
```

**Exemplo com o script de teste `dummy.py`:**
O repositório inclui um script Python para fins de demonstração que cria um processo filho e uma thread.

```bash
# A partir do diretório de build, execute:
sudo ./TracerC -f python3 ../scripts/dummy.py
```

Os logs de rastreamento serão exibidos no console (`INFO` e acima) e salvos em um arquivo de log detalhado (`TRACE` e
acima) no diretório `/logs` com um timestamp no nome.

---

## 5. Estrutura do Projeto

O repositório está organizado da seguinte forma:

```
.
├── build/                  # (Criado após a compilação) Contém os arquivos de build e o executável.
│   ├── logs/               # Pasta de logs ficarão no mesmo diretorio do executavel.
│   │   └── trace-{...}.log        
│   └── TracerC             # Executável.
├── include/
│   ├── Syscall.h           # Define as estruturas de dados para informações de syscalls.
│   └── Tracer.h            # Declaração da classe Tracer e da função fork_and_trace.
├── src/
│   ├── main.cpp            # Ponto de entrada, parsing de argumentos e configuração inicial.
│   ├── Tracer.cpp          # Implementação da lógica de rastreamento com ptrace.
│   └── SyscallMap.cpp      # (Auto-gerado) Mapa global de números para informações de syscalls.
├── scripts/
│   ├── dummy.py            # Script Python para teste, cria processos e threads.
│   └── generate_syscall_map.py # Script Python para gerar o SyscallMap.cpp.
├── .gitignore              # Arquivos e diretórios ignorados pelo Git.
├── CMakeLists.txt          # Arquivo de configuração de build para o CMake.
└── README.md               # Este arquivo.
```