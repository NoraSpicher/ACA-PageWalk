# ACA-PageWalk
Repository for ECEN-5593 final project. Will implement page table flattening in the linux kernel. 

## Setup
You will need QEMU installed to use this repository. The repository now contains the linux kernel (with some changes). Originally, we got the kernel from here:
```
wget https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/snapshot/linux-6.15.tar.gz
tar -xzf linux-6.15.tar.gz
```
## Kernel Recompilation Steps
Every time you modify the kernel (as well as initially when you unzip it), you will need to recompile with the following commands. From the `linux-6.15` directory: 

```
make defconfig 
make kvm_guest.config
make -j$(nproc)
```
For convenience, I added `compile.sh`, which runs all the necessary commands and should be ran from the main repo directory. 

## Use
Just run the `run.sh` file to run the kernel with qemu (you will need qemu to be installed). I also added `gdbrun.sh` to link qemu to gdb. It only adds two tags, `-s` and `-S`. 

To work with gdb, use `gdbrun.sh` in one tab. Then, in another tab, run `gdb linux-6.15/vmlinux`. This will start gdb. To link to qemu, type `target remote:1234`. From here, you should be able to use gdb to debug the kernel.

## File System
The file system should be fully set up and hopefully won't need modification. If you do modify it (for example, to add benchmark files), you can update the zip file used by qemu with these commands (run from inside the `initramfs` directory):

```
rm initramfs.cpio.gz
rm ../initramfs.cpi.gz
find . -print0 | cpio --null -ov --format=newc > initramfs.cpio
gzip ./initramfs.cpio
cp ./initramfs.cpio.gz ./../
```

Technically, you should only need the zip file, but the file system isnt too big, so I included it here so that we don't have to rebuild it from scratch if we need to modify it.
