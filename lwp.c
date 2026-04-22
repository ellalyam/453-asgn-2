#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include "lwp.h"
#include "fp.h"

/* THINK THESE FUNCTIONS ARE MISSING STUFF.
   DOUBLE CHECK INSTRUCTIONS */
/* CHECK STACK */
/* CHECK SCHEDULER IS BEING PROPERLY UPDATED */

/* SCHEDULER */
/* sched_one is PREVIOUS, sched_two is NEXT */
static thread head = NULL;
static thread current_thread = NULL;

static thread terminated_head = NULL;
static thread terminated_tail = NULL;
static thread waiting_head = NULL;
static thread waiting_tail = NULL;

void init() {
    /* initialize variables */
}

void shutdown() {
    /* don't need? */
}

void admit(thread new) {
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

void remove(thread victim) {
    if(victim->sched_one == victim) {
        head = NULL;
        victim->sched_one = NULL;
        victim->sched_two = NULL;
        return;
    } else {
        if(victim == head) {
            head = victim->sched_two;
        }
        thread left = victim->sched_one;
        thread right = victim->sched_two;
        left->sched_two = right;
        right->sched_one = left;
    }
    
    victim->sched_one = NULL;
    victim->sched_two = NULL;
    /* add error for if reaching end of list without finding */
}

thread next() {
    if(head == NULL) {
        return NULL;
    }

    thread result = head;
    head = head->sched_two;
    return result;   
}

int qlen() {
    /* count var
    count as traversing
    return count*/

    int count = 1;
    thread current = head->sched_two;

    if(current == NULL) {
        return 0;
    }

    while(current != head) {
        count = count + 1;
        current = current->sched_two;
    }

    return count;
}

struct scheduler curr_sched = {NULL, NULL, &admit, &remove, &next, &qlen};
static scheduler current_sched = &curr_sched;

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
    if(cont == NULL) {
        return NO_THREAD;
    }

    /* Create the stack */
    struct rlimit rl;
    getrlimit(RLIMIT_STACK, &rl);
    long stack_size = rl.rlim_cur;
    if(stack_size == RLIM_INFINITY) {
        stack_size = 8388608; /* MAGIC NUMBER */
    }

    void *stack_base = mmap(NULL, stack_size, PROT_READ|PROT_WRITE,
                            MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK, -1, 0);
    //printf("base address: %p\n", (void *)stack_base);

    if(stack_base==MAP_FAILED) {
        /* Deal with error */
        return NO_THREAD;
    }

    /* Fake stack frame */
    cont->stacksize = stack_size;
    cont->stack = (unsigned long *)stack_base;

    unsigned long *stack_top = cont->stack + 
        (stack_size / sizeof(unsigned long));

    //printf("before align: %p, mod 16 = %lu\n", 
    //(void*)stack_top, (unsigned long)stack_top % 16);

    stack_top = (unsigned long *)((unsigned long)stack_top & ~0xFUL);
    //printf("top address: %p\n", (void *)stack_top);
    //printf("after align:  %p, mod 16 = %lu\n", 
    //(void*)stack_top, (unsigned long)stack_top % 16);

    stack_top[-3] = 0; /* fake saved rbp for swap_rfiles */
    stack_top[-2] = (unsigned long)lwp_wrap; /* ret target from swap_rfiles */
    stack_top[-1] = 0; /* fake return address for lwp_wrap */
    

    /* setting new thread */
    cont->state.rbp = (unsigned long)&stack_top[-3];
    cont->state.rsp = (unsigned long)&stack_top[-3];
    cont->state.rdi = (unsigned long)function;
    cont->state.rsi = (unsigned long)argument;
    cont->status = LWP_LIVE;
    cont->state.fxsave = FPU_INIT;
    cont->tid = thread_id;
    cont->lib_one = NULL;
    cont->lib_two = NULL;
    cont->sched_one = NULL;
    cont->sched_two = NULL;
    cont->exited = NULL;
    thread_id = thread_id + 1;
    /* Need to set anything else? */


    /* admit thread to scheduler */
    current_sched->admit(cont);

    return cont->tid;
}

