#include <stdio.h>     
#include <stdlib.h>    
#include <string.h>    
#include <unistd.h>     
#include <fcntl.h>      
#include <sys/mman.h>   
#include <errno.h>     

// We explicitly use 4KB pages since that is the base page size we want to test
#define PAGE_SZ 4096UL

// The proc file is what we write to in order to trigger the kernelside walk
#define PTWALK_RUN_PATH "/proc/ptwalk_run"

// The proc file is what we read from in order to fetch the kernel counters
#define PTWALK_SUMMARY_PATH "/proc/ptwalk_summary"

/*
 * I'm storing the  expected ground-truth values in one struct so that
 * both modes single and range can use the same print path.
 */
struct expected_counts {
    unsigned long pgd;
    unsigned long p4d;
    unsigned long pud;
    unsigned long pmd;
    unsigned long pte;
};

//whenever system cal fails we call this function to print the error and exit
static void die(const char *msg)
{
    perror(msg);
    exit(1);
}

/*
 * this function is to ensure that the mode entered isnt something unrecognised 
 *
 * We have only two benchmark modes:
 *
 *   1) single
 *      Kernel should do N independent descents, one per page
 *
 *   2) range
 *      Kernel should do one walk_page_range() over the whole mapped interval
 *
 *  1 means the mode is valid.
 * 0 means the user typed something unsupported.
 */
static int validate_mode(const char *mode)
{
    if (strcmp(mode, "single") == 0)
        return 1;

    if (strcmp(mode, "range") == 0)
        return 1;

    return 0;
}

/*
 * touch_pages()
 
 *
 * By touching one byte in each page, we force page faults to happen and
 * ensure that the mapping is actually backed by normal page-table entries.
 *
 * Without this step, the pagewalk may observe no entries so our calculations wont be valdi 
 */
static void touch_pages(char *buf, long npages)
{
    long i;

    for (i = 0; i < npages; i++) {
        buf[i * PAGE_SZ] = 1;   // touch exactly one byte per page
    }
}

/*
 *
 * to compute the exact ground truth for range mode.
 *
 * For a continuous virtual address interval [start, end), we want to know
 * how many distinct entries at a given page-table level are touched.
 *
 * We do that by shifting the addresses right by the level-specific shift:
 *
 *   PTE uses shift 12  -> 4KB granularity
 *   PMD uses shift 21  -> 2MB granularity
 *   PUD uses shift 30  -> 1GB granularity
 *   P4D uses shift 39
 *   PGD uses shift 48
 *
 * Example:
 *   if start and end fall inside the same PMD-sized region,
 *   then PMD count will be 1
 *
 *   if they cross into another PMD-sized region,
 *   then PMD count becomes 2, and so on.
 */
static unsigned long count_distinct_entries(unsigned long start,
                                            unsigned long end,
                                            unsigned int shift)
{
    unsigned long first;
    unsigned long last;

    /*
     * start >> shift gives the first entry index touched at that level
     * (end - 1) >> shift gives the last entry index touched at that level
     *
     * so total number of distinct entries covered is:
     *   last - first + 1
     */
    first = start >> shift;
    last  = (end - 1) >> shift;

    return last - first + 1;
}

/*
 * this is for the single mode, the kernel will perform N independent descents.
 *
 * That means:
 *   page 0 -> one PGD/P4D/PUD/PMD/PTE path
 *   page 1 -> again one full path
 *   ...
 *
 * So every level must be touched exactly N times.
 */
static struct expected_counts compute_expected_single(long npages)
{
    struct expected_counts exp;

    exp.pgd = (unsigned long)npages;
    exp.p4d = (unsigned long)npages;
    exp.pud = (unsigned long)npages;
    exp.pmd = (unsigned long)npages;
    exp.pte = (unsigned long)npages;

    return exp;
}

/*

 * In range mode, the kernel is expected to walk one continuous  interval:
 *
 *   [start, end)
 *
 * Here the counts are NOT all equal.
 * Instead, each count is the number of distinct entries touched at that level.
 *
 * Small ranges often give:
 *   pgd = 1
 *   p4d = 1
 *   pud = 1
 *   pmd = 1
 *   pte = N
 *
 * But large ranges can cross PMD/PUD/P4D/PGD boundaries, and then the
 * upper-level counts increase too. This function computes that exactly.
 */
static struct expected_counts compute_expected_range(unsigned long start,
                                                     unsigned long end)
{
    struct expected_counts exp;

    exp.pte = count_distinct_entries(start, end, 12);
    exp.pmd = count_distinct_entries(start, end, 21);
    exp.pud = count_distinct_entries(start, end, 30);
    exp.p4d = count_distinct_entries(start, end, 39);
    exp.pgd = count_distinct_entries(start, end, 48);

    return exp;
}

/*this prints the ground-truth values we computed locally in userspace.
 * These are the values that should match the kernel summary if the
 * counters in pagewalk.c are correctly placed.
 */
static void print_expected_counts(const char *mode,
                                  const struct expected_counts *exp)
{
    printf("\nGround truth expected for %s mode:\n", mode);
    printf("  pgd = %lu\n", exp->pgd);
    printf("  p4d = %lu\n", exp->p4d);
    printf("  pud = %lu\n", exp->pud);
    printf("  pmd = %lu\n", exp->pmd);
    printf("  pte = %lu\n", exp->pte);
}

