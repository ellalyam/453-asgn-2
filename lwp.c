#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include "lwp.h"
#include "fp.h"

/* SCHEDULER */
/* sched_one is PREVIOUS
   sched_two is NEXT */
static thread head = NULL;

void init() {
    /* don't need? */
}

void shutdown() {
    /* don't need? */
}

void admit(thread new) {
    if(head == NULL) {
        return;
    }

    thread old_end = head->sched_one;
    head->sched_one = new;
    new->sched_one = old_end;
    new->sched_two = head;
    old_end->sched_two = new;
}

void t_remove(thread victim) {
    thread current = head;

    while(current != head) {
        if(current == victim) {
            thread left = victim->sched_one;
            thread right = victim->sched_two;
            left->sched_two = right;
            right->sched_one = left;

            victim->sched_one = NULL;
            victim->sched_two = NULL;

            break;
        }
        current = current->sched_two;
    }

    /* add error for if reaching end of list without finding */
}

thread next() {
    if(head == NULL) {
        return NULL;
    }

    head = head->sched_two;
    return head->sched_one;   
}

int qlen() {
    /* count var
    count as traversing
    return count*/

    int count = 1;
    thread current = head->sched_two;

    while(current != head) {
        count = count + 1;
    }

    return count;
}

struct scheduler sched = {NULL, NULL, &admit, &t_remove, &next, &qlen};
static scheduler current_sched = &sched;

/* LWP */
static long thread_id = 1;

static void lwp_wrap(lwpfun fun, void *arg) {
    /* Call the given lwpfunction with the given argument.
    * Calls lwp exit() with its return value
    */
    int rval;
    rval=fun(arg);
    lwp_exit(rval);
}

tid_t lwp_create(lwpfun function, void *argument) {
    /* Create the context */
    thread cont = malloc(sizeof(context));

    /* Create the stack */
    struct rlimit rl;
    getrlimit(RLIMIT_STACK, &rl);
    long stack_size = rl.rlim_cur;
    if(stack_size == RLIM_INFINITY) {
        stack_size = 8388608;
    }

    void *stack_base = mmap(NULL, stack_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK, -1, 0);

    if(stack_base==MAP_FAILED) {
        /* Deal with error */
        return NO_THREAD;
    }

    /* Fake stack frame */
    cont->stacksize = stack_size;
    cont->stack = stack_base;

    unsigned long *stack_top = stack_base + stack_size;

    stack_top[-1] = (unsigned long)lwp_wrap; /* return address, calls actual function */
    stack_top[-2] = 0; /* fake base pointer */

    /* setting new thread */
    cont->state.rbp = 0; /* any values, will be teared down anyways */
    cont->state.rsp = 0;
    cont->state.rdi = (unsigned long)function;
    cont->state.rsi = (unsigned long)argument;
    cont->tid = thread_id;
    thread_id = thread_id + 1;
    /* Need to set anything else? */

    /* admit thread to scheduler */
    current_sched->admit(cont);

    return cont->tid;
}

void lwp_start() {
    /* */
}

void lwp_yield() {
    /* */
}

void lwp_exit(int exitval) {
    /* */
}

tid_t lwp_wait(int *status) {
    /* */
    return;
}

tid_t lwp_gettid() {
    /* */
    return;
}

thread tid2thread(tid_t tid) {
    /* */
    return;
}

void lwp_set_scheduler(scheduler sched) {
    /* */
}

scheduler lwp_get_scheduler() {
    /* */
    return;
}