void lwp_start() {
    /* Calling thread -> LWP */
    //printf("START \n");
    thread cont = malloc(sizeof(context));
    
    cont->tid = thread_id;
    thread_id = thread_id + 1;
    cont->stacksize = 0; /* Using the one created by the OS for main() */
    cont->stack = NULL; /* Using the one created by the OS for main() */
    cont->status = LWP_LIVE;
    cont->state.fxsave = FPU_INIT;
    cont->lib_one = NULL;
    cont->lib_two = NULL;
    cont->exited = NULL;
    cont->sched_one = NULL;
    cont->sched_two = NULL;

    /* Admit thread to scheduler */
    current_sched->admit(cont);
    current_thread = cont;

    /* Call lwp_yield() */
    lwp_yield();
}

void lwp_yield() {
    /* Get next thread */
    thread old_thread = current_thread;
    thread next_thread = current_sched->next();

    if(next_thread == NULL) {
       exit(LWPTERMSTAT(old_thread->status));
    }

    current_thread = next_thread;
    
    /* Swap */
    swap_rfiles(&old_thread->state, &current_thread->state);

}

/* NOTE: use sched_one & sched_two OR exited thread pointer???? */
void lwp_exit(int exitval) {
    /* Set termination status */
    current_thread->status = MKTERMSTAT(LWP_TERM, exitval);
    
    current_sched->remove(current_thread);

    /* Wake oldest in waiting queue (head) */
    if(waiting_head != NULL) {
        thread waited = waiting_head;
        waiting_head = waiting_head->lib_one;

        waited->exited = current_thread;
        current_sched->admit(waited);
    } else {
        /* Remove from scheduler and add to terminated queue */
        if(terminated_head == NULL) {
            terminated_head = current_thread;
            terminated_tail = current_thread;
        } else {
            /* Currently have it as sched_one and sched_two, maybe change? */
            terminated_tail->lib_one = current_thread;
            terminated_tail = current_thread;
        }
        current_thread->lib_one = NULL;
    }

    /* Yield */
    lwp_yield();
}

/* NOTE: use lib_one = terminated & lib_two = waiting */
tid_t lwp_wait(int *status) {
    /* Waits for a thread to terminate, deallocates its resources (UNMAP!!!),
       and reports its termination status if status is non-NULL.
       Returns the tid of the terminated thread or NO THREAD */

    /* Case 1: Nothing in scheduler*/
    if(terminated_head == NULL && current_sched->qlen() <= 1) {
        return NO_THREAD;
    }
    
    /* Case 2: Zombie thread */
    if(terminated_head != NULL) {
        /* Get oldest in terminated queue */
        thread dealloc = terminated_head;
        terminated_head = terminated_head->lib_one;
        tid_t dealloc_tid = dealloc->tid;

        /* Deallocate resources in stack */
        if(dealloc->stack != NULL) {
            munmap(dealloc->stack, dealloc->stacksize);
        }

        /* Send status */
        if(status != NULL) {
            *status = dealloc->status;
        }
        
        /* Return tid*/
        return dealloc_tid;

    } else {
        /* Case 3: Threads still running */
        /* Caller waiting -> move to waiting queue */
        
        current_sched->remove(current_thread);

        if(waiting_head == NULL) {
            waiting_head = current_thread;
            waiting_tail = current_thread;
        } else {
            waiting_tail->lib_two = current_thread;
            waiting_tail = waiting_tail->lib_two;
        }

        lwp_yield();

        /* Wait here */

        /* Get oldest in terminated queue */
        thread dealloc = current_thread->exited;
        /*terminated_head = terminated_head->lib_one;*/
        tid_t dealloc_tid = dealloc->tid;

        /* Deallocate resources in stack */
        if(dealloc->stack != NULL) {
            munmap(dealloc->stack, dealloc->stacksize);
        }

        /* Send status */
        if(status != NULL) {
            *status = dealloc->status;
        }

        /* Return tid*/
        return dealloc_tid;

    }

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
    
    if(tid == NO_THREAD) {
        return NULL;
    }
    /* idk fix after checking if wait and term are cycles */

    thread current_wait = waiting_head;
    thread current_term = terminated_head;

    /* Keep here if current_thread not in any of the lists */
    if(current_thread->tid == tid) {
        return current_thread;
    }

    /* Checking scheduler */
    if(head != NULL) {
        thread current = head;

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

    scheduler old = current_sched;
    thread t;

    
    if (sched == NULL) { /* can use init?? */
        sched = &current_sched;
    }

    while ((t = old->next()) != NULL) {
        old->remove(t);
        sched->admit(t);
    }

    current_sched = sched;
}

scheduler lwp_get_scheduler() {
    /* Returns the pointer to the current scheduler */
    return current_sched;
}
