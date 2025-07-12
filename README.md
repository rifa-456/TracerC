find src include -name "*.cpp" -o -name "*.h" | xargs clang-format -i
sudo apt install -y libspdlog-dev libcxxopts-dev

rm -rf build && mkdir build && cd build
cmake ..
make -j$(nproc)

sudo apt update
sudo apt install linux-source
sudo apt-get install libclang-dev

cd /usr/src
sudo tar xjf linux-source-*.tar.bz2

cd /usr/src/linux-source-6.8.0
sudo make defconfig
sudo make prepare

mkdir -p /tmp/kernel_files/arch/x86/entry/syscalls
mkdir -p /tmp/kernel_files/include/linux

wget -O
/tmp/kernel_files/arch/x86/entry/syscalls/syscall_64.tbl https://raw.githubusercontent.com/microsoft/WSL2-Linux-Kernel/linux-msft-wsl-5.15.153.1/arch/x86/entry/syscalls/syscall_64.tbl
wget -O
/tmp/kernel_files/include/linux/syscalls.h https://raw.githubusercontent.com/microsoft/WSL2-Linux-Kernel/linux-msft-wsl-5.15.153.1/include/linux/syscalls.h

kernel_source_path = "/tmp/kernel_files"