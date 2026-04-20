#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include "lwp.h"
#include "fp.h"

#define LWP_LIVE
#define LWP_TERM
#define MKTERMSTAT(s,v)
#define LWPTERMINATED(s)
#define LWPTERMSTAT(s)

/* THINK THESE FUNCTIONS ARE MISSING STUFF.
   DOUBLE CHECK INSTRUCTIONS */
/* CHECK STACK */
/* CHECK SCHEDULER IS BEING PROPERLY UPDATED */
/* CHECK TID INCREMENTING? */


/* SCHEDULER */
/* sched_one is PREVIOUS, sched_two is NEXT */
static thread head = NULL;
static thread current_thread = NULL;

static thread terminated_head = NULL;
static thread terminated_tail = NULL;
static thread waiting_head = NULL;
static thread waiting_tail = NULL;

void t_init() {
    /* don't need? */
}

void t_shutdown() {
    /* don't need? */
}

void t_admit(thread new) {
    if(head == NULL) {
        new->sched_one = new;
        new->sched_two = new;
        head = new;
    } else {
        thread old_end = head->sched_one;
        head->sched_one = new;
        new->sched_one = old_end;
        new->sched_two = head;
        old_end->sched_two = new;
    }
}

void t_remove(thread victim) {
    thread current = head->sched_two;

    do {
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
    } while(current != head);

    /* add error for if reaching end of list without finding */
}

thread next() {
    if(head == NULL) {
        return NULL;
    }

    thread next_thread = head;
    head = head->sched_two;
    
    return next_thread;   
}

int t_qlen() {
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
    printf("in create\n");
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
    cont->stack = (unsigned long *)stack_base;

    unsigned long *stack_top = cont->stack + (stack_size / sizeof(unsigned long));

    stack_top[-1] = (unsigned long)lwp_wrap; /* return address, calls actual function */
    stack_top[-2] = 0; /* fake base pointer */

    /* setting new thread */
    cont->state.rbp = (unsigned long)&stack_top[-2]; /* any values, will be teared down anyways */
    cont->state.rsp = (unsigned long)&stack_top[-3];
    cont->state.rdi = (unsigned long)function;
    cont->state.rsi = (unsigned long)argument;
    cont->status = LWP_LIVE;
    cont->state.fxsave = FPU_INIT;
    cont->tid = thread_id;
    thread_id = thread_id + 1;
    /* Need to set anything else? */

    /* admit thread to scheduler */
    current_sched->admit(cont);

    return cont->tid;
}

void lwp_start() {
    /* Calling thread -> LWP */
    printf("in start\n");
    thread cont = malloc(sizeof(context));
    cont->tid = thread_id;
    thread_id = thread_id + 1;
    cont->stacksize = 0; /* Using the one created by the OS for main() */
    cont->stack = NULL; /* Using the one created by the OS for main() */

    /* Admit thread to scheduler */
    current_sched->admit(cont);
    current_thread = cont;

    /* Call lwp_yield() */
    lwp_yield();
}

void lwp_yield() {
    /* Get next thread */
    printf("in yield\n");
    thread old_thread = current_thread;
    thread next_thread = current_sched->next();

    if(next_thread == NULL) {
        exit(3);
    }

    printf("switching from tid %lu to tid %lu\n", old_thread->tid, next_thread->tid);
    current_thread = next_thread;

    /* Swap */
    swap_rfiles(&old_thread->state, &next_thread->state);

    printf("end yield\n");
}

/* NOTE: use sched_one & sched_two
   OR exited thread pointer???? */
void lwp_exit(int exitval) {
    /* Set termination status */
    current_thread->status = MKTERMSTAT(LWP_TERM, exitval);

    /* Remove from scheduler and add to terminated queue */
    current_sched->t_remove(current_thread);
    if(terminated_head == NULL) {
        terminated_head = current_thread;
        terminated_tail = current_tail;
    } else {
        /* Currently have it as sched_one and sched_two but maybe change it? */
        terminated_tail->sched_two = current_thread;
        terminated_tail = current_thread;
    }

    /* Wake oldest in waiting queue (head) */
    if(waiting_head != NULL) {
        thread waited = waiting_head;
        waiting_head = waiting_head->sched_two;

        waited->exited = current_thread;
        current_sched->admit(waited);
    }

    printf("lwp_exit called with %d\n", exitval);

    /* Yield */
    lwp_yield();
}

/* NOTE: use lib_one & lib_two? */
tid_t lwp_wait(int *status) {
    /* Waits for a thread to terminate, deallocates its resources (UNMAP!!!),
       and reports its termination status if status is non-NULL.
       Returns the tid of the terminated thread or NO THREAD */
    
    /* Case 1: Zombie thread */
    /* Get oldest in terminated queue */
    
    /* Deallocate resources in stack */

    /* Send status */

    /* Return tid*/


    /* NOT TOO SURE ABOUT THIS
       Case 2: Threads still running */
    /* Caller waiting -> move to waiting queue */

    /* Yield to another program */

    /* Wait here */

    /* Caller wakes up, 
       get oldest thread in termination queue,
       deallocate resources */

    /* Send status */

    /* Return dead thread tid */


    /* Case 3: None of the above */
    if(qlen() < 1) {
        return NO_THREAD;
    }

    return;
}

tid_t lwp_gettid() {
    /* Returns the tid of the calling LWP or NO THREAD if not called by a LWP */
    if(current_thread == NULL) {
        return NO_THREAD;
    }
    return current_thread->tid;
}

thread tid2thread(tid_t tid) {
    /* Returns the thread corresponding to the given thread ID,
       or NULL if the ID is invalid */
    
    /* idk fix after checking if wait and term are cycles */

    thread found = NULL;
    thread current_wait = waiting;
    thread current_term = terminated;

    /* Keep here if current_thread not in any of the lists */
    if(current_thread->tid == tid) {
        return current_thread;
    }

    /* Checking scheduler */
    if(head != NULL) {
        current = head;

        do {
            if(current->tid == tid) {
                return current;
            }
            current = current->sched_two;
        } while (current != head);
    }

    /* Checking waiting queue */
    while(current_wait != NULL) {
        if(current_wait->tid == tid) {
            return current_wait;
        }
        current_wait = current_wait->sched_two;
    }

    /* Checking termination queue */
    while(current_term != NULL) {
        if (current_term->tid == tid) {
            return current_term;
        }
        current_term = current_term->sched_two;
    }

    return NULL;
}

void lwp_set_scheduler(scheduler sched) {
    /* Causes the LWP package to use the given scheduler to choose the
       next process to run. Transfers all threads from the old scheduler
       to the new one in next() order. If scheduler is NULL the library
       should return to round-robin scheduling */
}

scheduler lwp_get_scheduler() {
    /* Returns the pointer to the current scheduler */
    return current_sched;
}