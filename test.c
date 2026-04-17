#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>

int main() {
    long page_size = sysconf(_SC_PAGE_SIZE);
    printf("Page size: %ld\n", page_size);

    struct rlimit rl;
    getrlimit(RLIMIT_STACK, &rl);
    printf("Stack limit: %ld\n", (long)rl.rlim_cur);

    return 0;
}