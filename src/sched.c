
#include "queue.h"
#include "sched.h"
#include <pthread.h>
 
#include <stdlib.h>
#include <stdio.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

static struct queue_t running_list;
#ifdef MLQ_SCHED
// static struct queue_t mlq_ready_queue[MAX_PRIO];
// static int slot[MAX_PRIO];

// Remove static to allow access from other files
struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
#endif



int queue_empty(void) {
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if(!empty(&mlq_ready_queue[prio])) 
			return -1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
	int i ;
	for (i = 0; i < MAX_PRIO; i ++) {
		mlq_ready_queue[i].size = 0;
		slot[i] = MAX_PRIO - i; 
	}
#endif

	ready_queue.size = 0;
	run_queue.size = 0;
	pthread_mutex_init(&queue_lock, NULL);
}

#ifdef MLQ_SCHED
/* 
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
struct pcb_t * get_mlq_proc(void) {
    struct pcb_t * proc = NULL;

    // Lock the queue to ensure thread safety
    pthread_mutex_lock(&queue_lock);

    unsigned long prio;
    int first_non_empty_queue = -1;
    int process_found = 0;

    // Iterate through all priority levels
    for (prio = 0; prio < MAX_PRIO; prio++) {
        if (!empty(&mlq_ready_queue[prio])) {
            // Mark the first non-empty queue
            if (first_non_empty_queue == -1) {
                first_non_empty_queue = prio;
            }

            // Check if the current priority level has available slots
            if (slot[prio] > 0) {
                proc = dequeue(&mlq_ready_queue[prio]);
                if (proc != NULL) {
                    process_found = 1;
                    slot[prio]--;
                    break;
                }
            }
        }
    }

    // If no process is found in any queue
    if (first_non_empty_queue == -1) {
        pthread_mutex_unlock(&queue_lock);
        return NULL;
    } else {
        // If no process could run due to exhausted slots
        if (process_found == 0) {
            // Reset all slots to their initial values
            for (prio = 0; prio < MAX_PRIO; prio++) {
                slot[prio] = MAX_PRIO - prio;
            }

            // Dequeue a process from the first non-empty queue
            proc = dequeue(&mlq_ready_queue[first_non_empty_queue]);
            if (proc != NULL) {
                slot[prio]--;
            }
        }
    }

    // Unlock the queue and return the selected process
    pthread_mutex_unlock(&queue_lock);
    return proc;
}

void put_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);	
}

struct pcb_t * get_proc(void) {
	return get_mlq_proc();
}

void put_proc(struct pcb_t * proc) {  
	// /* TODO: put running proc to running_list */
	// pthread_mutex_lock(&queue_lock);
    // enqueue(&running_list, proc);
	// pthread_mutex_unlock(&queue_lock);	

	put_mlq_proc(proc);
}

void add_proc(struct pcb_t * proc) {
	// /* TODO: put running proc to running_list */
	// enqueue(&running_list, proc);

	add_mlq_proc(proc);
}

#else

struct pcb_t * get_proc(void) {
	struct pcb_t * proc = NULL;
	/*TODO: get a process from [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
	return proc;
}

void put_proc(struct pcb_t * proc) {
	proc->ready_queue = &ready_queue;
	proc->running_list = & running_list;

	/* TODO: put running proc to running_list */

	pthread_mutex_lock(&queue_lock);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
	proc->ready_queue = &ready_queue;
	proc->running_list = & running_list;

	/* TODO: put running proc to running_list */

	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);	
}

#endif


