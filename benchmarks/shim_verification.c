#include <stdio.h>
#include <sys/mman.h>
#include <string.h>

int main() {
    // Map to the EXACT address we used in the kernel trigger
    void *ptr = mmap((void *)0x7ffff7ff0000, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
    if (ptr == MAP_FAILED) { perror("mmap"); return 1; }

    printf("Triggering verification at 0x7ffff7ff0000...\n");
    strcpy(ptr, "Hello Flattened World"); // This triggers the page fault
    printf("Check dmesg for the verification report!\n");
    return 0;
}
