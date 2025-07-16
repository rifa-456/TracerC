sudo apt install -y libspdlog-dev libcxxopts-dev

mkdir -p ~/kernel_files/arch/x86/entry/syscalls
mkdir -p ~/kernel_files/include/linux

wget -O ~
/kernel_files/arch/x86/entry/syscalls/syscall_64.tbl https://raw.githubusercontent.com/torvalds/linux/refs/heads/master/arch/x86/entry/syscalls/syscall_64.tbl

wget -O ~
/kernel_files/include/linux/syscalls.h https://raw.githubusercontent.com/torvalds/linux/refs/heads/master/include/linux/syscalls.h

rm -rf build && mkdir build && cd build
cmake ..
make -j$(nproc)

cd build

sudo ./TracerC -a (PID)
sudo ./TracerC -f python3 ~/CLionProjects/TracerC/scripts/dummy.py