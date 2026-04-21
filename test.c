#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>
#include "lwp.h"


int my_fun(void *arg) {
    printf("thread is running!\n");
    return 67;
}

int main() {
    printf("starting main\n");
    lwp_create(my_fun, NULL);
    lwp_start();  // hands control to scheduler
    printf("back in main\n");
    return 0;
}