/*
 *
 * the actual function that  communicates with the kernel.
 *
 * It writes one line to /proc/ptwalk_run in this exact format:
 *
 *   <start_addr_hex> <npages> <mode>
 *
 * Example:
 *   7fabc1234000 3 single
 *   7fabc1234000 4096 range
 *
 * The kernel-side ptwalk_run handler is expected to see  this line and
 * run the requested page-table walk.
 */
static void trigger_walk(unsigned long start_addr, long npages, const char *mode)
{
    int fd;
    char req[128];
    int len;
    ssize_t rc;

    
     //formatting the string correctly .
    
    len = snprintf(req, sizeof(req), "%lx %ld %s\n",
                   start_addr, npages, mode);

    
    // avoduing sernding th emalformed line to proc fie 
    if (len < 0 || len >= (int)sizeof(req)) {
        fprintf(stderr, "request formatting failed\n");
        exit(1);
    }

    //Open the proc entry for writing.
     //Writing to it is what triggers the kernel-side logic.
     
    fd = open(PTWALK_RUN_PATH, O_WRONLY);
    if (fd < 0)
        die("open /proc/ptwalk_run");

    
    //Send the request to the kernel.
     
    rc = write(fd, req, (size_t)len);
    if (rc < 0)
        die("write /proc/ptwalk_run");

    //we need to treat a short write as an error 
    if (rc != len) {
        fprintf(stderr, "short write to %s\n", PTWALK_RUN_PATH);
        close(fd);
        exit(1);
    }

    close(fd);
}

/*
 * Reads back whatever the kernel has printed into /proc/ptwalk_summary.
 *
 *we need to copare this to the groubd trueth 
 */
static void print_summary(void)
{
    int fd;
    char buf[2048];
    ssize_t n;

    //Open the proc file that exposes the kernel counters.
    
    fd = open(PTWALK_SUMMARY_PATH, O_RDONLY);
    if (fd < 0)
        die("open /proc/ptwalk_summary");

    // Read the summary string from the kernel.
     
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
    const char *mode;
    size_t len;
    char *region;
    unsigned long start_addr;
    unsigned long end_addr;
    struct expected_counts exp;

    /*
     * We expect exactly two user arguments:
     *
     *  1st v= number of pages
     *   2nd v = mode ("single" or "range")
     */
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_pages> <single|range>\n", argv[0]);
        fprintf(stderr, "Example: %s 3 single\n", argv[0]);
        fprintf(stderr, "Example: %s 3 range\n", argv[0]);
        fprintf(stderr, "Example: %s 4096 range\n", argv[0]);
        return 1;
    }

    // Convert the page count from string to integer.
    
    npages = atol(argv[1]);
    mode = argv[2];

    //Reject nonsensical input early.
    if (npages <= 0) {
        fprintf(stderr, "num_pages must be > 0\n");
        return 1;
    }

    
    if (!validate_mode(mode)) {
        fprintf(stderr, "mode must be either 'single' or 'range'\n");
        return 1;
    }

    //Total size of the mapping in bytes.

    len = (size_t)npages * PAGE_SZ;
    if (len > 0x40000000UL) {
    fprintf(stderr, "mapping exceeds allowed test range\n");
    exit(1);
    }
    //simple anonymous private mapping
    unsigned long fixed_addr = 0x700000000000UL;

    region = mmap((void *)fixed_addr,
              len,
              PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
              -1,
              0);

    if (region == MAP_FAILED)
        die("mmap(fixed range failed)");

    

    /*
     * Force every page in the region to be materialized so that the
     * kernel-side page walk sees real mappings with real page-table entries.
     */
    touch_pages(region, npages);

    //Record the exact virtual range that will be tested.
     
    start_addr = (unsigned long)region;
    end_addr   = start_addr + len;

    /*
     * Compute the exact expected values for the selected benchmark mode.
     *
     * single:
     *   exact per-page repeated descents
     *
     * range:
     *   exact number of distinct entries touched at each level across the span
     */
    if (strcmp(mode, "single") == 0)
        exp = compute_expected_single(npages);
    else
        exp = compute_expected_range(start_addr, end_addr);

    /*
     * Print the request details so you know exactly what region was used.
     */
    printf("Benchmark request:\n");
    printf("  start address : 0x%lx\n", start_addr);
    printf("  end address   : 0x%lx\n", end_addr);
    printf("  num pages     : %ld\n", npages);
    printf("  mode          : %s\n", mode);

    /*
     * Print the benchmark's own ground-truth calculation before asking
     * the kernel to perform the walk.
     */
    print_expected_counts(mode, &exp);

    /*
     * Ask the kernel to execute the requested walk mode.
     */
    trigger_walk(start_addr, npages, mode);

    /*
     * Read back what the kernel counted.
     * This is what you compare to the expected values above.
     */
    print_summary();

    /*
     * Release the userspace mapping once the experiment is complete.
     */
    if (munmap(region, len) != 0)
        die("munmap");

    return 0;
}