#include "lwp.h"

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
        return NULL;
    }

    thread old_end = head->sched_one;
    head->sched_one = new;
    new->sched_one = old_end:
    new->sched_two = head;
    old_end->sched_two = new;
}

void remove(thread victim) {
    thread current = head;

    while(current != head) {
        if(current == victim) {
            left = victim->sched_one;
            right = victim->sched_two;
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

typedef struct scheduler {
    void init();
    void shutdown();
    void admit(thread new);
    void remove(thread victim);
    thread next();
    int qlen();
} Scheduler;


/* LWP */
tid_t lwp_create(lwpfun function, void *argument) {
    /* */
    return;
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

schedule lwp_get_scheduler() {
    /* */
    return;
}