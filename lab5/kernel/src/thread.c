#include "thread.h"
#include "memory.h"

static int pid_cnt = 0;
thread *run_queue;
thread thread_list[MAXPID + 1];

void init_thread() {
    // TODO: lock
    // init 'thread_list' & 'run_queue'
    // run_queue =
    run_queue = run_queue->next = run_queue;
    run_queue->prev = run_queue;
    // TODO: unlock
}

thread *thread_create(void *func) {
    // TODO: lock
    thread *the_thread;
    if (pid_cnt > MAXPID)
        return 0;
    // check the pid in thread_list
    // if ()
    // init property of 'the_thread'
    // add it into run_queue
    // TODO: unlock
}