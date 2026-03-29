# ACA-PageWalk
Repository for ECEN-5593 final project. Will implement page table flattening in the linux kernel. 

## Setup
You will need QEMU installed to use this repository. Also, you will need to acquire a linux kernel! We are using 6.15, from the commands below. If you use a different version or get one from a different source, then you will need to edit either directory names or the `run.sh` file.
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

## Use
Just run the `run.sh` file to run the kernel with qemu (you will need qemu to be installed). I also added `runwgdb.sh` to link qemu to gdb. It only adds two tags, `-s` and `-S`. 

## File System
The file system should be fully set up and hopefully won't need modification. If you do modify it, you can update the zip file used by qemu with these commands (run from inside the `initramfs` directory):

```
find . -print0 | cpio --null -ov --format=newc > initramfs.cpio
gzip ./initramfs.cpio
cp ./initramfs.cpio.gz ./../
```

Technically, you should only need the zip file, but the file system isnt too big, so I included it here so that we don't have to rebuild it from scratch if we need to modify it.

to do:
gitignore ??
make readme pretty 