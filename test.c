#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>
#include "lwp.c"

int my_fun() {
    return 67;
}


int main() {
    lwp_create(my_fun, NULL);
    printf("thread created successfully\n");
    return 0;
}