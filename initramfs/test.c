#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

int main()
{
    void *addr = (void *)0x700000000000ULL;
    size_t size = 4096;

    //printf("Requesting mmap at %p\n", addr);
    
    void *p = mmap(addr, size,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                   -1, 0);

    if (p == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    //printf("mmap succeeded, touching pages...\n");
    
    while(1){
    char *c = (char *)p;
    c[0] = 1;
    }
//    printf("done\n");
    return 0;
}
