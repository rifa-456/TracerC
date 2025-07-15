sudo apt install -y libspdlog-dev libcxxopts-dev

rm -rf build && mkdir build && cd build
cmake ..

mkdir -p /home/(USUARIO)/kernel_files/arch/x86/entry/syscalls
mkdir -p /home/(USUARIO)/kernel_files/include/linux

wget -O
/home/(USUARIO)
/kernel_files/arch/x86/entry/syscalls/syscall_64.tbl https://raw.githubusercontent.com/microsoft/WSL2-Linux-Kernel/linux-msft-wsl-5.15.153.1/arch/x86/entry/syscalls/syscall_64.tbl

wget -O
/home/(USUARIO)
/kernel_files/include/linux/syscalls.h https://raw.githubusercontent.com/microsoft/WSL2-Linux-Kernel/linux-msft-wsl-5.15.153.1/include/linux/syscalls.h

kernel_source_path = "/tmp/kernel_files"

./TracerC -f python3 (PROEJCT ROOT PATH FROM /HOME)/scripts/dummy.py
./TracerC -f python3 /home/yordle/CLionProjects/TracerC/scripts/dummy.py

cd build
sudo ./TracerC -a (PID)