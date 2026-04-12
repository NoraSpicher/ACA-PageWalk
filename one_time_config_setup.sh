cd ./linux-6.15/
make defconfig
make kvm_guest.config
./scripts/config -e DEBUG_KERNEL \
                 -e DEBUG_INFO \
                 -e DEBUG_INFO_DWARF_TOOLCHAIN_DEFAULT \
                 -e GDB_SCRIPTS
make olddefconfig
make -j$(nproc)
cd ..

