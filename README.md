sudo apt install -y libspdlog-dev libcxxopts-dev

rm -rf build && mkdir build && cd build
cmake ..

mkdir -p ~/kernel_files/arch/x86/entry/syscalls
mkdir -p ~/kernel_files/include/linux

wget -O ~
/kernel_files/arch/x86/entry/syscalls/syscall_64.tbl https://raw.githubusercontent.com/torvalds/linux/refs/heads/master/arch/x86/entry/syscalls/syscall_64.tbl

wget -O ~
/kernel_files/include/linux/syscalls.h https://raw.githubusercontent.com/torvalds/linux/refs/heads/master/include/linux/syscalls.h

kernel_source_path = "/tmp/kernel_files"

./TracerC -f python3 (PROEJCT ROOT PATH FROM /HOME)/scripts/dummy.py
./TracerC -f python3 /home/yordle/CLionProjects/TracerC/scripts/dummy.py

cd build
sudo ./TracerC -a (PID)