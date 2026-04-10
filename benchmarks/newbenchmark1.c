#include <stdio.h>      // For printf, fprintf
#include <stdlib.h>     // For exit, atol
#include <string.h>     // For snprintf
#include <unistd.h>     // For close, read, write
#include <fcntl.h>      // For open flags
#include <sys/mman.h>   // For mmap, munmap

// We explicitly use 4KB pages since that's the base page size we want to test
#define PAGE_SZ 4096UL

// This is the proc file that triggers the kernel-side page walk
#define PTWALK_RUN_PATH "/proc/ptwalk_run"

// This proc file is used to read back the counters from the kernel
#define PTWALK_SUMMARY_PATH "/proc/ptwalk_summary"

//Used whenever a system call fails.
static void die(const char *msg)
{
    perror(msg);    // prints error string + errno reason
    exit(1);        // terminate program
}

/*
 *
 * mmap() alone only reserves virtual address space.
 * It does NOT guarantee that page table entries exist.
 *
 * if we dont add this pagwewalk may look empty 
 */
static void touch_pages(char *buf, long npages)
{
    long i;

    for (i = 0; i < npages; i++) {
        buf[i * PAGE_SZ] = 1;   // touch exactly one byte per page
    }
}

/*
 * trigger_single_walk()
 *
 * This function communicates with the kernel.
 *
 * It sends:
 *   <start_addr> <npages> single
 *
 * like thius
 *   7fabc1230000 3 single
 *
 * The kernel should:
 *   perform N independent page walks (one per page)
 */
static void trigger_single_walk(unsigned long start_addr, long npages)
{
    int fd;
    char req[128];     // buffer to hold request string
    int len;
    ssize_t rc;

    // formatting as per kernel requirmeents 
    len = snprintf(req, sizeof(req), "%lx %ld single\n", start_addr, npages);

    // Check if formatting failed or overflowed buffer
    if (len < 0 || len >= (int)sizeof(req)) {
        fprintf(stderr, "the formatting you have entered is wrong\n");
        exit(1);
    }

    // Open proc file for writing 
    fd = open(PTWALK_RUN_PATH, O_WRONLY);
    if (fd < 0)
        die("open /proc/ptwalk_run");

    // Send request to kernel
    rc = write(fd, req, (size_t)len);
    if (rc < 0)
        die("write /proc/ptwalk_run");

    // Ensure full request was written
    if (rc != len) {
        fprintf(stderr, "short write to %s\n", PTWALK_RUN_PATH);
        close(fd);
        exit(1);
    }

    close(fd);   // done with proc file
}

/*
 *
 * Reads back the kernel counters from:
 *   /proc/ptwalk_summary
 *
 * This is what we compare against our expected values.
 */
static void print_summary(void)
{
    int fd;
    char buf[1024];
    ssize_t n;

    // Open summary proc file
    fd = open(PTWALK_SUMMARY_PATH, O_RDONLY);
    if (fd < 0)
        die("open /proc/ptwalk_summary");

    // Read contents - this is where our kernel counters we incremented gets prited out 
    n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0)
        die("read /proc/ptwalk_summary");

    buf[n] = '\0';  

    printf("\nKernel summary:\n%s\n", buf);

    close(fd);
}

int main(int argc, char *argv[])
{
    long npages;
    size_t len;
    char *region;

    // Expect exactly 1 argument: number of pages
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <num_pages>\n", argv[0]);
        fprintf(stderr, "Example: %s 1\n", argv[0]);
        fprintf(stderr, "Example: %s 3\n", argv[0]);
        return 1;
    }

    // Convert input string to number
    npages = atol(argv[1]);

    // Validate input
    if (npages <= 0) {
        fprintf(stderr, "num_pages must be > 0\n");
        return 1;
    }

    // Total bytes to map
    len = (size_t)npages * PAGE_SZ;

    /*
     * Created a simple anonymous mapping of the requested size.
     */
    region = mmap(NULL, len,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS,
                  -1, 0);

    if (region == MAP_FAILED)
        die("mmap");

    // Force page table creation for each page
    touch_pages(region, npages);

    // Print info about this run
    printf("Benchmark request:\n");
    printf("  start address : 0x%lx\n", (unsigned long)region);
    printf("  num pages     : %ld\n", npages);

    /*
     * This is our ground truth:
     *
     * Since we will do N independent page walks,
     * each level must be visited exactly N times.
     */
    printf("\nGround truth expected for SINGLE mode:\n");
    printf("  pgd = %ld\n", npages);
    printf("  p4d = %ld\n", npages);
    printf("  pud = %ld\n", npages);
    printf("  pmd = %ld\n", npages);
    printf("  pte = %ld\n", npages);

    /*
     * Ask kernel to perform the page walks.
     *
     * Kernel will:
     *   for each page:
     *       do full PGD→PTE descent
     */
    trigger_single_walk((unsigned long)region, npages);

    /*
     * Now read back what the kernel counted.
     * This is what we compare with ground truth.
     */
    print_summary();

    // Clean up mapping
    if (munmap(region, len) != 0)
        die("munmap");

    return 0;
}