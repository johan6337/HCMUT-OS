/*
* Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
*/

/* Sierra release
* Source Code License Grant: The authors hereby grant to Licensee
* personal permission to use and modify the Licensed Source Code
* for the sole purpose of studying while attending the course CO2018->
*/

#include "common.h"
#include "syscall.h"
#include "stdio.h"
#include "libmem.h"
#include "queue.h"
#include "sched.h"
#include <queue.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// Helper function to get the basename of a path
const char* get_proc_name(const char *path) {
    const char *proc_name = path;
    for(const char *p = path; *p != '\0'; p++) {
        if (*p == '/') {
            proc_name = p + 1;
        }
    }

    return proc_name;
}


int __sys_killall(struct pcb_t *caller, struct sc_regs* regs)
{
    printf("===== EXECUTING SYS_KILLALL =====\n");

    char proc_name[100];
    uint32_t data;
    uint32_t memrg = regs->a1;
    
    // Get name of the target proc
    int i = 0;
    data = 0;
    while(data != -1){
        libread(caller, memrg, i, &data);
        proc_name[i]= data;
        if(data == -1) proc_name[i]='\0';
        i++;
    }
    proc_name[i] = '\0';
    printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);

    pthread_mutex_lock(&lock);
    int terminated_count = 0; // Count of terminated processes
    int list_visited[1000] = {-1}; // Track already processed PIDs in running list
    int queue_visited[1000] = {-1}; // Track already processed PIDs in queue

    if (caller->running_list) {
        struct queue_t *running = caller->running_list;
    
        if (running->size > 0) {
            for (i = running->size - 1; i >= 0; i--) {
                struct pcb_t *proc = running->proc[i];

                // Validate process
                if (proc == NULL || proc->pid >= 1000 || list_visited[(int)proc->pid] == 1) continue;

                // printf("[CHECKING IN RUNNING LIST] Process with PID: %d - Name: \"%s\"\n", proc->pid, get_proc_name(proc->path));

                // Now check for name match
                if (strcmp(get_proc_name(proc->path), proc_name) == 0) {
                    // Mark as visited
                    list_visited[proc->pid] = 1;
                    
                    // Remove from queue
                    int j;
                    for (j = i; j < running->size - 1; j++) {
                        running->proc[j] = running->proc[j + 1];
                    }
                    running->size--;
                    terminated_count++;

                    printf("[TERMINATED FROM RUNNING LIST] Process with PID: %d - Name: \"%s\"\n", proc->pid, get_proc_name(proc->path));
                    // free(proc);
                }

                // printf("List size: %d\n", running->size);
            }
        }
    }

#ifdef MLQ_SCHED
    if(caller->mlq_ready_queue) {
        for(int prio = 0; prio < MAX_PRIO; prio++) {
            struct queue_t *mlq_queue = &(caller->mlq_ready_queue[prio]);
            
            for(int m = mlq_queue->size - 1; m >= 0; m--) {
                struct pcb_t *proc = mlq_queue->proc[m];

                if (!proc || queue_visited[(int)proc->pid] == 1) continue;

                // printf("[CHECKING IN MLQ_READY_QUEUE] Process with PID: %d - Name: \"%s\"\n", proc->pid, get_proc_name(proc->path));
                                
                if(strcmp(get_proc_name(proc->path), proc_name) == 0) {                    
                    // Mark as terminated
                    proc->is_terminated = 1;
                    queue_visited[proc->pid] = 1;
                    terminated_count++;
                    mlq_queue->size--;
                    
                    printf("[TERMINATED FROM MLQ_READY_QUEUE] Process with PID: %d - Name: \"%s\"\n", proc->pid, get_proc_name(proc->path));
                    free(proc);
                }

                // printf("Queue size: %d\n", mlq_queue->size);
            }
        }
    }

#else
    // For non-MLQ scheduler
    if(caller->ready_queue) {
        for(int m = caller->ready_queue->size - 1; m >= 0; m--) {
            struct pcb_t *proc = caller->ready_queue->proc[m];

            if (!proc || queue_visited[proc->pid]) continue;

            printf("[CHECKING IN READY_QUEUE] Process with PID: %d - Name: \"%s\"\n", proc->pid, get_proc_name(proc->path));
            
            if(strcmp(get_proc_name(proc->path), proc_name) == 0) {
                proc->is_terminated = 1;
                queue_visited[(int)proc->pid] = 1;
                terminated_count++;
                caller->ready_queue->size--;

                printf("[TERMINATED FROM READY_QUEUE] Process with PID: %d - Name: \"%s\"\n", proc->pid, get_proc_name(proc->path));
                free(proc);
            }

            printf("Queue size: %d\n", mlq_queue->size);
        }
    }
#endif
    
    pthread_mutex_unlock(&lock);
    printf("Terminated %d processes with name: \"%s\"\n", terminated_count, proc_name);
    
    return terminated_count; 
}
