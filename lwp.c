#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include "lwp.h"
#include "fp.h"

#define STACKSIZE 8388608

/* This file contains all code for our implemented user level LWP package */

/* What pointers within context represent: 
   lib_one: waiting and terminated queues 
   lib_two: list of all threads 
   sched_one: previous thread in current scheduler 
   sched_two: next thread in current scheduler 
   exited: dead thread to be deallocated (used by caller in lwp_wait()) */

static thread head = NULL; /* Head of current scheduler */
static thread current_thread = NULL; /* Current thread of current scheduler */
static thread all_threads = NULL; /* Head of list of all threads */
static thread terminated_head = NULL; /* Head of terminated queue */
static thread terminated_tail = NULL; /* Tail of terminated queue */
static thread waiting_head = NULL; /* Head of waiting queue */
static thread waiting_tail = NULL; /* Tail of waiting queue */

static long thread_id = 1; /* Keeps track of next available thread id */

/* Admits new thread to scheduler */
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

/* Removes thread from scheduler */
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
}

/* Moves head of scheduler to next thread */
thread next() {
    if(head == NULL) {
        return NULL;
    }

    thread result = head;
    head = head->sched_two;
    return result;   
}

/* Returns number of threads in scheduler */
int qlen() {
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

/* Create current scheduler */
struct scheduler curr_sched = {NULL, NULL, &admit, &remove, &next, &qlen};
static scheduler current_sched = &curr_sched;

/* Call the given lwp function with the given argument. */
static void lwp_wrap(lwpfun fun, void *arg) {
    int rval;
    rval=fun(arg);
    lwp_exit(rval);
}

/* Creates a new LWP that executes the given function with the given argument */
tid_t lwp_create(lwpfun function, void *argument) {
    /* Create the thread context and null check */
    thread cont = malloc(sizeof(context));
    if(cont == NULL) {
        return NO_THREAD;
    }

    /* Create the stack */
    struct rlimit rl;
    getrlimit(RLIMIT_STACK, &rl);
    long stack_size = rl.rlim_cur;
    if(stack_size == RLIM_INFINITY) {
        stack_size = STACKSIZE;
    }

    /* Set stack base */
    void *stack_base = mmap(NULL, stack_size, PROT_READ|PROT_WRITE,
                            MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK, -1, 0);

    if(stack_base==MAP_FAILED) {
        return NO_THREAD;
    }

    /* Create stack with fake values */
    cont->stacksize = stack_size;
    cont->stack = (unsigned long *)stack_base;

    unsigned long *stack_top = cont->stack + 
        (stack_size / sizeof(unsigned long));

    /* Round stack size to be divisible by 16 */
    stack_top = (unsigned long *)(((unsigned long) stack_top + 15) & ~15);
    
    stack_top[-3] = 0; /* Fake saved rbp for swap_rfiles */
    stack_top[-2] = (unsigned long)lwp_wrap; /* Ret address for swap_rfiles */
    stack_top[-1] = 0; /* Fake return address for lwp_wrap */
    

    /* Setting values in thread struct */
    cont->state.rbp = (unsigned long)&stack_top[-3];
    cont->state.rsp = (unsigned long)&stack_top[-3];
    cont->state.rdi = (unsigned long)function;
    cont->state.rsi = (unsigned long)argument;
    cont->status = LWP_LIVE;
    cont->state.fxsave = FPU_INIT;
    cont->tid = thread_id;
    cont->lib_one = NULL;
    cont->lib_two = all_threads; /* Add new thread to list of all threads */
    all_threads = cont; /* Reset all_threads to point to head of list */
    cont->sched_one = NULL;
    cont->sched_two = NULL;
    cont->exited = NULL;
    thread_id = thread_id + 1;

    /* Admit thread to scheduler */
    current_sched->admit(cont);

    return cont->tid;
}

/* Starts LWP system by converting calling thread into LWP and yielding */
void lwp_start() {
    /* Create context struct for calling thread */
    thread cont = malloc(sizeof(context));
    
    /* Set values in context struct */
    cont->tid = thread_id;
    thread_id = thread_id + 1;
    cont->stacksize = 0; /* Using the one created by the OS for main() */
    cont->stack = NULL; /* Using the one created by the OS for main() */
    cont->status = LWP_LIVE;
    cont->state.fxsave = FPU_INIT;
    cont->lib_one = NULL;
    cont->lib_two = all_threads; /* Add new thread to list of all threads */
    all_threads = cont; /* Reset all_threads to point to head of list */
    cont->exited = NULL;
    cont->sched_one = NULL;
    cont->sched_two = NULL;

    /* Admit thread to scheduler */
    current_sched->admit(cont);
    current_thread = cont;

    /* Yield to another thread in the system */
    lwp_yield();
}

/* Yields control to another LWP: saves current context, picks next thread,
   and restores chosen thread's context the current LWP’s context */
void lwp_yield() {
    thread old_thread = current_thread;
    thread next_thread = current_sched->next();

    /* Exit with the status of the original thread if no more threads*/
    if(next_thread == NULL) {
       exit(LWPTERMSTAT(old_thread->status));
    }

    current_thread = next_thread;
    
    /* Context switch between original thread and new thread to be run */
    swap_rfiles(&old_thread->state, &current_thread->state);

}

/* Terminates the current LWP & yields to thread chosen by scheduler */
void lwp_exit(int exitval) {
    /* Set termination status of exiting (current) thread */
    current_thread->status = MKTERMSTAT(LWP_TERM, exitval);
    
    /* Remove current thread from the scheduler */
    current_sched->remove(current_thread);

    
    if(waiting_head != NULL) {
        /* Wake oldest in waiting queue (head of queue) */
        thread waited = waiting_head;
        waiting_head = waiting_head->lib_one; /* Set head to next in queue*/

        /* Store thread that exited & needs deallocation (in exited pointer) */
        waited->exited = current_thread; 

        /* Admit waited back into scheduler (woken up again)*/
        current_sched->admit(waited); 

    } else {
        /* Add exited thread to terminated queue if no waiting threads */
        if(terminated_head == NULL) {
            terminated_head = current_thread;
            terminated_tail = current_thread;
        } else {
            terminated_tail->lib_one = current_thread;
            terminated_tail = current_thread;
        }
        current_thread->lib_one = NULL;
    }

    /* Yield to another thread */
    lwp_yield();
}

/* Waits for a thread to terminate, deallocates its resources, and reports
   termination status if status is non-NULL. */
tid_t lwp_wait(int *status) {
    /* Case 1: Nothing in scheduler (besides main thread) */
    if(terminated_head == NULL && current_sched->qlen() <= 1) {
        return NO_THREAD;
    }
    
    if(terminated_head != NULL) {
        /* Case 2: Zombie thread waiting in terminated queue */

        /* Get oldest in terminated queue (head) & updated head */
        thread dealloc = terminated_head;
        terminated_head = terminated_head->lib_one;
        tid_t dealloc_tid = dealloc->tid;

        /* Deallocate resources in stack */
        if(dealloc->stack != NULL) {
            munmap(dealloc->stack, dealloc->stacksize);
        }

        /* Report status */
        if(status != NULL) {
            *status = dealloc->status;
        }
        
        /* Return tid of dead thread */
        return dealloc_tid;

    } else {
        /* Case 3: No dead threads in terminated queue */

        /* Caller removed from scheduler */
        current_sched->remove(current_thread);

        /* Caller added to waiting queue */
        if(waiting_head == NULL) {
            waiting_head = current_thread;
            waiting_tail = current_thread;
        } else {
            waiting_tail->lib_one = current_thread;
            waiting_tail = waiting_tail->lib_one;
        }

        /* Yield to another thread */
        lwp_yield();

        /* Wait here for a thread to be added to terminated queue */

        /* Get dead thread to be deallocated 
           (stored in exited pointer of thread popped from waiting queue */
        thread dealloc = current_thread->exited;
        tid_t dealloc_tid = dealloc->tid;

        /* Deallocate resources in stack */
        if(dealloc->stack != NULL) {
            munmap(dealloc->stack, dealloc->stacksize);
        }

        /* Report status */
        if(status != NULL) {
            *status = dealloc->status;
        }

        /* Return tid of dead thread */
        return dealloc_tid;
    }

}

/* Returns the tid of the current thread or NO THREAD if not called by a LWP */
tid_t lwp_gettid() {
    if(current_thread == NULL) {
        return NO_THREAD;
    }
    return current_thread->tid;
}

/* Returns thread corresponding to given thread ID or NULL for invalid ID */
thread tid2thread(tid_t tid) {
    if(tid == NO_THREAD) {
        return NULL;
    }

    /* Keep here if current_thread not in any of the lists */
    if(current_thread->tid == tid) {
        return current_thread;
    }

    /* Look through all values in linked list of all threads
       (starts with all_threads) until thread is found */
    thread current = all_threads;
    while(current != NULL) {
        if(current->tid == tid) {
            return current;
        }
        current = current->lib_two;
    }

    /* If tid isn't found, return NULL */
    return NULL;
}

/* Changes scheduler and transfers all threads to new scheduler */
void lwp_set_scheduler(scheduler sched) {
    /* Keep current scheduler if given is NULL */
    if(sched == NULL) {
        return;
    }
    
    /* Keep current scheduler if switching to the same one */
    if(sched == current_sched) {
        return;
    }
    
    /* Initialize scheduler if init() exists */
    if(sched->init != NULL) {
        sched->init();
    }
    
    scheduler old = current_sched;
    thread t;

    /* Transfer all threads from old to new scheduler in next() order */
    while ((t = old->next()) != NULL) {
        old->remove(t); /* Removes thread from old scheduler */
        sched->admit(t); /* Admits thread to new scheduler */
    }

    /* Set new schedule to be the current scheduler */
    current_sched = sched;

    /* Shutdown old scheduler if shutdown() exists */
    if(old->shutdown != NULL) {
        old->shutdown();
    }

}

/* Returns pointer to current scheduler */
scheduler lwp_get_scheduler() {
    return current_sched;
}
 