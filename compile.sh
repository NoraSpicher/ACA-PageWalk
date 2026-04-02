cd ./linux-6.15/
make defconfig
make kvm_guest.config
make -j$(nproc)
cd ..

